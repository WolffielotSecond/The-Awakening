#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/TAParkourInputTestFixture.h"
#include "Components/SplineComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/TAPromptComponent.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAUpperLowerParkourTest, "TheAwakening.Input.UpperLowerParkour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAUpperLowerParkourTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Pawn = World->SpawnActor<ACharacter>();
	Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	auto* Parkour = NewObject<UTAParkourComponent>(Pawn);
	Pawn->AddInstanceComponent(Parkour); Parkour->RegisterComponent(); Parkour->BeginPlay();
	FTAParkourInputTestFixture Held(Pawn, Parkour);
	if (!TestNotNull(TEXT("Real input controller blueprint"), Held.PC))
	{ World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
	auto* Marker = World->SpawnActor<ATAParkourMarker>();
	Marker->UpperArcHeight = 180; Marker->UpperJumpDuration = .6f;
	Marker->LowerArcHeight = 60; Marker->LowerJumpDuration = .9f;
	FTAParkourInputTestFixture::Register(Parkour, Marker);
	auto Release = [&]()
	{
		for (FKey Key : {EKeys::SpaceBar, EKeys::LeftControl, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Bottom}) Held.Observe(Key, 0);
		Held.Poll(); Pawn->SetActorLocation(FVector::ZeroVector);
	};
	auto Finish = [&]() { Parkour->TickComponent(2, LEVELTICK_All, nullptr); Pawn->SetActorLocation(FVector::ZeroVector); };
	for (int32 Route : {-1, 0, 1, 2})
	{
		for (bool Lower : {false, true})
		{
			auto* Spline = Marker->GetLandingSpline(Lower);
			const float Y = Route == 2 ? (Lower ? -300 : 300) : Route * 300;
			Spline->SetLocationAtSplinePoint(0, FVector(-100, Y, Lower ? -150 : 100), ESplineCoordinateSpace::Local, false);
			Spline->SetLocationAtSplinePoint(1, FVector(100, Y, Lower ? -150 : 100), ESplineCoordinateSpace::Local, true);
		}
		for (int32 Configuration : {1, 2, 3})
		{
			Marker->bEnableUpper = (Configuration & 1) != 0; Marker->bEnableLower = (Configuration & 2) != 0;
			for (bool RequestLower : {false, true})
				for (bool Pad : {false, true})
				{
					// No movement, stick or mouse direction is supplied anywhere in this matrix.
					const FKey Execute = Pad ? (RequestLower ? EKeys::Gamepad_FaceButton_Bottom : EKeys::Gamepad_FaceButton_Right) :
						(RequestLower ? EKeys::LeftControl : EKeys::SpaceBar);
					Held.Observe(Execute, 1);
					const bool ExpectedLower = RequestLower;
					if (!Marker->IsLandingEnabled(RequestLower))
					{
						const FVector ExistingBearing = (Marker->GetLandingLocationForSlot(Pawn->GetActorLocation(), Marker->bEnableLower) - Pawn->GetActorLocation()).GetSafeNormal2D();
						if (!ExistingBearing.IsNearlyZero()) Pawn->SetActorRotation(ExistingBearing.Rotation());
						Held.Poll();
						TestFalse(TEXT("Disabled requested slot never falls back to unique landing"), Parkour->IsParkouring());
						TestTrue(TEXT("Wrong action never consumes the marker"), Parkour->CanParkourToMarker(Marker));
						Release(); continue;
					}
					const FVector Expected = Marker->GetLandingLocationForSlot(Pawn->GetActorLocation(), ExpectedLower);
					const FVector Bearing = (Expected - Pawn->GetActorLocation()).GetSafeNormal2D();
					if (!Bearing.IsNearlyZero()) Pawn->SetActorRotation(Bearing.Rotation());
					const FRotator OriginalFacing = Pawn->GetActorRotation();
					Held.Poll();
					TestTrue(TEXT("Both devices/actions work on left, right, vertical and cross-side markers without movement"), Parkour->IsParkouring());
					if (Route == 0) TestTrue(TEXT("Pure vertical launch keeps original facing"), Pawn->GetActorRotation().Equals(OriginalFacing, .01f));
					TestEqual(TEXT("Selected duration retained"), Parkour->GetParkourDuration(), Marker->GetJumpDuration(ExpectedLower));
					Parkour->TickComponent(Marker->GetJumpDuration(ExpectedLower) * .5f, LEVELTICK_All, nullptr);
					TestTrue(TEXT("Original fixed-duration parabola retained"), Pawn->GetActorLocation().Equals(Expected * .5 + FVector(0, 0, Marker->GetArcHeight(ExpectedLower)), .02));
					Parkour->TickComponent(2, LEVELTICK_All, nullptr);
					TestTrue(TEXT("Correct logical endpoint selected"), Pawn->GetActorLocation().Equals(Expected, .02));
					Release();
				}
		}
	}
	Marker->bEnableUpper = Marker->bEnableLower = true;
	Held.Observe(EKeys::SpaceBar, 1); Held.Observe(EKeys::LeftControl, 1); Held.Poll();
	TestFalse(TEXT("Dual simultaneous Upper/Lower is rejected"), Parkour->IsParkouring());
	// Conflicting movement must not affect action selection; Lower becomes the sole held action.
	for (FKey Key : {EKeys::W, EKeys::S, EKeys::A, EKeys::D}) Held.Observe(Key, 1);
	Held.Observe(EKeys::SpaceBar, 0); Held.Poll();
	TestTrue(TEXT("Movement conflicts do not block Lower"), Parkour->IsParkouring()); Finish();
	Held.Observe(EKeys::SpaceBar, 1); Held.Poll();
	TestFalse(TEXT("Adding alternate action cannot bypass consumed marker"), Parkour->IsParkouring());
	Held.Observe(EKeys::SpaceBar, 0); Held.Poll();
	TestFalse(TEXT("Releasing unused alternate action cannot rearm"), Parkour->IsParkouring());
	Release();
	Marker->bEnableLower = false;
	Pawn->SetActorRotation(FRotator(0, 90, 0));
	Held.Observe(EKeys::SpaceBar, 1); Held.Observe(EKeys::LeftControl, 1); Held.Poll();
	TestFalse(TEXT("Single simultaneous Upper/Lower requests are rejected"), Parkour->IsParkouring());
	Held.Observe(EKeys::LeftControl, 0); Held.Poll();
	TestTrue(TEXT("Only matching Upper request launches Upper Only"), Parkour->IsParkouring()); Finish();
	Held.Observe(EKeys::LeftControl, 1); Held.Poll();
	TestFalse(TEXT("Alternate action cannot bypass Upper consumption"), Parkour->IsParkouring());
	Release();
	// Both legacy callable entry points use the same physically observed player path.
	Marker->bEnableLower = true;
	Pawn->SetActorRotation(FRotator(0, 90, 0));
	Held.Observe(EKeys::SpaceBar, 1); Parkour->TryParkourJump();
	TestTrue(TEXT("Upper compatibility entry executes"), Parkour->IsParkouring()); Finish(); Release();
	Pawn->SetActorRotation(FRotator(0, -90, 0));
	Held.Observe(EKeys::LeftControl, 1); Parkour->TryParkourDrop();
	TestTrue(TEXT("Lower compatibility entry executes"), Parkour->IsParkouring()); Finish(); Release();
	// Resolve actual remapped execution actions, not physical key constants.
	Held.Context->UnmapKey(Held.Jump, EKeys::SpaceBar); Held.Context->MapKey(Held.Jump, EKeys::J);
	Held.Context->UnmapKey(Held.Drop, EKeys::LeftControl); Held.Context->MapKey(Held.Drop, EKeys::K);
	FModifyContextOptions Options; Options.bForceImmediately = true;
	Held.Mappings.RequestRebuildControlMappings(Options);
	Held.Observe(EKeys::SpaceBar, 1); Held.Observe(EKeys::LeftControl, 1); Held.Poll();
	TestFalse(TEXT("Old physical keys do not execute after remap"), Parkour->IsParkouring());
	for (FKey Key : {EKeys::J, EKeys::K})
	{
		Pawn->SetActorRotation(FRotator(0, Key == EKeys::J ? 90 : -90, 0));
		Held.Observe(Key, 1); Held.Poll(); TestTrue(TEXT("Remapped action executes"), Parkour->IsParkouring());
		Finish(); Held.Observe(Key, 0); Held.Poll();
	}
	auto* DefaultIMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
	if (TestNotNull(TEXT("Authored IMC"), DefaultIMC))
	{
		for (const auto& Pair : TArray<TPair<UInputAction*, FKey>>{{Held.Jump, EKeys::SpaceBar}, {Held.Jump, EKeys::Gamepad_FaceButton_Right}, {Held.Drop, EKeys::LeftControl}, {Held.Drop, EKeys::Gamepad_FaceButton_Bottom}})
		{
			bool Found = false;
			for (const auto& Mapping : DefaultIMC->GetMappings()) if (Mapping.Action == Pair.Key && Mapping.Key == Pair.Value) Found = true;
			TestTrue(TEXT("Authored Upper/Lower default mapping exists"), Found);
		}
		for (const auto& Mapping : DefaultIMC->GetMappings())
			TestFalse(TEXT("A is not still bound to Upper"), Mapping.Action == Held.Jump && Mapping.Key == EKeys::Gamepad_FaceButton_Bottom);
	}
	auto* CharacterBP = LoadClass<ACharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
	if (TestNotNull(TEXT("Actual character blueprint"), CharacterBP))
	{
		for (const auto& Pair : TArray<TPair<FName, UInputAction*>>{{TEXT("ParkourJumpAction"), Held.Jump}, {TEXT("ParkourDropAction"), Held.Drop}})
		{
			const auto* Property = FindFProperty<FObjectPropertyBase>(CharacterBP, Pair.Key);
			TestTrue(TEXT("Character blueprint retains correct action reference"), Property && Property->GetObjectPropertyValue_InContainer(CharacterBP->GetDefaultObject()) == Pair.Value);
		}
	}
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAUpperLowerParkourMarkerTest, "TheAwakening.Input.UpperLowerParkourMarker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAUpperLowerParkourMarkerTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Marker = World->SpawnActor<ATAParkourMarker>();
	Marker->bEnableLower = true;
	const FVector Point = Marker->GetLandingSpline(true)->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
	Marker->bEnableLower = false; Marker->OnConstruction(Marker->GetActorTransform());
	TestFalse(TEXT("Disabled slot ineligible"), Marker->HasValidLanding(true));
	const FVector AnchorBefore = Marker->GetLandingTargetComponent(true)->GetComponentLocation();
	Marker->UpdateLandingTargets(FVector(50, 0, 0));
	TestTrue(TEXT("Disabled target not updated"), Marker->GetLandingTargetComponent(true)->GetComponentLocation().Equals(AnchorBefore));
	Marker->bEnableLower = true; Marker->OnConstruction(Marker->GetActorTransform());
	TestTrue(TEXT("Spline points survive disabling and construction"), Marker->GetLandingSpline(true)->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local).Equals(Point));
	Marker->bEnableUpper = Marker->bEnableLower = false;
	Marker->OnConstruction(Marker->GetActorTransform());
	TestFalse(TEXT("Both disabled invalid"), Marker->IsValidMarker());
	// Authored defaults set before construction (as with a new Blueprint) must not be reset by migration.
	auto* Deferred = World->SpawnActorDeferred<ATAParkourMarker>(ATAParkourMarker::StaticClass(), FTransform::Identity);
	Deferred->bEnableUpper = false; Deferred->bEnableLower = true; Deferred->LowerArcHeight = 55;
	Deferred->FinishSpawning(FTransform::Identity);
	TestTrue(TEXT("New Blueprint/deferred Lower defaults survive construction"), !Deferred->bEnableUpper && Deferred->bEnableLower && Deferred->LowerArcHeight == 55);
	Marker->bEnableUpper = Marker->bEnableLower = true;
	Marker->GetLandingSpline(true)->SetLocationAtSplinePoint(0, FVector(-100, -300, -100), ESplineCoordinateSpace::Local, false);
	Marker->GetLandingSpline(true)->SetLocationAtSplinePoint(1, FVector(100, -300, -100), ESplineCoordinateSpace::Local, true);
	TestTrue(TEXT("Opposite side slots retain valid configuration"), Marker->IsValidMarker());
	Marker->bEnableLower = false;
	Marker->SetActorTransform(FTransform(FRotator(0, 25, 0), FVector(30, 50, 20)));
	const FVector Player = Marker->GetActorTransform().TransformPosition(FVector(45, 0, 0));
	const FVector Result = Marker->GetActorTransform().InverseTransformPosition(Marker->GetLandingLocationForSlot(Player, false));
	TestTrue(TEXT("Rotated marker retains binary depth matching"), FMath::IsNearlyEqual(Result.X, 45, .01));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAUpperLowerParkourPromptTest, "TheAwakening.Prompt.UpperLowerParkour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAUpperLowerParkourPromptTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Pawn = World->SpawnActor<ACharacter>(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	auto* Parkour = NewObject<UTAParkourComponent>(Pawn);
	Pawn->AddInstanceComponent(Parkour); Parkour->RegisterComponent(); Parkour->BeginPlay();
	FTAParkourInputTestFixture Held(Pawn, Parkour);
	auto* Prompt = NewObject<UTAPromptComponent>(Pawn);
	Pawn->AddInstanceComponent(Prompt); Prompt->RegisterComponent(); Prompt->BeginPlay();
	Prompt->ParkourJumpAction = Held.Jump; Prompt->ParkourDropAction = Held.Drop;
	auto* Near = World->SpawnActor<ATAParkourMarker>(); Near->bEnableLower = true;
	auto* Far = World->SpawnActor<ATAParkourMarker>(FVector(20, 0, 0), FRotator::ZeroRotator);
	FTAParkourInputTestFixture::Register(Parkour, Near); FTAParkourInputTestFixture::Register(Parkour, Far);
	Prompt->UpdatePrompts();
	TestTrue(TEXT("Upper and Lower both share execution winner"), Prompt->GetShownSource(ETAPromptType::ParkourJump) == Near && Prompt->GetShownSource(ETAPromptType::ParkourDrop) == Near && Parkour->SelectParkourMarker() == Near);
	TestTrue(TEXT("Upper prompt uses Upper action"), Prompt->ActivePrompts.FindChecked(ETAPromptType::ParkourJump).Action == Held.Jump);
	TestTrue(TEXT("Lower prompt uses Lower action"), Prompt->ActivePrompts.FindChecked(ETAPromptType::ParkourDrop).Action == Held.Drop);
	Held.Observe(EKeys::S, 1); Held.Observe(EKeys::D, 1); Prompt->UpdatePrompts();
	TestTrue(TEXT("S never hides upper prompt"), Near->GetLandingPrompt(false)->IsPromptShown());
	TestTrue(TEXT("S keeps lower prompt visible"), Near->GetLandingPrompt(true)->IsPromptShown());
	Near->UpdateLandingTargets(FVector(60, 0, 0));
	TestTrue(TEXT("Both anchors independently match depth"), FMath::IsNearlyEqual(Near->GetLandingTargetComponent(false)->GetComponentLocation().X, 60, .01) && FMath::IsNearlyEqual(Near->GetLandingTargetComponent(true)->GetComponentLocation().X, 60, .01));
	auto* Anchor = Near->GetLandingPrompt(false);
	Anchor->ShowPromptChord({nullptr, nullptr, nullptr}, {FText::FromString(TEXT("W")), FText::FromString(TEXT("D")), FText::FromString(TEXT("Space"))}, FText::FromString(TEXT("Test")));
	auto* Control = Anchor->GetUserWidgetObject();
	bool HasChord = false;
	if (Control)
	{
		TArray<UWidget*> Widgets; Control->WidgetTree->GetAllWidgets(Widgets);
		for (auto* Child : Widgets)
			if (auto* Row = Cast<UHorizontalBox>(Child); Row && Row->GetChildrenCount() == 5) HasChord = true;
	}
	TestTrue(TEXT("Existing WBP supports chord row"), HasChord);
	Anchor->ShowPromptChord({nullptr, nullptr}, {FText::FromString(TEXT("LS")), FText::FromString(TEXT("A"))}, FText::FromString(TEXT("Test")), FText::FromString(TEXT("↗")));
	bool HasStickChord = false;
	if (Control)
	{
		TArray<UWidget*> Widgets; Control->WidgetTree->GetAllWidgets(Widgets);
		for (auto* Child : Widgets)
			if (auto* Row = Cast<UHorizontalBox>(Child); Row && Row->GetChildrenCount() == 4) HasStickChord = true;
	}
	TestTrue(TEXT("Stick direction is a suffix, not another plus-separated key"), HasStickChord);
	Anchor->ShowPrompt(nullptr, FText::FromString(TEXT("Ordinary prompt")));
	TestNotNull(TEXT("Ordinary single-icon prompt restores Image_Key"), Control ? Control->GetWidgetFromName(TEXT("Image_Key")) : nullptr);
	Near->bEnableLower = false; Prompt->UpdatePrompts();
	TestNull(TEXT("Disabled lower prompt removed"), Prompt->GetShownSource(ETAPromptType::ParkourDrop));
	TestFalse(TEXT("Disabled lower anchor hidden"), Near->GetLandingPrompt(true)->IsPromptShown());
	FTAParkourInputTestFixture::Register(Parkour, Near); TestTrue(TEXT("Registration order cannot change winner"), Parkour->SelectParkourMarker() == Near);
	Near->bEnableUpper = false; Near->bEnableLower = true; Prompt->UpdatePrompts();
	TestNull(TEXT("Lower Only has no upper prompt"), Prompt->GetShownSource(ETAPromptType::ParkourJump));
	TestTrue(TEXT("Lower Only uses Lower action"), Prompt->ActivePrompts.FindChecked(ETAPromptType::ParkourDrop).Action == Held.Drop);
	const FVector Expected = Near->GetLandingLocationForSlot(Pawn->GetActorLocation(), true);
	Held.Observe(EKeys::SpaceBar, 1); Held.Poll();
	TestFalse(TEXT("Space cannot execute a prompted Lower Only landing"), Parkour->IsParkouring());
	Held.Observe(EKeys::SpaceBar, 0); Held.Observe(EKeys::LeftControl, 1); Held.Poll();
	TestTrue(TEXT("Ctrl executes the prompted Lower Only landing"), Parkour->IsParkouring());
	Parkour->TickComponent(2, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Execution ends at prompted Lower target"), Pawn->GetActorLocation().Equals(Expected, .02));

	// Cross-side dual: only the landing inside its own cone may preview or execute.
	Held.Observe(EKeys::SpaceBar, 0); Held.Observe(EKeys::LeftControl, 0); Held.Poll(); Pawn->SetActorLocation(FVector::ZeroVector);
	Near->bEnableUpper = Near->bEnableLower = true;
	Near->GetLandingSpline(true)->SetLocationAtSplinePoint(0, FVector(-100, -300, -200), ESplineCoordinateSpace::Local, false);
	Near->GetLandingSpline(true)->SetLocationAtSplinePoint(1, FVector(100, -300, -200), ESplineCoordinateSpace::Local, true);
	Pawn->SetActorRotation(FRotator(0, 90, 0)); Prompt->UpdatePrompts();
	TestTrue(TEXT("Facing Upper shows Upper"), Near->GetLandingPrompt(false)->IsPromptShown());
	TestFalse(TEXT("Back-facing Lower is hidden even on the winning marker"), Near->GetLandingPrompt(true)->IsPromptShown());
	Held.Observe(EKeys::LeftControl, 1); Held.Poll();
	TestFalse(TEXT("Lower request cannot launch a back-facing landing"), Parkour->IsParkouring());
	Pawn->SetActorRotation(FRotator(0, 0, 0)); Prompt->UpdatePrompts();
	TestFalse(TEXT("Outside both cones hides Upper"), Near->GetLandingPrompt(false)->IsPromptShown());
	TestFalse(TEXT("Outside both cones hides Lower"), Near->GetLandingPrompt(true)->IsPromptShown());
	for (float Yaw : {10.f, 170.f})
	{
		Pawn->SetActorRotation(FRotator(0, Yaw, 0)); Prompt->UpdatePrompts();
		TestFalse(TEXT("Both strict Upper boundaries hide its preview"), Near->GetLandingPrompt(false)->IsPromptShown());
	}
	Pawn->SetActorRotation(FRotator(0, -90, 0)); Prompt->UpdatePrompts();
	TestFalse(TEXT("Turning to Lower hides Upper"), Near->GetLandingPrompt(false)->IsPromptShown());
	TestTrue(TEXT("Turning to Lower shows Lower"), Near->GetLandingPrompt(true)->IsPromptShown());
	const FVector LowerEnd = Near->GetLandingLocationForSlot(Pawn->GetActorLocation(), true);
	Held.Poll();
	TestTrue(TEXT("Rejected hold did not consume marker; turning inside launches"), Parkour->IsParkouring());
	Parkour->TickComponent(2, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Cone-approved execution matches the visible landing"), Pawn->GetActorLocation().Equals(LowerEnd, .02));

	Prompt->EndPlay(EEndPlayReason::Destroyed);
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif

