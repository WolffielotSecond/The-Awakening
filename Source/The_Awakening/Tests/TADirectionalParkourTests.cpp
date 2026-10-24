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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADirectionalParkourTest, "TheAwakening.Input.DirectionalParkour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADirectionalParkourTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Pawn = World->SpawnActor<ACharacter>();
	Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	auto* Parkour = NewObject<UTAParkourComponent>(Pawn);
	Pawn->AddInstanceComponent(Parkour); Parkour->RegisterComponent(); Parkour->BeginPlay();
	FTAParkourInputTestFixture Held(Pawn, Parkour);
	if (!TestNotNull(TEXT("Real input controller blueprint"), Held.PC)) return false;
	auto* Marker = World->SpawnActor<ATAParkourMarker>();
	Marker->UpperArcHeight = 180; Marker->UpperJumpDuration = .6f;
	Marker->LowerArcHeight = 60; Marker->LowerJumpDuration = .9f;
	FTAParkourInputTestFixture::Register(Parkour, Marker);
	for (int32 Side : {-1, 1})
	{
		for (bool Lower : {false, true})
		{
			auto* Spline = Marker->GetLandingSpline(Lower);
			Spline->SetLocationAtSplinePoint(0, FVector(-100, Side * 300, Lower ? -150 : 100), ESplineCoordinateSpace::Local, false);
			Spline->SetLocationAtSplinePoint(1, FVector(100, Side * 300, Lower ? -150 : 100), ESplineCoordinateSpace::Local, true);
		}
		for (int32 Configuration : {1, 2, 3})
		{
			Marker->bEnableUpper = (Configuration & 1) != 0; Marker->bEnableLower = (Configuration & 2) != 0;
			for (int32 Vertical : {-1, 0, 1})
			{
				for (bool Pad : {false, true})
				{
					Held.Observe(EKeys::A, !Pad && Side < 0); Held.Observe(EKeys::D, !Pad && Side > 0);
					Held.Observe(EKeys::W, !Pad && Vertical > 0); Held.Observe(EKeys::S, !Pad && Vertical < 0);
					Held.Observe(EKeys::Gamepad_LeftX, Pad ? Side * .7f : 0);
					Held.Observe(EKeys::Gamepad_LeftY, Pad ? Vertical * .45f : 0);
					Held.Observe(EKeys::SpaceBar, !Pad); Held.Observe(EKeys::Gamepad_FaceButton_Bottom, Pad);
					const bool ExpectedLower = Configuration == 2 || (Configuration == 3 && Vertical < 0);
					const FVector Expected = Marker->GetLandingLocationForSlot(Pawn->GetActorLocation(), ExpectedLower);
					Held.Poll();
					TestTrue(TEXT("All keyboard/pad side + vertical combinations launch"), Parkour->IsParkouring());
					TestEqual(TEXT("Selected duration retained"), Parkour->GetParkourDuration(), Marker->GetJumpDuration(ExpectedLower));
					Parkour->TickComponent(Marker->GetJumpDuration(ExpectedLower) * .5f, LEVELTICK_All, nullptr);
					TestTrue(TEXT("Original fixed-duration parabola retained"), Pawn->GetActorLocation().Equals(Expected * .5 + FVector(0, 0, Marker->GetArcHeight(ExpectedLower)), .02));
					Parkour->TickComponent(2, LEVELTICK_All, nullptr);
					TestTrue(TEXT("Correct fixed endpoint selected"), Pawn->GetActorLocation().Equals(Expected, .02));
					Held.Observe(EKeys::SpaceBar, 0); Held.Observe(EKeys::Gamepad_FaceButton_Bottom, 0); Held.Poll();
					Pawn->SetActorLocation(FVector::ZeroVector);
				}
			}
		}
	}
	Marker->bEnableUpper = Marker->bEnableLower = true;
	Held.Observe(EKeys::Gamepad_LeftX, 0); Held.Observe(EKeys::Gamepad_LeftY, 0);
	Held.Observe(EKeys::A, 0); Held.Observe(EKeys::D, 0); Held.Observe(EKeys::W, 1);
	Held.Observe(EKeys::SpaceBar, 1); Held.Poll();
	TestFalse(TEXT("W + execute alone rejected"), Parkour->IsParkouring());
	Held.Observe(EKeys::W, 0); Held.Observe(EKeys::S, 1); Held.Poll();
	TestFalse(TEXT("S + execute alone rejected"), Parkour->IsParkouring());
	Held.Observe(EKeys::A, 1); Held.Observe(EKeys::D, 1); Held.Poll();
	TestFalse(TEXT("A/D conflict rejected"), Parkour->IsParkouring());
	Held.Observe(EKeys::A, 0); Held.Observe(EKeys::W, 1); Held.Poll();
	TestFalse(TEXT("W/S conflict on dual rejected"), Parkour->IsParkouring());
	Held.Observe(EKeys::W, 0); Held.Observe(EKeys::S, 0); Held.Observe(EKeys::D, 0);
	Held.Observe(EKeys::Gamepad_LeftX, .19); Held.Poll();
	TestFalse(TEXT("Stick inside dead zone rejected"), Parkour->IsParkouring());
	Held.Observe(EKeys::Gamepad_LeftX, .34); Held.Poll();
	TestFalse(TEXT("Stick below horizontal threshold rejected"), Parkour->IsParkouring());
	Held.Observe(EKeys::Gamepad_LeftX, .36); Held.Poll();
	TestTrue(TEXT("Direction after execute can launch without buffer delay"), Parkour->IsParkouring());
	Parkour->TickComponent(2, LEVELTICK_All, nullptr); Pawn->SetActorLocation(FVector::ZeroVector);
	Held.Observe(EKeys::Gamepad_LeftX, 0); Held.Observe(EKeys::D, 1); Held.Poll();
	TestFalse(TEXT("Direction release/switch never rearms execution hold"), Parkour->IsParkouring());
	Held.Observe(EKeys::SpaceBar, 0); Held.Poll();
	Held.Observe(EKeys::LeftControl, 1); Held.Observe(EKeys::Gamepad_FaceButton_Right, 1); Held.Poll();
	TestFalse(TEXT("Legacy Ctrl/B do not execute"), Parkour->IsParkouring());
	// Live remapping resolves both direction and execute without hardcoded keys.
	Held.Context->UnmapKey(Held.Jump, EKeys::SpaceBar); Held.Context->MapKey(Held.Jump, EKeys::J);
	Held.Context->UnmapKey(Held.Right, EKeys::D); Held.Context->MapKey(Held.Right, EKeys::L);
	FModifyContextOptions Options; Options.bForceImmediately = true;
	Held.Mappings.RequestRebuildControlMappings(Options);
	Held.Observe(EKeys::J, 1); Held.Observe(EKeys::L, 1); Held.Poll();
	TestTrue(TEXT("Remapped direction + execute launch"), Parkour->IsParkouring());
	Parkour->AbortParkour();
	auto* DefaultIMC = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default"));
	bool HasSpace = false, HasA = false, HasOld = false;
	if (TestNotNull(TEXT("Authored IMC"), DefaultIMC))
	{
		for (const auto& Mapping : DefaultIMC->GetMappings())
		{
			if (Mapping.Action == Held.Jump) { HasSpace |= Mapping.Key == EKeys::SpaceBar; HasA |= Mapping.Key == EKeys::Gamepad_FaceButton_Bottom; HasOld |= Mapping.Key == EKeys::Gamepad_FaceButton_Right; }
			HasOld |= Mapping.Action == Held.Drop;
		}
		TestTrue(TEXT("Authored unified defaults are Space/A without Drop/B"), HasSpace && HasA && !HasOld);
	}
	for (const FString Folder : {TEXT("Xbox"), TEXT("PS")})
		for (const FString Direction : {TEXT("UpLeft"), TEXT("DownLeft"), TEXT("UpRight"), TEXT("DownRight")})
			TestNotNull(TEXT("Diagonal placeholder loads"), LoadObject<UObject>(nullptr, *(TEXT("/Game/Textures/Keys/") + Folder + TEXT("/LS_") + Direction + TEXT(".LS_") + Direction)));
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADirectionalParkourMarkerTest, "TheAwakening.Input.DirectionalParkourMarker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADirectionalParkourMarkerTest::RunTest(const FString&)
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
	TestEqual(TEXT("Opposite side slots safely rejected"), Marker->GetLandingSide(FVector::ZeroVector), 0);
	Marker->bEnableLower = false;
	Marker->SetActorTransform(FTransform(FRotator(0, 25, 0), FVector(30, 50, 20)));
	const FVector Player = Marker->GetActorTransform().TransformPosition(FVector(45, 0, 0));
	const FVector Result = Marker->GetActorTransform().InverseTransformPosition(Marker->GetLandingLocationForSlot(Player, false));
	TestTrue(TEXT("Rotated marker retains binary depth matching"), FMath::IsNearlyEqual(Result.X, 45, .01));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADirectionalParkourPromptTest, "TheAwakening.Prompt.DirectionalParkour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADirectionalParkourPromptTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Pawn = World->SpawnActor<ACharacter>(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	auto* Parkour = NewObject<UTAParkourComponent>(Pawn);
	Pawn->AddInstanceComponent(Parkour); Parkour->RegisterComponent(); Parkour->BeginPlay();
	FTAParkourInputTestFixture Held(Pawn, Parkour);
	auto* Prompt = NewObject<UTAPromptComponent>(Pawn);
	Pawn->AddInstanceComponent(Prompt); Prompt->RegisterComponent(); Prompt->BeginPlay();
	Prompt->ParkourJumpAction = Held.Jump;
	auto* Near = World->SpawnActor<ATAParkourMarker>(); Near->bEnableLower = true;
	auto* Far = World->SpawnActor<ATAParkourMarker>(FVector(20, 0, 0), FRotator::ZeroRotator);
	FTAParkourInputTestFixture::Register(Parkour, Near); FTAParkourInputTestFixture::Register(Parkour, Far);
	Prompt->UpdatePrompts();
	TestTrue(TEXT("Upper and Lower both share execution winner"), Prompt->GetShownSource(ETAPromptType::ParkourJump) == Near && Prompt->GetShownSource(ETAPromptType::ParkourDrop) == Near && Parkour->SelectDirectionalMarker(0) == Near);
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
	FTAParkourInputTestFixture::Register(Parkour, Near); TestTrue(TEXT("Registration order cannot change winner"), Parkour->SelectDirectionalMarker(1) == Near);
	Prompt->EndPlay(EEndPlayReason::Destroyed);
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif

