#include "Save/TASaveSubsystem.h"
#include "Save/TASaveGame.h"
#include "Core/TAPlayerState.h"
#include "Core/TAVersionLibrary.h"
#include "Document/TADocumentSubsystem.h"
#include "Document/TAWorldDocument.h"
#include "Document/TADocumentDefinition.h"
#include "Health/TACheckpoint.h"
#include "Health/TARespawnSubsystem.h"
#include "Inventory/TAInventoryComponent.h"
#include "Story/TADialogueSubsystem.h"
#include "The_AwakeningCharacter.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogTASave, Log, All);

const FString UTASaveSubsystem::SlotName(TEXT("TA_Slot0"));

namespace
{
	AThe_AwakeningCharacter* FindPlayerCharacter(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AThe_AwakeningCharacter>(PC->GetPawn()) : nullptr;
	}
}

UTASaveSubsystem* UTASaveSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UTASaveSubsystem>() : nullptr;
}

void UTASaveSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
	Super::Deinitialize();
}

// ==================== 存 ====================

UTASaveGame* UTASaveSubsystem::BuildSaveGame(UWorld* World) const
{
	AThe_AwakeningCharacter* Character = FindPlayerCharacter(World);
	if (!Character)
	{
		return nullptr;
	}
	UTASaveGame* Save = Cast<UTASaveGame>(UGameplayStatics::CreateSaveGameObject(UTASaveGame::StaticClass()));
	Save->SaveFormatVersion = UTAVersionLibrary::GetSaveFormatVersion();
	Save->GameVersion = UTAVersionLibrary::GetFullVersion();
	Save->SaveTime = FDateTime::Now();
	Save->LevelName = UGameplayStatics::GetCurrentLevelName(World, true);
	if (const UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(World))
	{
		if (const ATACheckpoint* Checkpoint = Respawn->GetActiveCheckpoint())
		{
			Save->CheckpointId = Checkpoint->CheckpointId;
		}
	}
	if (const UTADialogueSubsystem* Dialogue = GetGameInstance()->GetSubsystem<UTADialogueSubsystem>())
	{
		Save->Story = Dialogue->GetSaveData();
	}
	if (const UTADocumentSubsystem* Documents = GetGameInstance()->GetSubsystem<UTADocumentSubsystem>())
	{
		Save->Documents = Documents->GetSaveData();
	}
	if (const UTAInventoryComponent* Inventory = Character->FindComponentByClass<UTAInventoryComponent>())
	{
		Save->Inventory = Inventory->GetSaveData();
	}
	if (const ATAPlayerState* PS = Character->GetPlayerState<ATAPlayerState>())
	{
		Save->Money = PS->GetMoney();
	}
	return Save;
}

bool UTASaveSubsystem::SaveGame()
{
	if (bRestoring || PendingLoad)
	{
		return false;
	}
	UTASaveGame* Save = BuildSaveGame(GetWorld());
	if (!Save)
	{
		UE_LOG(LogTASave, Warning, TEXT("SaveGame: no player character; nothing saved."));
		return false;
	}
	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex);
	UE_LOG(LogTASave, Log, TEXT("SaveGame %s: level=%s checkpoint=%s documents=%d version=%d"),
		bSaved ? TEXT("ok") : TEXT("FAILED"), *Save->LevelName, *Save->CheckpointId.ToString(),
		Save->Documents.CollectedDocuments.Num(), Save->SaveFormatVersion);
	return bSaved;
}

void UTASaveSubsystem::AutoSave()
{
	SaveGame();
}

bool UTASaveSubsystem::HasSaveGame() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
}

bool UTASaveSubsystem::DeleteSaveGame()
{
	return HasSaveGame() && UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
}

UTASaveGame* UTASaveSubsystem::PeekSaveGame() const
{
	return HasSaveGame() ? Cast<UTASaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)) : nullptr;
}

// ==================== 读 ====================

