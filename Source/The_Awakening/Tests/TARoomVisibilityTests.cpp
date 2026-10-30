#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Camera/TARoomRegion.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace TARoomVisibilityTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		UTARoomRegistrySubsystem* Registry = nullptr;

		FFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Registry = World->GetSubsystem<UTARoomRegistrySubsystem>();
		}
		~FFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		ATARoomRegion* Region(FName Id)
		{
			ATARoomRegion* R = World->SpawnActor<ATARoomRegion>();
			R->RoomId = Id;
			Registry->RegisterRegion(R);
			return R;
		}
		UStaticMeshComponent* Mesh()
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Actor);
			Actor->AddInstanceComponent(C);
			Actor->SetRootComponent(C);
			C->RegisterComponent();
			return C;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTARoomExplicitBlackoutTest, "TheAwakening.Camera.RoomVisibility.ExplicitRulesAndRestoration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTARoomExplicitBlackoutTest::RunTest(const FString&)
{
	TARoomVisibilityTests::FFixture F;
	ATARoomRegion* Hall = F.Region(TEXT("Home.Hall"));
	ATARoomRegion* Bedroom = F.Region(TEXT("Home.SisterBedroom"));
	ATARoomRegion* Other = F.Region(TEXT("Home.Other"));
	UStaticMeshComponent* Furniture = F.Mesh();
	UStaticMeshComponent* Wall = F.Mesh();
	UStaticMeshComponent* Unrelated = F.Mesh();
	Bedroom->IncludedAssets.Add(Furniture->GetOwner());
	Other->IncludedAssets.Add(Unrelated->GetOwner());
	Furniture->SetRenderInDepthPass(false);
	Furniture->SetRenderCustomDepth(true);
	Furniture->SetCustomDepthStencilValue(2);
	F.Registry->ApplyRoomVisibility(Hall);
	TestTrue(TEXT("Unspecified room remains visible"), Furniture->bRenderInMainPass);

	Hall->BlackoutRoomIds = {Bedroom->RoomId, Bedroom->RoomId, NAME_None, FName(TEXT("Missing.Room"))};
	Hall->BlackoutActors = {Wall->GetOwner(), Furniture->GetOwner(), nullptr};
	// Target rules must not be recursively executed.
	Bedroom->BlackoutRoomIds.Add(Other->RoomId);
	F.Registry->ApplyRoomVisibility(Hall);
	TestFalse(TEXT("Specified room assets blacked out"), Furniture->bRenderInMainPass);
	TestFalse(TEXT("Extra wall blacked out"), Wall->bRenderInMainPass);
	TestEqual(TEXT("Blackout stencil"), Furniture->CustomDepthStencilValue, 3);
	TestTrue(TEXT("Unspecified room and nonrecursive rule remain visible"), Unrelated->bRenderInMainPass);
	TestTrue(TEXT("Room reports explicitly targeted state"), Bedroom->IsBlackedOut());
	F.Registry->ApplyRoomVisibility(Hall);

	Other->BlackoutActors.Add(Furniture->GetOwner());
	F.Registry->ApplyRoomVisibility(Other);
	TestFalse(TEXT("Shared target remains blacked out across regions"), Furniture->bRenderInMainPass);
	TestTrue(TEXT("Previous extra wall restored"), Wall->bRenderInMainPass);
	TestFalse(TEXT("Room flag follows IDs, not individual actor references"), Bedroom->IsBlackedOut());
	F.Registry->ApplyRoomVisibility(nullptr);
	TestTrue(TEXT("Original main pass restored after repeated/shared references"), Furniture->bRenderInMainPass);
	TestFalse(TEXT("Original depth pass restored exactly"), Furniture->bRenderInDepthPass);
	TestTrue(TEXT("Original custom depth restored exactly"), Furniture->bRenderCustomDepth);
	TestEqual(TEXT("Original stencil restored exactly"), Furniture->CustomDepthStencilValue, 2);

	F.Registry->ApplyRoomVisibility(Hall);
	F.Registry->UnregisterRegion(Bedroom);
	TestFalse(TEXT("Direct reference keeps asset blacked out after target region unload"), Furniture->bRenderInMainPass);
	Hall->BlackoutActors.Reset();
	F.Registry->ApplyRoomVisibility(Hall);
	TestTrue(TEXT("Unloaded room contributes no assets"), Furniture->bRenderInMainPass);
	Hall->BlackoutActors.Add(Wall->GetOwner());
	F.Registry->ApplyRoomVisibility(Hall);
	F.Registry->UnregisterRegion(Hall);
	TestTrue(TEXT("Active region unloading restores wall"), Wall->bRenderInMainPass);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTARoomRegionSelectionTest, "TheAwakening.Camera.RoomVisibility.PriorityAndNoRegion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTARoomRegionSelectionTest::RunTest(const FString&)
{
	TARoomVisibilityTests::FFixture F;
	ATARoomRegion* A = F.Region(TEXT("A"));
	ATARoomRegion* B = F.Region(TEXT("B"));
	TestTrue(TEXT("Equal priority retains previous region"), F.Registry->ResolveRegion(FVector::ZeroVector, B) == B);
	A->Priority = 1;
	TestTrue(TEXT("Higher priority wins"), F.Registry->ResolveRegion(FVector::ZeroVector, B) == A);
	TestNull(TEXT("Outside all regions uses fallback"), F.Registry->ResolveRegion(FVector(100000.f), A));
	return true;
}
#endif
