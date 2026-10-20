#include "Document/TACharacterActionComponent.h"
#include "Core/TAGameplayTags.h"
#include "Health/TAHealthComponent.h"
#include "The_AwakeningPlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogTACharacterAction, Log, All);

UTACharacterActionComponent::UTACharacterActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 文档规则：拿起 / 放下文本期间禁止跑酷并无敌；塞入日记本期间只禁跑酷和移动
	FTACharacterActionConfig& Pickup = ActionConfigs.AddDefaulted_GetRef();
	Pickup.Tag = TAGameplayTags::Anim_Pickup;
	Pickup.bInvulnerable = true;
	Pickup.FallbackKeyframes = { TEXT("HideWorldMesh") };

	FTACharacterActionConfig& PutDown = ActionConfigs.AddDefaulted_GetRef();
	PutDown.Tag = TAGameplayTags::Anim_PutDown;
	PutDown.bInvulnerable = true;
	PutDown.FallbackKeyframes = { TEXT("ShowWorldMesh") };

	FTACharacterActionConfig& InsertPage = ActionConfigs.AddDefaulted_GetRef();
	InsertPage.Tag = TAGameplayTags::Anim_InsertPage;
	InsertPage.bBlockMove = true;
	InsertPage.bBlockInteract = false;
}

void UTACharacterActionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UTAHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UTAHealthComponent>() : nullptr)
	{
		Health->OnDeathStarted.AddDynamic(this, &UTACharacterActionComponent::HandleOwnerDeathOrRespawn);
		Health->OnRespawned.AddDynamic(this, &UTACharacterActionComponent::HandleOwnerDeathOrRespawn);
	}
}

void UTACharacterActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTAHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UTAHealthComponent>() : nullptr)
	{
		Health->OnDeathStarted.RemoveDynamic(this, &UTACharacterActionComponent::HandleOwnerDeathOrRespawn);
		Health->OnRespawned.RemoveDynamic(this, &UTACharacterActionComponent::HandleOwnerDeathOrRespawn);
	}
	InterruptAction();
	Super::EndPlay(EndPlayReason);
}

void UTACharacterActionComponent::HandleOwnerDeathOrRespawn()
{
	InterruptAction();
}

const FTACharacterActionConfig& UTACharacterActionComponent::FindConfig(FGameplayTag ActionTag) const
{
	const FTACharacterActionConfig* Found = ActionConfigs.FindByPredicate(
		[&ActionTag](const FTACharacterActionConfig& Config) { return Config.Tag == ActionTag; });
	return Found ? *Found : DefaultConfig;
}

void UTACharacterActionComponent::PlayAction(FGameplayTag ActionTag, FOnFinished OnFinished)
{
	InterruptAction();
	if (!ActionTag.IsValid())
	{
		UE_LOG(LogTACharacterAction, Warning, TEXT("PlayAction: invalid action tag."));
		if (OnFinished)
		{
			OnFinished(true);
		}
		return;
	}

	const FTACharacterActionConfig& Config = FindConfig(ActionTag);
	ActiveTag = ActionTag;
	ActiveOnFinished = MoveTemp(OnFinished);
	bAccepted = false;
	AcquireRestrictions(Config);

	// 动画蓝图在广播中同步认领；广播里也可能打断或开始了别的动作
	OnActionRequested.Broadcast(ActionTag);
	if (ActiveTag != ActionTag || bAccepted)
	{
		return;
	}

	for (const FName& Key : Config.FallbackKeyframes)
	{
		UE_LOG(LogTACharacterAction, Log, TEXT("Action '%s' keyframe '%s' (fallback)."), *ActionTag.ToString(), *Key.ToString());
		OnActionKeyframe.Broadcast(ActionTag, Key);
		if (ActiveTag != ActionTag)
		{
			return;
		}
	}

	UTATimeSubsystem* Time = UTATimeSubsystem::Get(this);
	if (Config.FallbackDuration <= 0.f || !Time)
	{
		Finish(false);
		return;
	}
	TimerHandle = Time->SetGameplayTimer(FTimerDelegate::CreateUObject(this, &UTACharacterActionComponent::HandleTimerElapsed), Config.FallbackDuration);
}

