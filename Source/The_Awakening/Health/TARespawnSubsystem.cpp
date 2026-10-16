#include "Health/TARespawnSubsystem.h"
#include "Health/TACheckpoint.h"
#include "Health/TAHealthComponent.h"
#include "Health/TARespawnSettings.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "Core/TAGameplayTags.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"

DEFINE_LOG_CATEGORY_STATIC(LogTARespawn, Log, All);

UTARespawnSubsystem* UTARespawnSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTARespawnSubsystem>() : nullptr;
}

// -------------------- 复活点 --------------------

void UTARespawnSubsystem::RegisterCheckpoint(ATACheckpoint* Checkpoint)
{
	if (!IsValid(Checkpoint))
	{
		return;
	}
	Checkpoints.AddUnique(Checkpoint);
	if (Checkpoint->bIsLevelStart)
	{
		if (const ATACheckpoint* Active = ActiveCheckpoint.Get(); Active && Active->bIsLevelStart && Active != Checkpoint)
		{
			UE_LOG(LogTARespawn, Warning, TEXT("Multiple level-start checkpoints: %s and %s. Keep one per level."),
				*Active->GetName(), *Checkpoint->GetName());
		}
		else if (!ActiveCheckpoint.IsValid())
		{
			ActivateCheckpoint(Checkpoint);
		}
	}
}

void UTARespawnSubsystem::UnregisterCheckpoint(ATACheckpoint* Checkpoint)
{
	Checkpoints.Remove(Checkpoint);
	if (ActiveCheckpoint.Get() == Checkpoint)
	{
		ActiveCheckpoint.Reset();
		OnCheckpointChanged.Broadcast(nullptr);
	}
}

void UTARespawnSubsystem::ActivateCheckpoint(ATACheckpoint* Checkpoint)
{
	if (!IsValid(Checkpoint))
	{
		return;
	}
	ATACheckpoint* Previous = ActiveCheckpoint.Get();
	if (Previous == Checkpoint)
	{
		return;
	}
	if (Previous)
	{
		Previous->SetActiveCheckpoint(false);
	}
	ActiveCheckpoint = Checkpoint;
	Checkpoint->SetActiveCheckpoint(true);
	OnCheckpointChanged.Broadcast(Checkpoint);
}

ATACheckpoint* UTARespawnSubsystem::FindCheckpoint(FName CheckpointId) const
{
	for (const TWeakObjectPtr<ATACheckpoint>& Entry : Checkpoints)
	{
		if (ATACheckpoint* Checkpoint = Entry.Get(); Checkpoint && Checkpoint->CheckpointId == CheckpointId)
		{
			return Checkpoint;
		}
	}
	return nullptr;
}

// -------------------- 死亡输入 --------------------

FTAInputRouter::FHandle UTARespawnSubsystem::AcquireDeathInput(AThe_AwakeningPlayerController* PC)
{
	if (!PC)
	{
		return 0;
	}
	FTAInputRequest Request;
	Request.Owner = this;
	Request.Priority = DeathInputPriority;
	// Allowed 留空：Move/Interact/Scan 都被拒绝；暂停键检查 Scan 权限，所以也打不开暂停
	Request.Presentation.InputMode = ETAInputModeRequirement::GameOnly;
	Request.Presentation.bShowCursor = false;
	return PC->AcquireInputRequest(Request);
}

void UTARespawnSubsystem::ReleaseDeathInput(AThe_AwakeningPlayerController* PC, FTAInputRouter::FHandle Handle)
{
	if (PC && Handle)
	{
		PC->ReleaseInputRequest(Handle);
	}
}

// -------------------- 复活流程 --------------------

