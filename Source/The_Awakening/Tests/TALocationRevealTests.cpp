#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/TALocationRevealWidget.h"
#include "Engine/World.h"
#include "Engine/Font.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontCache.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTALocationRevealTest,"TheAwakening.LocationReveal.InterruptionAndLifecycle",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTALocationRevealTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 auto* Widget=CreateWidget<UTALocationRevealWidget>(World);
 if (!TestNotNull(TEXT("Native UMG widget"),Widget)) { World->DestroyWorld(false); return false; }
 const FText Chapter=FText::FromString(TEXT("Chapter 01"));
 const FText A=FText::FromString(TEXT("Hospital")), B=FText::FromString(TEXT("Home")), C=FText::FromString(TEXT("Courtyard"));
 TestEqual(TEXT("Passive hit testing"),Widget->GetVisibility(),ESlateVisibility::HitTestInvisible);
 TestFalse(TEXT("Never focuses"),Widget->IsFocusable());
 TestEqual(TEXT("Initially transparent"),Widget->GetRenderOpacity(),0.f);
 Widget->ShowLocationReveal(Chapter,A);
 Widget->AdvancePresentation(.4f);
 Widget->ShowLocationReveal(Chapter,A);
 TestEqual(TEXT("Repeat ignored in reveal"),Widget->GetRevealPhase(),ETALocationRevealPhase::Revealing);
 Widget->AdvancePresentation(.6f);
 TestEqual(TEXT("Holding after entrance"),Widget->GetRevealPhase(),ETALocationRevealPhase::Holding);
 Widget->AdvancePresentation(1.75f);
 TestEqual(TEXT("Erasing after hold"),Widget->GetRevealPhase(),ETALocationRevealPhase::Erasing);
 Widget->AdvancePresentation(1);
 TestEqual(TEXT("ECG continues moving beyond the right edge"),Widget->GetRevealPhase(),ETALocationRevealPhase::FadingLine);
 Widget->AdvancePresentation(Widget->PhaseDuration()*.5f);
 TestTrue(TEXT("Normal exit does not fade"),FMath::IsNearlyEqual(Widget->GetRenderOpacity(),1.f));
 // External opacity changes must still be respected by interruption fading.
 Widget->SetRenderOpacity(.5f);
 Widget->ShowLocationReveal(Chapter,B);
 Widget->AdvancePresentation(.1f);
 TestTrue(TEXT("Interrupted from actual .5 opacity"),FMath::IsNearlyEqual(Widget->GetRenderOpacity(),.25f));
 Widget->AdvancePresentation(.1f);
 TestEqual(TEXT("New reveal after fade"),Widget->GetRevealPhase(),ETALocationRevealPhase::Revealing);
 Widget->AdvancePresentation(4);
 TestEqual(TEXT("Normal full playback ends"),Widget->GetRevealPhase(),ETALocationRevealPhase::Hidden);
 TestEqual(TEXT("Final fully transparent"),Widget->GetRenderOpacity(),0.f);
 Widget->ShowLocationReveal(Chapter,B);
 TestEqual(TEXT("Repeat after completion ignored"),Widget->GetRevealPhase(),ETALocationRevealPhase::Hidden);
 for (float At : {.4f,1.4f,3.1f})
 {
  Widget->ShowLocationReveal(Chapter,A,true); Widget->AdvancePresentation(At);
  Widget->ShowLocationReveal(Chapter,B);
  TestEqual(TEXT("Interrupts each active phase"),Widget->GetRevealPhase(),ETALocationRevealPhase::Interrupting);
  Widget->AdvancePresentation(.1f);
  const float Opacity=Widget->GetRenderOpacity();
  Widget->ShowLocationReveal(Chapter,C);
  TestEqual(TEXT("New requests do not restart fade"),Widget->GetRenderOpacity(),Opacity);
  Widget->AdvancePresentation(.1f);
  Widget->ShowLocationReveal(Chapter,C);
  TestEqual(TEXT("Only newest request actually started"),Widget->GetRevealPhase(),ETALocationRevealPhase::Revealing);
  Widget->ShowLocationReveal(Chapter,C,true);
  TestEqual(TEXT("Force replay interrupts same region"),Widget->GetRevealPhase(),ETALocationRevealPhase::Interrupting);
  Widget->AdvancePresentation(4.3f);
 }
 FTALocationRevealClock Clock;
 Clock.Request(Chapter,A,false,0); const auto Old=Clock.PlayingVersion;
 Clock.Request(Chapter,B,false,.7f); Clock.Elapsed=.1f;
 Clock.Request(Chapter,C,false,.35f);
 TestTrue(TEXT("Version invalidates old cues immediately"),Old!=Clock.RequestVersion);
 TestEqual(TEXT("Pending replacement retains fade time"),Clock.Elapsed,.1f);
 TestEqual(TEXT("Pending replacement retains initial opacity"),Clock.FadeStartOpacity,.7f);
 Clock.StartPending();
 TestTrue(TEXT("Latest FText retained"),Clock.Location.IdenticalTo(C));
 TestEqual(TEXT("New generation owns playback"),Clock.RequestVersion,Clock.PlayingVersion);
 TestFalse(TEXT("New cues reset"),Clock.bInPosted||Clock.bOutPosted);
 Widget->InEvent=TSoftObjectPtr<UAkAudioEvent>(FSoftObjectPath(TEXT("/Game/Missing/NoLocationCue.NoLocationCue")));
 Widget->ShowLocationReveal(Chapter,A,true);
 Widget->AdvancePresentation(.1f);
 const auto OldVersion=Widget->Clock.PlayingVersion;
 Widget->ShowLocationReveal(Chapter,B);
 TestTrue(TEXT("Widget immediately invalidates old cue generation"),Widget->Clock.RequestVersion!=OldVersion);
 Widget->AdvancePresentation(.2f);
 TestFalse(TEXT("Old cue state is cleared before new sequence"),Widget->Clock.bInPosted||Widget->Clock.bOutPosted);
 Widget->AdvancePresentation(4);
 TestEqual(TEXT("Unresolvable soft event is silent and completes"),Widget->GetRevealPhase(),ETALocationRevealPhase::Hidden);
 Widget->ShowLocationReveal(Chapter,A,true); Widget->ShowLocationReveal(Chapter,B);
 Widget->NativeDestruct(); Widget->AdvancePresentation(10);
 TestEqual(TEXT("Destruction cancels running and pending work"),Widget->GetRevealPhase(),ETALocationRevealPhase::Hidden);
 TestFalse(TEXT("No pending work after destruction"),Widget->Clock.bPending);
 Widget->RemoveFromParent(); World->DestroyWorld(false);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTALocationRevealInputTest,"TheAwakening.LocationReveal.PassiveInputOwnership",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTALocationRevealInputTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 auto* Class=LoadClass<AThe_AwakeningPlayerController>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PC=Class ? World->SpawnActor<AThe_AwakeningPlayerController>(Class) : nullptr;
 if (!TestNotNull(TEXT("Existing project controller"),PC)) { World->DestroyWorld(false); return false; }
 auto* Local=NewObject<ULocalPlayer>(GEngine); Local->PlayerController=PC; PC->Player=Local; World->AddController(PC);
 PC->SynchronizeInputPresentation(); int32 Changes=0;
 const auto Handle=PC->OnInputOwnerChanged.AddLambda([&Changes](){++Changes;});
 auto* Widget=CreateWidget<UTALocationRevealWidget>(PC);
 auto Slate=Widget->TakeWidget();
 TestFalse(TEXT("Slate cannot focus"),Slate->SupportsKeyboardFocus());
 TestTrue(TEXT("Slate skips all descendants in hit testing"),Slate->GetVisibility()==EVisibility::HitTestInvisible);
 Widget->ShowLocationReveal(FText::FromString(TEXT("CHAPTER")),FText::FromString(TEXT("A")));
 Widget->AdvancePresentation(.4f);
 Widget->ShowLocationReveal(FText::FromString(TEXT("CHAPTER")),FText::FromString(TEXT("B")));
 Widget->AdvancePresentation(4.3f);
 TestEqual(TEXT("No Ownership transitions"),Changes,0);
 TestTrue(TEXT("Gameplay remains base owner"),PC->GetInputWinner().IsBase());
 PC->OnInputOwnerChanged.Remove(Handle); Widget->RemoveFromParent(); World->DestroyWorld(false);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTALocationRevealFontTest,"TheAwakening.LocationReveal.GlobalFontStyle",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTALocationRevealFontTest::RunTest(const FString&)
{
 auto* Font=LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/TA_GlobalFont.TA_GlobalFont"));
 if (!TestNotNull(TEXT("Global composite"),Font)) return false;
 const FTypefaceEntry* Found=Font->GetInternalCompositeFont().DefaultTypeface.Fonts.FindByPredicate([](const auto& E){return E.Name==TEXT("GoogleSansMedium");});
 if (!TestNotNull(TEXT("Named style is in existing composite"),Found)) return false;
 TestEqual(TEXT("Imported Google face"),GetPathNameSafe(Found->Font.GetFontFaceAsset()),FString(TEXT("/Game/Fonts/GoogleSans/GoogleSans_Medium.GoogleSans_Medium")));
 return true;
}
#endif