void UTACharacterActionComponent::AcceptAction(FGameplayTag ActionTag)
{
	if (ActionTag != ActiveTag || bAccepted)
	{
		return;
	}
	bAccepted = true;

	// 从回退计时切换为保底计时
	if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
	{
		Time->ClearGameplayTimer(TimerHandle);
		TimerHandle = Time->SetGameplayTimer(FTimerDelegate::CreateUObject(this, &UTACharacterActionComponent::HandleTimerElapsed), FindConfig(ActionTag).MaxDuration);
	}
}

void UTACharacterActionComponent::NotifyActionKeyframe(FGameplayTag ActionTag, FName KeyName)
{
	if (ActionTag == ActiveTag)
	{
		UE_LOG(LogTACharacterAction, Log, TEXT("Action '%s' keyframe '%s'."), *ActionTag.ToString(), *KeyName.ToString());
		OnActionKeyframe.Broadcast(ActionTag, KeyName);
	}
}

void UTACharacterActionComponent::NotifyActionFinished(FGameplayTag ActionTag)
{
	if (ActionTag == ActiveTag)
	{
		Finish(false);
	}
}

void UTACharacterActionComponent::InterruptAction()
{
	Finish(true);
}

void UTACharacterActionComponent::HandleTimerElapsed()
{
	TimerHandle.Invalidate();
	if (bAccepted)
	{
		UE_LOG(LogTACharacterAction, Warning, TEXT("Action '%s' was accepted but NotifyActionFinished never came; finished after MaxDuration."),
			*ActiveTag.ToString());
	}
	Finish(false);
}

void UTACharacterActionComponent::Finish(bool bInterrupted)
{
	if (!ActiveTag.IsValid())
	{
		return;
	}
	if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
	{
		Time->ClearGameplayTimer(TimerHandle);
	}
	TimerHandle.Invalidate();
	ReleaseRestrictions();

	// 先清状态再回调：回调里可以立刻开始下一个动作
	const FGameplayTag FinishedTag = ActiveTag;
	FOnFinished Callback = MoveTemp(ActiveOnFinished);
	ActiveTag = FGameplayTag();
	ActiveOnFinished = nullptr;
	bAccepted = false;

	if (Callback)
	{
		Callback(bInterrupted);
	}
	OnActionFinished.Broadcast(FinishedTag, bInterrupted);
}

void UTACharacterActionComponent::AcquireRestrictions(const FTACharacterActionConfig& Config)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Config.bBlockMove || Config.bBlockParkour || Config.bBlockInteract)
	{
		if (AThe_AwakeningPlayerController* PC = Pawn ? Cast<AThe_AwakeningPlayerController>(Pawn->GetController()) : nullptr)
		{
			// 从默认玩法权限里去掉被禁止的能力
			FTAInputRequest Request;
			Request.Owner = this;
			Request.Priority = ActionInputPriority;
			Request.Allowed = FTAInputRouter().GetWinner().Request.Allowed;
			if (Config.bBlockMove) Request.Allowed.Remove(ETAInputCapability::Move);
			if (Config.bBlockParkour) Request.Allowed.Remove(ETAInputCapability::Parkour);
			if (Config.bBlockInteract) Request.Allowed.Remove(ETAInputCapability::Interact);
			InputRequestController = PC;
			InputRequestHandle = PC->AcquireInputRequest(Request);
		}
	}
	if (Config.bInvulnerable)
	{
		if (UTAHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UTAHealthComponent>() : nullptr)
		{
			InvulnerableHandle = Health->AddProtection(ETAHealthProtection::Invulnerable, this, FGameplayTagContainer());
		}
	}
}

void UTACharacterActionComponent::ReleaseRestrictions()
{
	const FTAInputRouter::FHandle Handle = InputRequestHandle;
	InputRequestHandle = 0;
	if (AThe_AwakeningPlayerController* PC = InputRequestController.Get(); PC && Handle)
	{
		PC->ReleaseInputRequest(Handle);
	}
	InputRequestController.Reset();

	if (!InvulnerableHandle.IsValid())
	{
		return;
	}
	if (UTAHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UTAHealthComponent>() : nullptr)
	{
		Health->RemoveProtection(InvulnerableHandle);
	}
	InvulnerableHandle = FTAHealthProtectionHandle();
}