bool UTASaveSubsystem::LoadGame()
{
	UTASaveGame* Save = PeekSaveGame();
	if (!Save)
	{
		UE_LOG(LogTASave, Warning, TEXT("LoadGame: no save in slot '%s'."), *SlotName);
		return false;
	}
	const int32 Current = UTAVersionLibrary::GetSaveFormatVersion();
	if (Save->SaveFormatVersion > Current)
	{
		// 版本约定：更高格式的存档提示不支持，不覆盖
		UE_LOG(LogTASave, Warning, TEXT("LoadGame: save format %d is newer than this build (%d); refused."), Save->SaveFormatVersion, Current);
		return false;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FString CurrentLevel = UGameplayStatics::GetCurrentLevelName(World, true);
	if (!Save->LevelName.IsEmpty() && Save->LevelName != CurrentLevel)
	{
		// 先打开存档关卡，角色生成后再恢复
		PendingLoad = Save;
		PendingAttempts = 0;
		if (!PostLoadMapHandle.IsValid())
		{
			PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTASaveSubsystem::HandlePostLoadMap);
		}
		UE_LOG(LogTASave, Log, TEXT("LoadGame: opening level %s."), *Save->LevelName);
		UGameplayStatics::OpenLevel(World, FName(*Save->LevelName));
		return true;
	}
	return ApplySaveGame(World, Save);
}

void UTASaveSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!PendingLoad || !World)
	{
		return;
	}
	// 角色可能稍后才生成：每 0.1 秒试一次，最多 5 秒
	World->GetTimerManager().SetTimer(PendingTimer, FTimerDelegate::CreateUObject(this, &UTASaveSubsystem::TryApplyPending), 0.1f, true);
}

void UTASaveSubsystem::TryApplyPending()
{
	UWorld* World = GetWorld();
	if (!PendingLoad || !World)
	{
		return;
	}
	UTASaveGame* Save = PendingLoad;
	if (ApplySaveGame(World, Save) || ++PendingAttempts >= 50)
	{
		if (PendingAttempts >= 50)
		{
			UE_LOG(LogTASave, Warning, TEXT("LoadGame: player character never appeared in %s; save not applied."), *Save->LevelName);
		}
		PendingLoad = nullptr;
		World->GetTimerManager().ClearTimer(PendingTimer);
	}
}

bool UTASaveSubsystem::ApplySaveGame(UWorld* World, UTASaveGame* Save)
{
	AThe_AwakeningCharacter* Character = FindPlayerCharacter(World);
	if (!Character || !Save)
	{
		return false;
	}
	TGuardValue<bool> Guard(bRestoring, true);
	Character->CloseInventoryIfOpen();

	if (UTADialogueSubsystem* Dialogue = GetGameInstance()->GetSubsystem<UTADialogueSubsystem>())
	{
		Dialogue->RestoreSaveData(Save->Story);
	}
	UTADocumentSubsystem* Documents = GetGameInstance()->GetSubsystem<UTADocumentSubsystem>();
	if (Documents)
	{
		Documents->RestoreSaveData(Save->Documents);
	}
	if (UTAInventoryComponent* Inventory = Character->FindComponentByClass<UTAInventoryComponent>())
	{
		Inventory->RestoreSaveData(Save->Inventory);
	}
	if (ATAPlayerState* PS = Character->GetPlayerState<ATAPlayerState>())
	{
		const int32 Delta = Save->Money - PS->GetMoney();
		if (Delta > 0) PS->AddMoney(Delta);
		else if (Delta < 0) PS->SpendMoney(-Delta);
	}
	// 场景里已收集的文本不再出现
	if (Documents)
	{
		for (TActorIterator<ATAWorldDocument> It(World); It; ++It)
		{
			const UTADocumentDefinition* Definition = It->GetDefinition();
			if (Definition && Documents->IsCollected(Definition->DocumentId))
			{
				It->Destroy();
			}
		}
	}
	// 复活点：设为当前复活点，角色出现在它的出生位置
	if (UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(World))
	{
		if (ATACheckpoint* Checkpoint = Save->CheckpointId.IsNone() ? nullptr : Respawn->FindCheckpoint(Save->CheckpointId))
		{
			Respawn->ActivateCheckpoint(Checkpoint);
			const FTransform Spawn = Checkpoint->GetSpawnTransform();
			Character->SetActorLocationAndRotation(Spawn.GetLocation(), Spawn.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		}
		else if (!Save->CheckpointId.IsNone())
		{
			UE_LOG(LogTASave, Warning, TEXT("LoadGame: checkpoint '%s' not found in this level; player stays in place."), *Save->CheckpointId.ToString());
		}
	}
	UE_LOG(LogTASave, Log, TEXT("LoadGame applied: level=%s checkpoint=%s saved at %s (%s)."),
		*Save->LevelName, *Save->CheckpointId.ToString(), *Save->SaveTime.ToString(), *Save->GameVersion);
	return true;
}
