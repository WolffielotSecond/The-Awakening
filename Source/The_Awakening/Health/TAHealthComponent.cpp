#include "Health/TAHealthComponent.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "Core/TAGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Health/TARespawnSubsystem.h"
#include "Health/TARollbackSubsystem.h"
#include "UI/TACommonDialog.h"
#include "Engine/LocalPlayer.h"
#include "Movement/TAParkourComponent.h"
#include "Scan/TAScanningComponent.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogTAHealth, Log, All);

UTAHealthComponent::UTAHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

UTAHealthComponent* UTAHealthComponent::FindHealthComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UTAHealthComponent>() : nullptr;
}

void UTAHealthComponent::InitializeWithASC(UAbilitySystemComponent* InASC)
{
	if (InASC == AbilitySystemComponent.Get())
	{
		return;
	}
	UninitializeFromASC();
	if (!InASC)
	{
		return;
	}
	if (!InASC->GetSet<UTAAttributeSet>())
	{
		UE_LOG(LogTAHealth, Warning, TEXT("%s: ASC %s has no UTAAttributeSet; health component stays unbound."),
			*GetNameSafe(GetOwner()), *GetNameSafe(InASC));
		return;
	}

	AbilitySystemComponent = InASC;
	HealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UTAAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UTAHealthComponent::HandleHealthChanged);
	MaxHealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UTAAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UTAHealthComponent::HandleMaxHealthChanged);

	// 换绑后把已有的保护 Tag 补到新 ASC 上
	PruneAndSyncProtectionTags();

	// 绑定时同步一次，HUD 不必区分先后创建顺序
	OnHealthChanged.Broadcast(GetHealth(), GetMaxHealth(), 0.f);
}

void UTAHealthComponent::UninitializeFromASC()
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UTAAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UTAAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		// 只撤掉自己加在这个 ASC 上的保护 Tag
		for (; AppliedInvulnerableTags > 0; --AppliedInvulnerableTags) ASC->RemoveLooseGameplayTag(TAGameplayTags::State_Invulnerable);
		for (; AppliedUndyingTags > 0; --AppliedUndyingTags) ASC->RemoveLooseGameplayTag(TAGameplayTags::State_Undying);
	}
	AppliedInvulnerableTags = 0;
	AppliedUndyingTags = 0;
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	AbilitySystemComponent.Reset();
}

void UTAHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FModifierEntry& Entry : Modifiers)
	{
		ClearModifierTimer(Entry);
	}
	Modifiers.Reset();
	UninitializeFromASC();
	Super::EndPlay(EndPlayReason);
}

bool UTAHealthComponent::IsDead() const
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	return ASC && ASC->HasMatchingGameplayTag(TAGameplayTags::State_Dead);
}

float UTAHealthComponent::GetHealth() const
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	return ASC ? ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute()) : 0.f;
}

float UTAHealthComponent::GetMaxHealth() const
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	return ASC ? ASC->GetNumericAttribute(UTAAttributeSet::GetMaxHealthAttribute()) : 0.f;
}

float UTAHealthComponent::GetHealthNormalized() const
{
	const float Max = GetMaxHealth();
	return Max > 0.f ? FMath::Clamp(GetHealth() / Max, 0.f, 1.f) : 0.f;
}

void UTAHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	OnHealthChanged.Broadcast(ChangeData.NewValue, GetMaxHealth(), ChangeData.NewValue - ChangeData.OldValue);

	// 所有扣血路径（GE、ApplyModToAttribute 直接改 Health）都经过这里，死亡统一在此判定
	if (ChangeData.OldValue > 0.f && ChangeData.NewValue <= 0.f && !IsDead())
	{
		HandleDeath();
	}
}

void UTAHealthComponent::HandleDeath()
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC || IsDead() || ShouldInterceptDeath())
	{
		return;
	}

	// 顺序很重要：先挂死亡状态并拿住输入，再关 UI，最后改角色状态；否则关菜单那一帧输入权会短暂回到玩家手里。
	// 1. 死亡状态。之后的伤害（包括同一帧的第二次致命伤害）都会被 AttributeSet 忽略。
	ASC->AddLooseGameplayTag(TAGameplayTags::State_Dead);
	bAppliedDeathTag = true;

	AThe_AwakeningCharacter* Character = Cast<AThe_AwakeningCharacter>(GetOwner());
	AThe_AwakeningPlayerController* PC = Character ? Cast<AThe_AwakeningPlayerController>(Character->GetController()) : nullptr;
	UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(this);

	// 2. 死亡输入请求（优先级 900，不允许任何动作）。句柄交给复活子系统保管。
	const FTAInputRouter::FHandle DeathInput = (PC && Respawn) ? Respawn->AcquireDeathInput(PC) : 0;

	// 3. 强制关闭所有页面。
	//    对话和小游戏：先按逆序撤销它们记录的全部改动，再由各自的中止动作关闭界面，回到“还没开始”。
	//    （对话 / 小游戏期间有无敌 + 不会死，只有强制伤害能走到这里。）
	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this))
	{
		Rollback->RollbackAll();
	}
	if (PC)
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UTACommonDialogSubsystem* Dialogs = LocalPlayer->GetSubsystem<UTACommonDialogSubsystem>())
			{
				if (UTACommonDialogWidget* Dialog = Dialogs->GetActiveDialog()) Dialog->Dismiss();
			}
		}
		if (UTAScanningComponent* Scan = PC->FindComponentByClass<UTAScanningComponent>())
		{
			// 正在扫描时，第 2 步的输入请求已让扫描以 UIInterrupted 自行取消；这里关掉扫描权限，复活前不能再开。
			// 冻结请求由扫描在淡出结束时自己释放，不绕过它直接清冻结表。
			bScanEnabledBeforeDeath = Scan->IsScanEnabled();
			ScanDisabledOnDeath = Scan;
			Scan->SetScanEnabled(false);
		}
		// 防御性处理：正常情况下世界暂停时不会受伤
		PC->CloseSettingsMenu(true);
		PC->ClosePauseMenu();
	}
	if (Character)
	{
		Character->CloseInventoryIfOpen();

		// 4. 角色：停住移动；空中跑酷立刻中止，靠重力落地。
		// 交互提示和拾取列表按 AllowsInput(Interact) 显示，死亡请求生效后自动隐藏。
		// 不清物理按住缓存（InputRouting 约定：权限拒绝不清 Held），不禁用 CharacterMovement。
		Character->StopCurrentMovement();
		if (UTAParkourComponent* Parkour = Character->GetParkourComponent())
		{
			Parkour->AbortParkour();
		}
	}

	// 5. 广播并进入复活流程
	OnDeathStarted.Broadcast();
	if (PC && Respawn)
	{
		if (!Respawn->BeginRespawn(PC, DeathInput))
		{
			// 子系统没有接管句柄时由自己释放，避免输入锁残留
			Respawn->ReleaseDeathInput(PC, DeathInput);
		}
	}
}

// -------------------- 伤害修改器 --------------------

FTADamageModifierHandle UTAHealthComponent::AddDamageModifier(const FTADamageModifier& Modifier, UObject* Owner)
{
	FTADamageModifierHandle Handle;
	if (!IsValid(Owner))
	{
		UE_LOG(LogTAHealth, Warning, TEXT("%s: AddDamageModifier needs a valid owner."), *GetNameSafe(GetOwner()));
		return Handle;
	}
	FModifierEntry& Entry = Modifiers.AddDefaulted_GetRef();
	Entry.Id = ++NextEntryId;
	Entry.Modifier = Modifier;
	Entry.Owner = Owner;
	Handle.Id = Entry.Id;
	if (Modifier.IsTemporary())
	{
		// 临时修改器按游戏时间计时：扫描减速时变慢，小游戏冻结和暂停时停止
		if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
		{
			const uint64 Id = Entry.Id;
			Entry.ExpiryTimer = Time->SetGameplayTimer(FTimerDelegate::CreateWeakLambda(this, [this, Id] { ExpireDamageModifier(Id); }),
				Modifier.Duration);
		}
	}
	return Handle;
}

bool UTAHealthComponent::RemoveDamageModifier(FTADamageModifierHandle& Handle)
{
	const uint64 Id = Handle.Id;
	Handle = FTADamageModifierHandle();
	const int32 Index = Id ? Modifiers.IndexOfByPredicate([Id](const FModifierEntry& Entry) { return Entry.Id == Id; }) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return false;
	}
	ClearModifierTimer(Modifiers[Index]);
	Modifiers.RemoveAt(Index);
	return true;
}

void UTAHealthComponent::ExpireDamageModifier(uint64 Id)
{
	const int32 Index = Modifiers.IndexOfByPredicate([Id](const FModifierEntry& Entry) { return Entry.Id == Id; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	Modifiers.RemoveAt(Index);
	FTADamageModifierHandle Expired;
	Expired.Id = Id;
	OnDamageModifierExpired.Broadcast(Expired);
}

void UTAHealthComponent::ClearModifierTimer(FModifierEntry& Entry)
{
	if (Entry.ExpiryTimer.IsValid())
	{
		if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
		{
			Time->ClearGameplayTimer(Entry.ExpiryTimer);
		}
		Entry.ExpiryTimer = FTAGameplayTimerHandle();
	}
}

void UTAHealthComponent::ClearTemporaryDamageModifiers()
{
	for (int32 Index = Modifiers.Num() - 1; Index >= 0; --Index)
	{
		if (Modifiers[Index].Modifier.IsTemporary())
		{
			ClearModifierTimer(Modifiers[Index]);
			Modifiers.RemoveAt(Index);
		}
	}
}

TArray<FTADamageModifier> UTAHealthComponent::GetActiveDamageModifiers() const
{
	TArray<FTADamageModifier> Result;
	Result.Reserve(Modifiers.Num());
	for (const FModifierEntry& Entry : Modifiers)
	{
		if (Entry.IsActive())
		{
			Result.Add(Entry.Modifier);
		}
	}
	return Result;
}

// -------------------- 保护 --------------------

FTAHealthProtectionHandle UTAHealthComponent::AddProtection(ETAHealthProtection Kind, UObject* Owner, FGameplayTagContainer AllowedDamageSources)
{
	FTAHealthProtectionHandle Handle;
	if (!IsValid(Owner))
	{
		UE_LOG(LogTAHealth, Warning, TEXT("%s: AddProtection needs a valid owner."), *GetNameSafe(GetOwner()));
		return Handle;
	}
	FProtectionEntry& Entry = Protections.AddDefaulted_GetRef();
	Entry.Id = ++NextEntryId;
	Entry.Kind = Kind;
	Entry.Owner = Owner;
	Entry.AllowedSources = MoveTemp(AllowedDamageSources);
	Handle.Id = Entry.Id;
	PruneAndSyncProtectionTags();
	return Handle;
}

bool UTAHealthComponent::RemoveProtection(FTAHealthProtectionHandle& Handle)
{
	const uint64 Id = Handle.Id;
	Handle = FTAHealthProtectionHandle();
	const int32 Removed = Id ? Protections.RemoveAll([Id](const FProtectionEntry& Entry) { return Entry.Id == Id; }) : 0;
	PruneAndSyncProtectionTags();
	return Removed > 0;
}

int32 UTAHealthComponent::CountActiveProtections(ETAHealthProtection Kind) const
{
	int32 Count = 0;
	for (const FProtectionEntry& Entry : Protections)
	{
		if (Entry.Kind == Kind && Entry.IsActive()) ++Count;
	}
	return Count;
}

bool UTAHealthComponent::HasProtection(ETAHealthProtection Kind) const
{
	return CountActiveProtections(Kind) > 0;
}

bool UTAHealthComponent::IsDamageBlocked(const FGameplayTag& Source) const
{
	for (const FProtectionEntry& Entry : Protections)
	{
		if (Entry.Kind == ETAHealthProtection::Invulnerable && Entry.IsActive() &&
			!(Source.IsValid() && Source.MatchesAny(Entry.AllowedSources)))
		{
			return true;
		}
	}
	return false;
}

void UTAHealthComponent::PruneAndSyncProtectionTags()
{
	Protections.RemoveAll([](const FProtectionEntry& Entry) { return !Entry.IsActive(); });
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}
	auto Sync = [ASC](const FGameplayTag& Tag, int32& Applied, int32 Desired)
	{
		for (; Applied < Desired; ++Applied) ASC->AddLooseGameplayTag(Tag);
		for (; Applied > Desired; --Applied) ASC->RemoveLooseGameplayTag(Tag);
	};
	Sync(TAGameplayTags::State_Invulnerable, AppliedInvulnerableTags, CountActiveProtections(ETAHealthProtection::Invulnerable));
	Sync(TAGameplayTags::State_Undying, AppliedUndyingTags, CountActiveProtections(ETAHealthProtection::Undying));
}

void UTAHealthComponent::NotifyDamageResolved(float Amount, ETADamageType Type, FGameplayTag Source)
{
	LastDamageSource = Source;
	LastDamageType = Type;
	// 顺带清理持有者已销毁的保护，让 ASC 上的状态 Tag 不长期滞留
	PruneAndSyncProtectionTags();
	if (Amount > 0.f)
	{
		OnDamageTaken.Broadcast(Amount, Type, Source);
	}
}

void UTAHealthComponent::FinishRespawn()
{
	// 复活只清临时修改器，永久修改器保留
	ClearTemporaryDamageModifiers();

	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get(); ASC && bAppliedDeathTag)
	{
		ASC->RemoveLooseGameplayTag(TAGameplayTags::State_Dead);
	}
	bAppliedDeathTag = false;

	if (UTAScanningComponent* Scan = ScanDisabledOnDeath.Get())
	{
		// 恢复死亡前的值，不无条件打开：策划可能本来就关掉了扫描
		Scan->SetScanEnabled(bScanEnabledBeforeDeath);
	}
	ScanDisabledOnDeath.Reset();
	bScanEnabledBeforeDeath = true;

	OnRespawned.Broadcast();
}

void UTAHealthComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	// MaxHealth 变化不算受伤，Delta 固定为 0；若因此压低了 Health，会另有一次 Health 变化广播。
	OnHealthChanged.Broadcast(GetHealth(), ChangeData.NewValue, 0.f);
}
