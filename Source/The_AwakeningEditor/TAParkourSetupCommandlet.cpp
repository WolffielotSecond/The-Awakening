#include "TAParkourSetupCommandlet.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/Texture2D.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Movement/TAParkourMarker.h"
#include "Components/SplineComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "EditorWorldUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

UTAParkourSetupCommandlet::UTAParkourSetupCommandlet()
{ IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; }

int32 UTAParkourSetupCommandlet::Main(const FString& Params)
{
	bool OK = true;
	auto SaveParkourAsset = [&](UObject* Object)
	{
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString File = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		OK &= UPackage::SavePackage(Object->GetOutermost(), Object, *File, Args);
	};
	if (!Params.Contains(TEXT("AuditOnly")))
	{
		auto* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
		auto* Jump = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_ParkourJump.IA_ParkourJump"));
		auto* Drop = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_ParkourDrop.IA_ParkourDrop"));
		if (!Context || !Jump || !Drop) return 1;
		Context->UnmapKey(Jump, EKeys::Gamepad_FaceButton_Bottom);
		auto EnsureMapping = [&](UInputAction* Action, FKey Key)
		{
			for (const auto& Mapping : Context->GetMappings())
				if (Mapping.Action == Action && Mapping.Key == Key) return;
			Context->MapKey(Action, Key);
		};
		EnsureMapping(Jump, EKeys::SpaceBar);
		EnsureMapping(Jump, EKeys::Gamepad_FaceButton_Right);
		EnsureMapping(Drop, EKeys::LeftControl);
		EnsureMapping(Drop, EKeys::Gamepad_FaceButton_Bottom);
		SaveParkourAsset(Context);
		for (const FString Folder : {TEXT("Xbox"), TEXT("PS")})
		{
			const FString Source = TEXT("/Game/Textures/Keys/") + Folder + (Folder == TEXT("PS") ? TEXT("/PlayStation_Stick_L.PlayStation_Stick_L") : TEXT("/Xbox_Stick_L.Xbox_Stick_L"));
			auto* Texture = LoadObject<UTexture2D>(nullptr, *Source);
			if (!Texture) return 1;
			for (const FString Direction : {TEXT("UpLeft"), TEXT("DownLeft"), TEXT("UpRight"), TEXT("DownRight")})
			{
				const FString Name = TEXT("LS_") + Direction;
				const FString Path = TEXT("/Game/Textures/Keys/") + Folder + TEXT("/") + Name;
				if (FPackageName::DoesPackageExist(Path)) continue;
				auto* Copy = DuplicateObject<UTexture2D>(Texture, CreatePackage(*Path), *Name);
				Copy->SetFlags(RF_Public | RF_Standalone);
				FAssetRegistryModule::AssetCreated(Copy); SaveParkourAsset(Copy);
			}
		}
	}
	FString Report;
	auto Line = [&](const FString& Text) { Report += Text + TEXT("\n"); UE_LOG(LogTemp, Display, TEXT("PARKOUR_AUDIT %s"), *Text); };
	auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelDesign/BP_ParkourMarker.BP_ParkourMarker"));
	if (!BP) return 1;
	FKismetEditorUtilities::CompileBlueprint(BP);
	Line(FString::Printf(TEXT("Marker Blueprint compile status=%d"), int32(BP->Status)));
	OK &= BP->Status != BS_Error;
	// Inspect all authored levels without saving them; native migration must preserve the original component.
	auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);
	TArray<FAssetData> Worlds;
	Registry.GetAssetsByClass(UWorld::StaticClass()->GetClassPathName(), Worlds);
	for (const auto& Asset : Worlds)
	{
		if (!Asset.PackageName.ToString().StartsWith(TEXT("/Game/"))) continue;
		UWorld::InitializationValues Values;
		Values.RequiresHitProxies(false).ShouldSimulatePhysics(false).EnableTraceCollision(false)
			.CreateNavigation(false).CreateAISystem(false).AllowAudioPlayback(false).CreatePhysicsScene(true);
		FScopedEditorWorld Scope(FStringView(Asset.PackageName.ToString()), Values);
		auto* World = Scope.GetWorld(); if (!World) { OK = false; continue; }
		int32 Count = 0;
		auto Inspect = [&](AActor* Actor)
		{
			auto* It = Cast<ATAParkourMarker>(Actor); if (!It) return;
			++Count;
			Line(FString::Printf(TEXT("%s %s type=%d upper=%d lower=%d original=%s points=%d arc=%.2f duration=%.2f"),
				*Asset.PackageName.ToString(), *It->GetName(), int32(It->MarkerType), It->bEnableUpper, It->bEnableLower,
				*GetNameSafe(It->LandingSpline), It->LandingSpline ? It->LandingSpline->GetNumberOfSplinePoints() : 0,
				It->ArcHeight, It->JumpDuration));
			Line(FString::Printf(TEXT("  migrated slot=%s arc=%.2f duration=%.2f originalFirst=%s originalLast=%s"),
				It->bLegacySplineIsLower ? TEXT("Lower") : TEXT("Upper"), It->GetArcHeight(It->bLegacySplineIsLower),
				It->GetJumpDuration(It->bLegacySplineIsLower),
				It->LandingSpline ? *It->LandingSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local).ToString() : TEXT("missing"),
				It->LandingSpline ? *It->LandingSpline->GetLocationAtSplinePoint(It->LandingSpline->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::Local).ToString() : TEXT("missing")));
		};
		if (auto* Partition = World->GetWorldPartition())
		{
			FWorldPartitionHelpers::FForEachActorWithLoadingParams Options;
			Options.ActorClasses = {ATAParkourMarker::StaticClass()};
			FWorldPartitionHelpers::ForEachActorWithLoading(Partition, [&](const FWorldPartitionActorDescInstance* Desc)
			{ Inspect(Desc->GetActor()); return true; }, Options);
		}
		else for (AActor* Actor : World->PersistentLevel->Actors) Inspect(Actor);
		Line(FString::Printf(TEXT("Level %s markers=%d"), *Asset.PackageName.ToString(), Count));
	}
	FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("ParkourMigrationAudit.txt")));
	return OK ? 0 : 1;
}
