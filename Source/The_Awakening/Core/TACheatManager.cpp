#include "Core/TACheatManager.h"
#include "Core/TAGameplayTags.h"
#include "Health/TADamageLibrary.h"
#include "Health/TAHealthComponent.h"
#include "Health/TARespawnSubsystem.h"
#include "Health/TACheckpoint.h"
#include "Health/TARollbackSubsystem.h"
#include "Document/TACharacterActionComponent.h"
#include "Document/TAPreviewStage.h"
#include "Document/TADocumentSettings.h"
#include "Document/TADocumentSubsystem.h"
#include "Save/TASaveSubsystem.h"
#include "Save/TASaveGame.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogTACheat, Log, All);

UAbilitySystemComponent* UTACheatManager::GetPlayerASC() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	return PC ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
}

UTAHealthComponent* UTACheatManager::GetPlayerHealth() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	return PC ? UTAHealthComponent::FindHealthComponent(PC->GetPawn()) : nullptr;
}

void UTACheatManager::ApplyConsoleDamage(float Amount, ETADamageType Type, const TCHAR* Command)
{
	APlayerController* PC = GetOuterAPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UAbilitySystemComponent* ASC = GetPlayerASC();
	if (!Pawn || !ASC)
	{
		UE_LOG(LogTACheat, Warning, TEXT("%s: no controlled pawn with an AbilitySystemComponent."), Command);
		return;
	}
	const float Before = ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute());
	const bool bApplied = UTADamageLibrary::ApplyDamage(Pawn, Pawn, Amount, Type, TAGameplayTags::Damage_Source_Console);
	const float After = ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute());
	UE_LOG(LogTACheat, Log, TEXT("%s %.2f: applied=%d Health %.2f -> %.2f"), Command, Amount, bApplied ? 1 : 0, Before, After);
}

void UTACheatManager::TADamage(float Amount)
{
	ApplyConsoleDamage(Amount, ETADamageType::Normal, TEXT("TADamage"));
}

void UTACheatManager::TATrueDamage(float Amount)
{
	ApplyConsoleDamage(Amount, ETADamageType::TrueDamage, TEXT("TATrueDamage"));
}

void UTACheatManager::TAKill()
{
	const UAbilitySystemComponent* ASC = GetPlayerASC();
	const float Health = ASC ? ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute()) : 0.f;
	ApplyConsoleDamage(Health, ETADamageType::Forced, TEXT("TAKill"));
}

void UTACheatManager::TAHeal(float Amount)
{
	UAbilitySystemComponent* ASC = GetPlayerASC();
	const UTAHealthComponent* Health = GetPlayerHealth();
	if (!ASC || !Health)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAHeal: no controlled pawn with a health component."));
		return;
	}
	if (Health->IsDead())
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAHeal: player is dead; healing does not revive. Wait for respawn."));
		return;
	}
	const float Before = ASC->GetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute());
	const float Max = ASC->GetNumericAttribute(UTAAttributeSet::GetMaxHealthAttribute());
	const float After = Amount > 0.f ? FMath::Min(Max, Before + Amount) : Max;
	// 对话 / 小游戏进行中：和其他改动一样记入回滚日志
	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(GetOuterAPlayerController()); Rollback && Rollback->IsRecording())
	{
		Rollback->RecordUndo(TEXT("Cheat.Heal"), [WeakASC = TWeakObjectPtr<UAbilitySystemComponent>(ASC), Before]
		{
			if (UAbilitySystemComponent* Target = WeakASC.Get()) Target->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), Before);
		});
	}
	ASC->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), After);
	UE_LOG(LogTACheat, Log, TEXT("TAHeal %.2f: Health %.2f -> %.2f"), Amount, Before, ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute()));
}

void UTACheatManager::TACheckpoint(FName CheckpointId)
{
	UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(GetOuterAPlayerController());
	ATACheckpoint* Checkpoint = Respawn ? Respawn->FindCheckpoint(CheckpointId) : nullptr;
	if (!Checkpoint)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TACheckpoint: no checkpoint with id '%s'."), *CheckpointId.ToString());
		return;
	}
	Respawn->ActivateCheckpoint(Checkpoint);
	UE_LOG(LogTACheat, Log, TEXT("TACheckpoint: active checkpoint is now %s (%s)."), *CheckpointId.ToString(), *Checkpoint->GetName());
}