bool UTARespawnSubsystem::BeginRespawn(AThe_AwakeningPlayerController* PC, FTAInputRouter::FHandle InDeathInputHandle)
{
	if (IsRespawning())
	{
		UE_LOG(LogTARespawn, Verbose, TEXT("BeginRespawn ignored: already respawning."));
		return false;
	}
	UTATimeSubsystem* Time = UTATimeSubsystem::Get(this);
	if (!PC || !Time)
	{
		return false;
	}
	RespawningController = PC;
	DeathInputHandle = InDeathInputHandle;
	Phase = ETARespawnPhase::Dying;
	// 上一次复活的无敌还没结束又死了：先撤掉，复活完成时重新加
	if (UTAHealthComponent* Health = ProtectedHealth.Get())
	{
		Health->RemoveProtection(RespawnProtection);
	}
	Time->ClearGameplayTimer(ProtectionTimer);
	ProtectedHealth.Reset();

	const UTARespawnSettings* Settings = GetDefault<UTARespawnSettings>();
	PhaseTimer = Time->SetGameplayTimer(FTimerDelegate::CreateUObject(this, &UTARespawnSubsystem::EnterFadeOut), Settings->DeathDelay);
	return true;
}

void UTARespawnSubsystem::EnterFadeOut()
{
	PhaseTimer = FTAGameplayTimerHandle();
	AThe_AwakeningPlayerController* PC = RespawningController.Get();
	UTATimeSubsystem* Time = UTATimeSubsystem::Get(this);
	if (!PC || !Time)
	{
		CompleteRespawn();
		return;
	}
	Phase = ETARespawnPhase::FadingOut;
	const UTARespawnSettings* Settings = GetDefault<UTARespawnSettings>();
	// 以后商人拖回的演出插在这一段
	StartCameraFade(PC, 0.f, 1.f, Settings->FadeOutTime, true);
	PhaseTimer = Time->SetGameplayTimer(FTimerDelegate::CreateUObject(this, &UTARespawnSubsystem::ResetAndFadeIn), Settings->FadeOutTime);
}

void UTARespawnSubsystem::ResetAndFadeIn()
{
	PhaseTimer = FTAGameplayTimerHandle();
	AThe_AwakeningPlayerController* PC = RespawningController.Get();
	UTATimeSubsystem* Time = UTATimeSubsystem::Get(this);
	AThe_AwakeningCharacter* Character = PC ? Cast<AThe_AwakeningCharacter>(PC->GetPawn()) : nullptr;
	const UTARespawnSettings* Settings = GetDefault<UTARespawnSettings>();

	if (Character)
	{
		// 1. 传送并同步朝向；镜头偏移归零、跳过一次 Camera Lag，避免镜头从死亡点飞过来
		FTransform Spawn;
		if (ResolveRespawnTransform(PC, Spawn))
		{
			Character->StopCurrentMovement();
			Character->SetActorLocationAndRotation(Spawn.GetLocation(), Spawn.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(Spawn.Rotator());
		}
		Character->ResetCameraForRespawn();

		// 2. 回血；3. 只清临时修改器和带 ClearOnRespawn 的持续效果
		if (UTAHealthComponent* Health = Character->GetHealthComponent())
		{
			if (UAbilitySystemComponent* ASC = Health->GetAbilitySystemComponent())
			{
				const float Max = ASC->GetNumericAttribute(UTAAttributeSet::GetMaxHealthAttribute());
				ASC->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), Max * FMath::Clamp(Settings->RespawnHealthRatio, 0.01f, 1.f));
				ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(TAGameplayTags::Effect_ClearOnRespawn));
			}
			Health->ClearTemporaryDamageModifiers();
		}
	}

	// 4. 世界复位（敌人回到初始位置和状态）
	OnWorldReset.Broadcast();

	if (!PC || !Time)
	{
		CompleteRespawn();
		return;
	}
	Phase = ETARespawnPhase::FadingIn;
	StartCameraFade(PC, 1.f, 0.f, Settings->FadeInTime, false);
	PhaseTimer = Time->SetGameplayTimer(FTimerDelegate::CreateUObject(this, &UTARespawnSubsystem::CompleteRespawn), Settings->FadeInTime);
}

void UTARespawnSubsystem::CompleteRespawn()
{
	ClearPhaseTimer();
	AThe_AwakeningPlayerController* PC = RespawningController.Get();
	const FTAInputRouter::FHandle Handle = DeathInputHandle;
	RespawningController.Reset();
	DeathInputHandle = 0;
	Phase = ETARespawnPhase::None;

	if (PC && PC->PlayerCameraManager)
	{
		// 提前结束（例如测试直接调用）时也不留黑屏
		PC->PlayerCameraManager->StopCameraFade();
	}

	if (UTAHealthComponent* Health = PC ? UTAHealthComponent::FindHealthComponent(PC->GetPawn()) : nullptr)
	{
		Health->FinishRespawn();

		// 复活无敌：持有者是本子系统，按游戏时间到期移除；强制伤害不受影响
		const float InvulnerableTime = GetDefault<UTARespawnSettings>()->RespawnInvulnerableTime;
		UTATimeSubsystem* Time = UTATimeSubsystem::Get(this);
		if (InvulnerableTime > 0.f && Time)
		{
			RespawnProtection = Health->AddProtection(ETAHealthProtection::Invulnerable, this, FGameplayTagContainer());
			ProtectedHealth = Health;
			ProtectionTimer = Time->SetGameplayTimer(
				FTimerDelegate::CreateUObject(this, &UTARespawnSubsystem::HandleRespawnProtectionExpired), InvulnerableTime);
		}
	}
	// 最后交还输入：先恢复角色状态，再放开控制
	ReleaseDeathInput(PC, Handle);
}

void UTARespawnSubsystem::HandleRespawnProtectionExpired()
{
	ProtectionTimer = FTAGameplayTimerHandle();
	if (UTAHealthComponent* Health = ProtectedHealth.Get())
	{
		Health->RemoveProtection(RespawnProtection);
	}
	ProtectedHealth.Reset();
}

bool UTARespawnSubsystem::ResolveRespawnTransform(AThe_AwakeningPlayerController* PC, FTransform& OutTransform) const
{
	if (const ATACheckpoint* Checkpoint = ActiveCheckpoint.Get())
	{
		OutTransform = Checkpoint->GetSpawnTransform();
		return true;
	}
	UWorld* World = GetWorld();
	AActor* Start = nullptr;
	if (AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr)
	{
		Start = GameMode->FindPlayerStart(PC);
	}
	if (!Start && World)
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Start = *It;
			break;
		}
	}
	if (Start)
	{
		UE_LOG(LogTARespawn, Warning, TEXT("No active checkpoint; respawning at PlayerStart %s."), *Start->GetName());
		OutTransform = Start->GetActorTransform();
		return true;
	}
	UE_LOG(LogTARespawn, Warning, TEXT("No active checkpoint and no PlayerStart; respawning in place."));
	return false;
}

void UTARespawnSubsystem::StartCameraFade(AThe_AwakeningPlayerController* PC, float From, float To, float Duration, bool bHold) const
{
	if (PC && PC->PlayerCameraManager)
	{
		const FLinearColor Color = GetDefault<UTARespawnSettings>()->FadeColor;
		PC->PlayerCameraManager->StartCameraFade(From, To, FMath::Max(Duration, 0.f), Color, false, bHold);
	}
}

void UTARespawnSubsystem::ClearPhaseTimer()
{
	if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
	{
		Time->ClearGameplayTimer(PhaseTimer);
	}
	PhaseTimer = FTAGameplayTimerHandle();
}

void UTARespawnSubsystem::Deinitialize()
{
	ClearPhaseTimer();
	ReleaseDeathInput(RespawningController.Get(), DeathInputHandle);
	RespawningController.Reset();
	DeathInputHandle = 0;
	Phase = ETARespawnPhase::None;
	if (UTAHealthComponent* Health = ProtectedHealth.Get())
	{
		Health->RemoveProtection(RespawnProtection);
	}
	ProtectedHealth.Reset();
	Super::Deinitialize();
}
