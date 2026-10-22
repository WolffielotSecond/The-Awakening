#include "Document/TADocumentDefinition.h"
#include "Document/TADiaryDocumentComponent.h"
#include "Document/TAPreviewStage.h"
#include "Document/TAWorldDocument.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADocumentMaterialsTest, "TheAwakening.Document.Paper.MaterialOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADocumentMaterialsTest::RunTest(const FString&)
{
	auto* Original = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_67.DA_Document_67"));
	if (!TestNotNull(TEXT("Existing paper definition"), Original)) return false;
	TestEqual(TEXT("Existing asset inherits default front"), GetPathNameSafe(Original->PaperFrontMaterial), FString(TEXT("/Game/Models/Document/M_Document_Front.M_Document_Front")));
	TestEqual(TEXT("Existing asset inherits default back"), GetPathNameSafe(Original->PaperBackMaterial), FString(TEXT("/Game/Models/Document/M_Document_Back.M_Document_Back")));
	auto* Custom = DuplicateObject<UTADocumentDefinition>(Original, GetTransientPackage());
	Custom->PaperFrontMaterial = UMaterialInstanceDynamic::Create(Original->PaperFrontMaterial, Custom);
	Custom->PaperBackMaterial = UMaterialInstanceDynamic::Create(Original->PaperBackMaterial, Custom);
	const auto Check = [&](UMeshComponent* Mesh, UTADocumentDefinition* Definition)
	{
		const int32 Front = Mesh->GetMaterialIndex(TEXT("M_Paper_Front"));
		const int32 Back = Mesh->GetMaterialIndex(TEXT("M_Paper_Back"));
		TestTrue(TEXT("Authored front and back slots found"), Front != INDEX_NONE && Back != INDEX_NONE);
		TestTrue(TEXT("Front receives this document's material"), Mesh->GetMaterial(Front) == Definition->PaperFrontMaterial);
		TestTrue(TEXT("Back receives this document's material"), Mesh->GetMaterial(Back) == Definition->PaperBackMaterial);
	};
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	auto* Stage = ATAPreviewStage::SpawnPreviewStage(World, nullptr, 7);
	Stage->SetDocument(Custom, Custom->PickupPreviewTransform, false);
	Check(Stage->GetSkeletalMeshComponent(), Custom);
	Stage->SetDocument(Original, Original->ReaderTransform, true);
	Check(Stage->GetSkeletalMeshComponent(), Original);
	auto* Pickup = World->SpawnActor<ATAWorldDocument>();
	Pickup->Definition = Custom;
	Pickup->OnConstruction(FTransform::Identity);
	Check(Pickup->SkeletalMeshComponent, Custom);
	Pickup->Definition = Original;
	Pickup->OnConstruction(FTransform::Identity);
	Check(Pickup->SkeletalMeshComponent, Original);
	auto* Sheet = NewObject<UTADiaryDocumentComponent>(Stage);
	Sheet->RegisterComponent();
	Sheet->RegisterMeshes();
	Sheet->Configure(Custom, 14.f);
	Check(Sheet->GetSkeletalMeshComponent(), Custom);
	Sheet->Configure(Original, 14.f);
	Check(Sheet->GetSkeletalMeshComponent(), Original);
	// A reordered static model and an unrelated slot must work without relying on slot indices.
	auto* Static = NewObject<UStaticMesh>();
	Static->GetStaticMaterials().Add(FStaticMaterial(Original->PaperBackMaterial, TEXT("M_Paper_Back")));
	Static->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), TEXT("Other")));
	Static->GetStaticMaterials().Add(FStaticMaterial(Original->PaperFrontMaterial, TEXT("M_Paper_Front")));
	auto* Component = NewObject<UStaticMeshComponent>(Stage);
	Component->SetStaticMesh(Static);
	Custom->ApplyPaperMaterials(Component);
	Check(Component, Custom);
	TestTrue(TEXT("Unrelated material preserved"), Component->GetMaterial(1) == Static->GetMaterial(1));
	Custom->PaperFrontMaterial = nullptr;
	Custom->PaperBackMaterial = nullptr;
	Custom->ApplyPaperMaterials(Component);
	Check(Component, Original);
	TestTrue(TEXT("Shared static mesh remains unchanged"), Static->GetMaterial(2) == Original->PaperFrontMaterial);
	Sheet->DestroyComponent();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