void UTACheatManager::TAGod()
{
	if (GodProtection.IsValid())
	{
		if (UTAHealthComponent* OldHealth = GodHealth.Get())
		{
			OldHealth->RemoveProtection(GodProtection);
		}
		GodProtection = FTAHealthProtectionHandle();
		GodHealth.Reset();
		UE_LOG(LogTACheat, Log, TEXT("TAGod: off"));
		return;
	}

	UTAHealthComponent* Health = GetPlayerHealth();
	if (!Health)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAGod: no controlled pawn with a health component."));
		return;
	}
	GodProtection = Health->AddProtection(ETAHealthProtection::Invulnerable, this, FGameplayTagContainer());
	GodHealth = Health;
	UE_LOG(LogTACheat, Log, TEXT("TAGod: on"));
}

void UTACheatManager::TAAction(const FString& ActionTag)
{
	const APlayerController* PC = GetOuterAPlayerController();
	UTACharacterActionComponent* Action = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UTACharacterActionComponent>() : nullptr;
	if (!Action)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAAction: no controlled pawn with a character action component."));
		return;
	}
	if (ActionTag.IsEmpty())
	{
		Action->InterruptAction();
		UE_LOG(LogTACheat, Log, TEXT("TAAction: interrupted current action."));
		return;
	}
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*ActionTag), false);
	if (!Tag.IsValid())
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAAction: unknown tag '%s'."), *ActionTag);
		return;
	}
	UE_LOG(LogTACheat, Log, TEXT("TAAction %s: started."), *Tag.ToString());
	Action->PlayAction(Tag, [Tag](bool bInterrupted)
	{
		UE_LOG(LogTACheat, Log, TEXT("TAAction %s: %s."), *Tag.ToString(), bInterrupted ? TEXT("interrupted") : TEXT("finished"));
	});
	UE_LOG(LogTACheat, Log, TEXT("TAAction %s: %s."), *Tag.ToString(),
		Action->IsActionAccepted() ? TEXT("accepted by animation") : TEXT("no animation, using fallback"));
}

void UTACheatManager::BeginDestroy()
{
	// GC 期间不销毁 Actor：预览台随关卡一起清理，这里只移除视口上的画面
	CloseDebugPreview(false);
	Super::BeginDestroy();
}

void UTACheatManager::CloseDebugPreview(bool bDestroyStage)
{
	if (UGameViewportClient* Viewport = DebugPreviewViewport.Get(); Viewport && DebugPreviewSlate.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(DebugPreviewSlate.ToSharedRef());
	}
	if (ATAPreviewStage* Stage = DebugPreviewStage.Get(); Stage && bDestroyStage)
	{
		Stage->Destroy();
	}
	DebugPreviewSlate.Reset();
	DebugPreviewViewport.Reset();
	DebugPreviewStage.Reset();
	DebugPreviewBrush.SetResourceObject(nullptr);
}

void UTACheatManager::TAPreview(const FString& MeshPath)
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		return;
	}
	if (MeshPath.Equals(TEXT("off"), ESearchCase::IgnoreCase))
	{
		CloseDebugPreview(true);
		UE_LOG(LogTACheat, Log, TEXT("TAPreview: closed."));
		return;
	}

	// 调试用：固定占用第 9 个预览台槽位，不影响正式界面使用的槽位
	ATAPreviewStage* Stage = DebugPreviewStage.Get();
	if (!Stage)
	{
		Stage = ATAPreviewStage::SpawnPreviewStage(PC, nullptr, 9);
		DebugPreviewStage = Stage;
	}
	if (!Stage)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAPreview: failed to spawn a preview stage."));
		return;
	}
	// 静态网格体和骨骼网格体都可以；类型由预览台判断
	UObject* Mesh = MeshPath.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *MeshPath);
	if (!MeshPath.IsEmpty() && !Mesh)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAPreview: asset '%s' not found, showing placeholder."), *MeshPath);
	}
	Stage->SetDisplayModel(Mesh, FTransform::Identity);
	Stage->SetCaptureEnabled(true);

	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
	if (!DebugPreviewSlate.IsValid() && Viewport)
	{
		const FVector2D Size(Stage->RenderTargetSize);
		// 透明背景要经显示材质还原透明度；材质缺失或不透明模式时直接显示 RenderTarget
		UObject* Resource = Stage->GetDisplayMaterial();
		DebugPreviewBrush.SetResourceObject(Resource ? Resource : Stage->GetRenderTarget());
		DebugPreviewBrush.ImageSize = Size;
		// 左上角，不接收鼠标
		DebugPreviewSlate = SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(FMargin(20.f))
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SBox)
				.WidthOverride(Size.X)
				.HeightOverride(Size.Y)
				[
					SNew(SImage).Image(&DebugPreviewBrush)
				]
			];
		Viewport->AddViewportWidgetContent(DebugPreviewSlate.ToSharedRef(), 5000);
		DebugPreviewViewport = Viewport;
	}
	UE_LOG(LogTACheat, Log, TEXT("TAPreview: showing %s."), Stage->IsShowingPlaceholder() ? TEXT("placeholder") : *Mesh->GetPathName());
}

