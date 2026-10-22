#include "TADiaryItemCommandlet.h"
#include "Inventory/TAItemDefinition.h"
#include "Inventory/TAWorldItem.h"
#include "Document/TAPreviewStage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "AssetCompilingManager.h"
#include "MeshUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "RenderingThread.h"

UTADiaryItemCommandlet::UTADiaryItemCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UTADiaryItemCommandlet::Main(const FString& Params)
{
	const bool bVerify = FParse::Param(*Params, TEXT("Verify"));
	auto* Item = LoadObject<UTAItemDefinition>(nullptr, TEXT("/Game/Document/DA_Item_Diary.DA_Item_Diary"));
	auto* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Document/BP_WorldItem_Diary.BP_WorldItem_Diary"));
	auto* Body = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Models/Diary/SK_Journal_Body.SK_Journal_Body"));
	auto* Open = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Models/Diary/Book_Open.Book_Open"));
	if (!Item || !Blueprint || !Blueprint->GeneratedClass || !Body || !Open) return 1;
	auto* Closed = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Models/Diary/SM_Diary_Closed.SM_Diary_Closed"));
	if (bVerify && !Closed) return 2;
	TGuardValue<bool> ClientRendering(GIsClient, true);
	TStrongObjectPtr<UGameInstance> Game(NewObject<UGameInstance>(GEngine));
	Game->InitializeStandalone();
	UWorld* World = Game->GetWorld();
	ON_SCOPE_EXIT
	{
		Game->Shutdown();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	World->InitializeActorsForPlay(FURL());
	if (!Closed)
	{
		auto* Actor = World->SpawnActor<AActor>();
		auto* Component = NewObject<USkeletalMeshComponent>(Actor);
		Actor->SetRootComponent(Component);
		Component->SetSkeletalMesh(Body);
		Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Component->RegisterComponent();
		Component->SetAnimation(Open);
		Component->SetPosition(0.f, false);
		Component->TickAnimation(0.f, false);
		Component->RefreshBoneTransforms();
		Component->UpdateBounds();
		FAssetCompilingManager::Get().FinishAllCompilation();
		FlushRenderingCommands();
		// Keep the original UVs/materials and physical dimensions, centered on the closed pose.
		Closed = FModuleManager::LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities")).ConvertMeshesToStaticMesh(
			{Component}, FTransform(Component->Bounds.Origin), TEXT("/Game/Models/Diary/SM_Diary_Closed"));
		Actor->Destroy();
		if (!Closed) return 3;
	}
	const auto Save = [](UObject* Asset)
	{
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Asset->GetOutermost(), Asset,
			*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	};
	if (!bVerify)
	{
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("DiaryItemBackups") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		IFileManager::Get().MakeDirectory(*BackupDir, true);
		for (UObject* Asset : {static_cast<UObject*>(Item), static_cast<UObject*>(Blueprint)})
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
			if (IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename, false) != COPY_OK) return 4;
		}
		Item->PreviewMesh = Closed;
		Item->PreviewTransform = FTransform(FRotator(-75.f, 0.f, 0.f).Quaternion() * FRotator(0.f, 90.f, 0.f).Quaternion());
		// World pickup lies flat; the generic item preview faces its camera.
		auto* Default = Blueprint->GeneratedClass->GetDefaultObject<ATAWorldItem>();
		auto* Mesh = Default->FindComponentByClass<UStaticMeshComponent>();
		if (!Mesh) return 5;
		Mesh->SetStaticMesh(Closed);
		Mesh->EmptyOverrideMaterials();
		Mesh->SetRelativeTransform(FTransform::Identity);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (!Save(Closed) || !Save(Item) || !Save(Blueprint)) return 6;
	}
	// Validate the real actor, including construction-script overrides and fallback materials.
	auto* Pickup = World->SpawnActor<ATAWorldItem>(Blueprint->GeneratedClass);
	if (!Pickup) return 7;
	Pickup->DispatchBeginPlay();
	auto* Mesh = Pickup->FindComponentByClass<UStaticMeshComponent>();
	bool bValid = Item->PreviewMesh == Closed && Mesh && Mesh->GetStaticMesh() == Closed && Pickup->GetItemDef() == Item;
	for (int32 Index = 0; Mesh && Index < Mesh->GetNumMaterials(); ++Index)
	{
		auto* Material = Mesh->GetMaterial(Index);
		bValid &= Material && Material == Closed->GetMaterial(Index);
		UE_LOG(LogTemp, Display, TEXT("Diary pickup material %d: %s"), Index, *GetPathNameSafe(Material));
	}
	const FVector Size = Closed->GetBounds().BoxExtent * 2.f;
	bValid &= Size.Z < 5.f && Size.X > 10.f && Size.Y > 10.f;
	UE_LOG(LogTemp, Display, TEXT("Diary item %s: mesh=%s size=%s preview=%s"), bValid ? TEXT("PASS") : TEXT("FAIL"),
		*GetPathNameSafe(Item->PreviewMesh), *Size.ToString(), *Item->PreviewTransform.ToString());
	if (bValid && FParse::Param(*Params, TEXT("Render")))
	{
		auto* Stage = World->SpawnActor<ATAPreviewStage>();
		Stage->bAutoRotate = false;
		Stage->SetDisplayMesh(Item->PreviewMesh, Item->PreviewTransform);
		Stage->GetRenderTarget();
		FAssetCompilingManager::Get().FinishAllCompilation();
		World->SendAllEndOfFrameUpdates();
		FlushRenderingCommands();
		Stage->GetCaptureComponent()->CaptureScene();
		FlushRenderingCommands();
		FBufferArchive Image;
		if (!FImageUtils::ExportRenderTarget2DAsPNG(Stage->GetRenderTarget(), Image)) return 8;
		if (!FFileHelper::SaveArrayToFile(Image, *(FPaths::ProjectSavedDir() / TEXT("DiaryItemPreview.png")))) return 9;
	}
	return bValid ? 0 : 10;
}
