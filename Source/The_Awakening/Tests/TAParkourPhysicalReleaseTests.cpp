#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/TAParkourInputTestFixture.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/BoxComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAParkourPhysicalReleaseTest,
    "TheAwakening.Input.ParkourPhysicalRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTAParkourPhysicalReleaseTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Pawn = World->SpawnActor<ACharacter>();
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    auto* Parkour = NewObject<UTAParkourComponent>(Pawn);
    Pawn->AddInstanceComponent(Parkour); Parkour->RegisterComponent(); Parkour->BeginPlay();
    FTAParkourInputTestFixture Held(Pawn, Parkour);
    if (!TestNotNull(TEXT("Controller"), Held.PC) || !TestNotNull(TEXT("Jump action"), Held.Jump) || !TestNotNull(TEXT("Drop action"), Held.Drop))
    {
        World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
    }
    auto* PC = Held.PC;
    using TAInputOwnershipAdapter::EState;
    PC->ObservePlayerInputOwnership(EState::Owned);
    FTAInputDeviceDetector Processor(PC);
    auto* Marker = World->SpawnActor<ATAParkourMarker>();
    Parkour->RegisterMarker(Marker);
    auto Finish = [&]() { Parkour->TickComponent(2.f, LEVELTICK_All, nullptr); };
    auto Blocked = [&](const TCHAR* Label)
    {
        Held.Poll(); TestFalse(Label, Parkour->IsParkouring());
        TestFalse(TEXT("Consumed marker remains unavailable"), Parkour->CanParkourToMarker(Marker));
    };
    auto Rearmed = [&](const TCHAR* Label)
    {
        Held.Poll(); TestTrue(Label, Parkour->CanParkourToMarker(Marker));
    };

    TestFalse(TEXT("Absent key is unknown"), PC->FindHeldKeyObservation(EKeys::SpaceBar).IsSet());
    Held.Observe(EKeys::SpaceBar, 1.f); Held.Poll();
    TestTrue(TEXT("Physical Space launches"), Parkour->IsParkouring()); Finish();
    PC->ObservePlayerInputOwnership(EState::Transition);
    Blocked(TEXT("Transition retains consumption"));
    PC->ObservePlayerInputOwnership(EState::Lost);
    PC->ObservePlayerInputOwnership(EState::Lost);
    TestFalse(TEXT("Lost invalidates instead of recording Up"), PC->FindHeldKeyObservation(EKeys::SpaceBar).IsSet());
    Blocked(TEXT("Lost is not Release"));
    PC->ObservePlayerInputOwnership(EState::Owned);
    Blocked(TEXT("Recovery without physical sample cannot rearm"));
    FKeyEvent Repeat(EKeys::SpaceBar, FModifierKeysState(), Held.User, true, 0, 0);
    Processor.HandleKeyDownEvent(FSlateApplication::Get(), Repeat);
    TestTrue(TEXT("New repeat is observed Held"), PC->FindHeldKeyObservation(EKeys::SpaceBar).IsSet());
    Blocked(TEXT("New Held/repeat cannot bypass consumption"));
    FKeyEvent Up(EKeys::SpaceBar, FModifierKeysState(), Held.User, false, 0, 0);
    Processor.HandleKeyUpEvent(FSlateApplication::Get(), Up);
    TestTrue(TEXT("Real Up has valid zero observation"), PC->FindHeldKeyObservation(EKeys::SpaceBar).IsSet() && PC->ReadHeldKey(EKeys::SpaceBar).IsNearlyZero());
    Rearmed(TEXT("Observed source Up rearms despite unknown unused B"));

    // Both current physical sources participate; neither a partial Up nor a
    // disconnect of the other source proves release of the entire consumption.
    for (bool bDrop : {false, true})
    {
        Marker->MarkerType = bDrop ? ETAParkourMarkerType::DropDown : ETAParkourMarkerType::JumpToPoint;
        const FKey Keyboard = bDrop ? EKeys::LeftControl : EKeys::SpaceBar;
        const FKey Pad = bDrop ? EKeys::Gamepad_FaceButton_Bottom : EKeys::Gamepad_FaceButton_Right;
        Held.Observe(Keyboard, 1.f); Held.Observe(Pad, 1.f); Held.Poll();
        TestTrue(TEXT("Two-source consumption launches"), Parkour->IsParkouring()); Finish();
        Held.Observe(Keyboard, 0.f);
        Blocked(TEXT("One source Up is insufficient"));
        PC->InvalidateGamepadObservationOnDisconnect();
        Blocked(TEXT("Disconnect cannot substitute for remaining source Up"));
        Held.Observe(Pad, 1.f);
        Blocked(TEXT("Reconnect Held cannot rearm"));
        Held.Observe(Pad, 0.f);
        Rearmed(TEXT("All consumed sources valid zero rearms"));
    }

    // Real resolved mapping shadowing: A is no longer in Drop's mappings, but
    // the consumed source's physical identity must survive that change.
    Marker->MarkerType = ETAParkourMarkerType::DropDown;
    Held.Observe(EKeys::Gamepad_FaceButton_Bottom, 1.f); Held.Poll();
    TestTrue(TEXT("A alone launches Drop"), Parkour->IsParkouring()); Finish();
    auto* CoverContext = NewObject<UInputMappingContext>(PC);
    auto* Confirm = NewObject<UInputAction>(PC); Confirm->bConsumeInput = true;
    CoverContext->MapKey(Confirm, EKeys::Gamepad_FaceButton_Bottom);
    CoverContext->MapKey(Confirm, EKeys::Gamepad_FaceButton_Right);
    FModifyContextOptions Options; Options.bForceImmediately = true;
    Held.Mappings.AddMappingContext(CoverContext, 20, Options);
    TestFalse(TEXT("UI mapping shadows Drop action value"), PC->ReadHeldAction(Held.Drop).IsNonZero());
    Blocked(TEXT("Mapping shadow is not Release"));
    Held.Mappings.RemoveMappingContext(CoverContext, Options);
    TestTrue(TEXT("Mapping restoration exposes still-Held A"), PC->ReadHeldAction(Held.Drop).IsNonZero());
    Blocked(TEXT("Mapping restoration does not rearm"));
    FTAInputRequest Cover; Cover.Owner = PC; Cover.Priority = 200;
    Cover.Allowed = {ETAInputCapability::Confirm};
    const auto Handle = PC->AcquireInputRequest(Cover);
    Blocked(TEXT("Capability denial is not Release"));
    Held.Observe(EKeys::Gamepad_FaceButton_Bottom, 0.f);
    Rearmed(TEXT("Real Release rearms even while capability denied"));
    PC->ReleaseInputRequest(Handle);

    // Exercise B shadowing too; overlap exit never proves physical release.
    Marker->MarkerType = ETAParkourMarkerType::JumpToPoint;
    Held.Observe(EKeys::Gamepad_FaceButton_Right, 1.f); Held.Poll();
    TestTrue(TEXT("B alone launches Jump"), Parkour->IsParkouring()); Finish();
    Held.Mappings.AddMappingContext(CoverContext, 20, Options);
    TestFalse(TEXT("UI mapping shadows Jump action value"), PC->ReadHeldAction(Held.Jump).IsNonZero());
    Blocked(TEXT("B mapping shadow retains consumption"));
    Held.Mappings.RemoveMappingContext(CoverContext, Options);
    Blocked(TEXT("B mapping restoration retains consumption"));
    Parkour->UnregisterMarker(Marker); Parkour->RegisterMarker(Marker); Held.Poll();
    TestTrue(TEXT("Idle exit/reentry creates a new opportunity"), Parkour->IsParkouring()); Finish();
    Marker->Destroy(); Held.Poll();
    TestFalse(TEXT("Destroyed marker cannot launch"), Parkour->IsParkouring());
    TestTrue(TEXT("Destroyed marker consumption is actually pruned"), Parkour->ConsumedHeldMarkers.IsEmpty());
    Marker = World->SpawnActor<ATAParkourMarker>(); Parkour->RegisterMarker(Marker); Held.Poll();
    TestTrue(TEXT("Destroyed consumption does not block another marker"), Parkour->IsParkouring()); Finish();
    Held.Observe(EKeys::Gamepad_FaceButton_Right, 0.f);
    Rearmed(TEXT("B-only consumption rearms without unused Space"));

    // Real regression: consuming A naturally leaves its volume during parkour.
    // Continuous Held supports A -> B -> A -> B across real overlap opportunities.
    auto* MarkerA = Marker;
    auto* MarkerB = World->SpawnActor<ATAParkourMarker>();
    Held.Observe(EKeys::SpaceBar, 1.f); Held.Observe(EKeys::Gamepad_FaceButton_Right, 1.f);
    Held.Poll(); TestTrue(TEXT("A launches with both physical sources"), Parkour->IsParkouring());
    TestEqual(TEXT("A records both sources"), Parkour->ConsumedHeldMarkers.FindChecked(MarkerA).Num(), 2);
    Parkour->RegisterMarker(MarkerA);
    TestTrue(TEXT("Duplicate registration preserves consumption"), Parkour->ConsumedHeldMarkers.Contains(MarkerA));
    Parkour->UnregisterMarker(MarkerA); Finish();
    TestFalse(TEXT("Real exit ends A consumption during parkour"), Parkour->ConsumedHeldMarkers.Contains(MarkerA));
    TestFalse(TEXT("Outside A is ineligible"), Parkour->CanParkourToMarker(MarkerA));
    Parkour->RegisterMarker(MarkerB); Held.Poll();
    TestTrue(TEXT("Same hold may consume a new marker B"), Parkour->IsParkouring());
    TestTrue(TEXT("B gets its own consumption"), Parkour->ConsumedHeldMarkers.Contains(MarkerB));
    Parkour->UnregisterMarker(MarkerB); Finish();
    Parkour->RegisterMarker(MarkerA); Held.Poll();
    TestTrue(TEXT("Return to A while held executes once"), Parkour->IsParkouring()); Finish();
    Parkour->RegisterMarker(MarkerA);
    Blocked(TEXT("Repeated Register within A cannot rearm"));
    Parkour->UnregisterMarker(MarkerA); Parkour->RegisterMarker(MarkerB); Held.Poll();
    TestTrue(TEXT("A-B-A-B completes without any physical Release"), Parkour->IsParkouring()); Finish();
    Parkour->UnregisterMarker(MarkerB); Parkour->RegisterMarker(MarkerA); Held.Poll();
    TestTrue(TEXT("Another genuine entry into A executes"), Parkour->IsParkouring()); Finish();
    Held.Observe(EKeys::SpaceBar, 0.f);
    Blocked(TEXT("Same A overlap with only Space released cannot execute"));
    Held.Observe(EKeys::SpaceBar, 1.f);
    Blocked(TEXT("Repressing Space while B remains Held cannot execute"));
    Held.Observe(EKeys::SpaceBar, 0.f); Held.Observe(EKeys::Gamepad_FaceButton_Right, 0.f);
    Rearmed(TEXT("All sources released rearms A after natural exit"));
    TestTrue(TEXT("Release clears the remaining same-overlap consumption"), Parkour->ConsumedHeldMarkers.IsEmpty());
    Held.Observe(EKeys::SpaceBar, 1.f); Held.Poll();
    TestTrue(TEXT("A executes after real release"), Parkour->IsParkouring());
    Parkour->UnregisterMarker(MarkerA); Finish();
    Parkour->RegisterMarker(MarkerB); Held.Poll();
    TestTrue(TEXT("Return to B while held executes once"), Parkour->IsParkouring());
    MarkerB->Destroy(); Finish();
    TestTrue(TEXT("Finish prunes destroyed launch marker"), Parkour->ConsumedHeldMarkers.IsEmpty());

    // Use UE's real pair cache, then invoke the native callback in this isolated
    // world (no BeginPlay notifications). One component's exit is not actor exit.
    Marker = MarkerA;
    Parkour->RegisterMarker(Marker); Held.Poll(); Finish();
    TestTrue(TEXT("Multi-component test starts consumed"), Parkour->ConsumedHeldMarkers.Contains(Marker));
    auto* First = NewObject<UBoxComponent>(Pawn);
    auto* Second = NewObject<UBoxComponent>(Pawn);
    for (auto* Part : {First, Second})
    {
        Pawn->AddInstanceComponent(Part); Part->SetCollisionObjectType(ECC_Pawn);
        Part->SetCollisionResponseToAllChannels(ECR_Overlap); Part->SetGenerateOverlapEvents(true);
        Part->RegisterComponent();
        Marker->TriggerBox->BeginComponentOverlap(FOverlapInfo(Part, INDEX_NONE), false);
    }
    TestTrue(TEXT("UE pair cache observes the actor"), Marker->TriggerBox->IsOverlappingActor(Pawn));
    auto EndPart = [&](UPrimitiveComponent* Part)
    {
        Marker->TriggerBox->EndComponentOverlap(FOverlapInfo(Part, INDEX_NONE), false);
        struct FEndArgs { UPrimitiveComponent* Component; AActor* Actor; UPrimitiveComponent* Other; int32 Body; } Args
            {Marker->TriggerBox, Pawn, Part, INDEX_NONE};
        TGuardValue<bool> AllowEvents(GAllowActorScriptExecutionInEditor, true);
        Marker->ProcessEvent(Marker->FindFunctionChecked(TEXT("OnEndOverlap")), &Args);
    };
    EndPart(First);
    TestTrue(TEXT("Partial component exit preserves registration"), Parkour->GetOverlappingMarkers().Contains(Marker));
    Blocked(TEXT("Partial component exit preserves consumed latch"));
    EndPart(Second);
    TestFalse(TEXT("Final component exit removes registration"), Parkour->GetOverlappingMarkers().Contains(Marker));
    TestFalse(TEXT("Final component exit ends consumption"), Parkour->ConsumedHeldMarkers.Contains(Marker));
    Parkour->RegisterMarker(Marker); Held.Poll();
    TestTrue(TEXT("Reentry after final component exit executes"), Parkour->IsParkouring()); Finish();

    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