void UTACheatManager::TAGiveDiary()
{
	const APlayerController* PC = GetOuterAPlayerController();
	UTAInventoryComponent* Inventory = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UTAInventoryComponent>() : nullptr;
	UTAItemDefinition* Diary = GetDefault<UTADocumentSettings>()->DiaryItem.LoadSynchronous();
	if (!Inventory || !Diary)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAGiveDiary: %s."), !Inventory ? TEXT("no inventory on the controlled pawn") : TEXT("diary item not set in Project Settings > The Awakening Documents"));
		return;
	}
	const int32 Added = Inventory->TryAddItem(Diary);
	UE_LOG(LogTACheat, Log, TEXT("TAGiveDiary: %s."), Added > 0 ? TEXT("diary added to its story slot") : TEXT("already holding the diary, or its story slot is unavailable"));
}

void UTACheatManager::TADocList()
{
	const UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(GetOuterAPlayerController());
	if (!Documents)
	{
		return;
	}
	const FTADocumentSaveData Data = Documents->GetSaveData();
	auto Join = [](const TArray<FName>& Ids)
	{
		TArray<FString> Names;
		for (const FName& Id : Ids) Names.Add(Id.ToString());
		return Names.IsEmpty() ? FString(TEXT("(none)")) : FString::Join(Names, TEXT(", "));
	};
	const FString Message = FString::Printf(TEXT("Collected (page order): %s | Read: %s | Diary page: %d"),
		*Join(Data.CollectedDocuments), *Join(Data.ReadDocuments), Data.DiaryPageIndex);
	UE_LOG(LogTACheat, Log, TEXT("TADocList: %s"), *Message);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Yellow, Message);
}

void UTACheatManager::TADocReset()
{
	if (UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(GetOuterAPlayerController()))
	{
		Documents->RestoreSaveData(FTADocumentSaveData());
		UE_LOG(LogTACheat, Log, TEXT("TADocReset: document records cleared."));
	}
}

void UTACheatManager::TASave()
{
	UTASaveSubsystem* Save = UTASaveSubsystem::Get(GetOuterAPlayerController());
	const bool bOk = Save && Save->SaveGame();
	UE_LOG(LogTACheat, Log, TEXT("TASave: %s."), bOk ? TEXT("saved") : TEXT("failed (see LogTASave)"));
}

void UTACheatManager::TALoad()
{
	UTASaveSubsystem* Save = UTASaveSubsystem::Get(GetOuterAPlayerController());
	const bool bOk = Save && Save->LoadGame();
	UE_LOG(LogTACheat, Log, TEXT("TALoad: %s."), bOk ? TEXT("loading") : TEXT("failed (see LogTASave)"));
}

void UTACheatManager::TASaveDelete()
{
	UTASaveSubsystem* Save = UTASaveSubsystem::Get(GetOuterAPlayerController());
	UE_LOG(LogTACheat, Log, TEXT("TASaveDelete: %s."), Save && Save->DeleteSaveGame() ? TEXT("deleted") : TEXT("no save"));
}

void UTACheatManager::TASaveInfo()
{
	const UTASaveSubsystem* Save = UTASaveSubsystem::Get(GetOuterAPlayerController());
	const UTASaveGame* Data = Save ? Save->PeekSaveGame() : nullptr;
	const FString Message = Data
		? FString::Printf(TEXT("Save: format %d, %s, %s, level %s, checkpoint %s, documents %d, money %d"),
			Data->SaveFormatVersion, *Data->GameVersion, *Data->SaveTime.ToString(), *Data->LevelName,
			*Data->CheckpointId.ToString(), Data->Documents.CollectedDocuments.Num(), Data->Money)
		: FString(TEXT("Save: (none)"));
	UE_LOG(LogTACheat, Log, TEXT("TASaveInfo: %s"), *Message);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Yellow, Message);
}
