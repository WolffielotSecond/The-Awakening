#include "Core/TATimeSubsystem.h"
#include "Core/TAFreezeSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogTATime, Log, All);

namespace
{
	// 单帧内单个循环计时器最多补触发的次数，防止极小间隔在长帧里卡死
	constexpr int32 MaxCatchUpFiresPerTimer = 16;
}

UTATimeSubsystem* UTATimeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTATimeSubsystem>() : nullptr;
}

// -------------------- 冻结 --------------------

void UTATimeSubsystem::RequestFreeze(UObject* Source, float Strength)
{
	if (UTAFreezeSubsystem* Freeze = GetWorld()->GetSubsystem<UTAFreezeSubsystem>())
	{
		Freeze->RequestFreeze(Source, Strength);
	}
}

void UTATimeSubsystem::ReleaseFreeze(UObject* Source)
{
	if (UTAFreezeSubsystem* Freeze = GetWorld()->GetSubsystem<UTAFreezeSubsystem>())
	{
		Freeze->ReleaseFreeze(Source);
	}
}

float UTATimeSubsystem::GetFreezeStrength() const
{
	const UTAFreezeSubsystem* Freeze = GetWorld()->GetSubsystem<UTAFreezeSubsystem>();
	return Freeze ? Freeze->GetFreezeStrength() : 0.f;
}

// -------------------- 全局暂停 --------------------

void UTATimeSubsystem::RequestPause(UObject* Source)
{
	if (!IsValid(Source))
	{
		return;
	}
	PauseSources.Add(Source);
	ApplyPauseState();
}

void UTATimeSubsystem::ReleasePause(UObject* Source)
{
	PauseSources.Remove(Source);
	ApplyPauseState();
}

bool UTATimeSubsystem::HasPauseRequest() const
{
	for (const auto& Source : PauseSources)
	{
		if (Source.IsValid()) return true;
	}
	return false;
}

void UTATimeSubsystem::PrunePauseSources()
{
	for (auto It = PauseSources.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}
}

void UTATimeSubsystem::ApplyPauseState()
{
	PrunePauseSources();
	const bool bWantPause = PauseSources.Num() > 0;
	if (bWantPause == bAppliedPause)
	{
		return;
	}
	// 只在第一个来源加入、最后一个来源离开时切换引擎暂停
	if (!UGameplayStatics::SetGamePaused(GetWorld(), bWantPause))
	{
		UE_LOG(LogTATime, Warning, TEXT("SetGamePaused(%d) was rejected by the game mode."), bWantPause ? 1 : 0);
	}
	bAppliedPause = bWantPause;
}

// -------------------- 游戏时间 --------------------

float UTATimeSubsystem::GetGameplayTimeScale() const
{
	const UWorld* World = GetWorld();
	if (!World || World->IsPaused())
	{
		return 0.f;
	}
	const AWorldSettings* Settings = World->GetWorldSettings();
	const float WorldScale = Settings ? Settings->GetEffectiveTimeDilation() : 1.f;
	return WorldScale * (1.f - FMath::Clamp(GetFreezeStrength(), 0.f, 1.f));
}

FTAGameplayTimerHandle UTATimeSubsystem::SetGameplayTimer(FTimerDelegate Delegate, float Duration, bool bLoop)
{
	FTAGameplayTimerHandle Handle;
	if (!Delegate.IsBound())
	{
		return Handle;
	}
	if (bLoop && Duration <= 0.f)
	{
		UE_LOG(LogTATime, Warning, TEXT("Looping gameplay timer needs a positive duration; timer not created."));
		return Handle;
	}
	FTimerEntry& Entry = Timers.AddDefaulted_GetRef();
	Entry.Id = ++NextTimerId;
	Entry.FireTime = GameplayTime + FMath::Max(Duration, 0.f);
	Entry.Interval = Duration;
	Entry.bLoop = bLoop;
	Entry.Delegate = MoveTemp(Delegate);
	Handle.Id = Entry.Id;
	return Handle;
}

FTAGameplayTimerHandle UTATimeSubsystem::SetGameplayTimerByEvent(FTAGameplayTimerDynamicDelegate Event, float Duration, bool bLoop)
{
	if (!Event.IsBound())
	{
		return FTAGameplayTimerHandle();
	}
	FTimerDelegate Delegate;
	Delegate.BindUFunction(Event.GetUObject(), Event.GetFunctionName());
	return SetGameplayTimer(MoveTemp(Delegate), Duration, bLoop);
}

void UTATimeSubsystem::ClearGameplayTimer(FTAGameplayTimerHandle& Handle)
{
	if (Handle.IsValid())
	{
		const uint64 Id = Handle.Id;
		Timers.RemoveAll([Id](const FTimerEntry& Entry) { return Entry.Id == Id; });
	}
	Handle.Invalidate();
}

bool UTATimeSubsystem::IsGameplayTimerActive(FTAGameplayTimerHandle Handle) const
{
	return Handle.IsValid() && Timers.ContainsByPredicate([Id = Handle.Id](const FTimerEntry& Entry) { return Entry.Id == Id; });
}

float UTATimeSubsystem::GetGameplayTimerRemaining(FTAGameplayTimerHandle Handle) const
{
	const FTimerEntry* Entry = Handle.IsValid()
		? Timers.FindByPredicate([Id = Handle.Id](const FTimerEntry& E) { return E.Id == Id; })
		: nullptr;
	return Entry ? static_cast<float>(FMath::Max(0.0, Entry->FireTime - GameplayTime)) : -1.f;
}

void UTATimeSubsystem::AdvanceGameplayTime(float WorldDeltaSeconds)
{
	const float Scale = 1.f - FMath::Clamp(GetFreezeStrength(), 0.f, 1.f);
	GameplayTime += FMath::Max(0.f, WorldDeltaSeconds) * Scale;

	// 按到期先后逐个触发；回调里可以安全地新增 / 清除计时器（每次都重新查找）
	TMap<uint64, int32> FireCounts;
	while (true)
	{
		int32 DueIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Timers.Num(); ++Index)
		{
			const FTimerEntry& Entry = Timers[Index];
			if (Entry.FireTime <= GameplayTime && FireCounts.FindRef(Entry.Id) < MaxCatchUpFiresPerTimer &&
				(DueIndex == INDEX_NONE || Entry.FireTime < Timers[DueIndex].FireTime))
			{
				DueIndex = Index;
			}
		}
		if (DueIndex == INDEX_NONE)
		{
			break;
		}

		FTimerEntry& Due = Timers[DueIndex];
		const FTimerDelegate Delegate = Due.Delegate;
		++FireCounts.FindOrAdd(Due.Id);
		if (Due.bLoop)
		{
			Due.FireTime += Due.Interval;
		}
		else
		{
			Timers.RemoveAt(DueIndex);
		}
		if (!Delegate.ExecuteIfBound())
		{
			// 绑定对象已销毁：把所有已失效的计时器（包括循环的）一并移除
			Timers.RemoveAll([](const FTimerEntry& Entry) { return !Entry.Delegate.IsBound(); });
		}
	}
}

void UTATimeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->IsPaused())
	{
		// 暂停期间只清理已销毁的来源（例如暂停菜单被意外销毁），游戏时间不走
		ApplyPauseState();
		return;
	}
	AdvanceGameplayTime(DeltaTime);
}

TStatId UTATimeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTATimeSubsystem, STATGROUP_Tickables);
}

void UTATimeSubsystem::Deinitialize()
{
	Timers.Reset();
	PauseSources.Reset();
	Super::Deinitialize();
}
