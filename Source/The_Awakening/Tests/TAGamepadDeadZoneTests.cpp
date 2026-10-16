#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TAGamepadDeadZone.h"
#include "Settings/TASettingsTypes.h"
#include "Settings/TADeadZonePreviewWidget.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAGamepadDeadZoneMathTest,"TheAwakening.Input.DeadZone.Math",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAGamepadDeadZoneMathTest::RunTest(const FString&)
{
 const FTADeadZoneRange R{0.2f,0.1f};
 TestTrue(TEXT("Below min is ignored"),FTAGamepadDeadZone::IsIgnoredMagnitude(0.19f,R));
 TestFalse(TEXT("Min itself is not ignored"),FTAGamepadDeadZone::IsIgnoredMagnitude(0.2f,R));
 TestEqual(TEXT("Below min is zero"),FTAGamepadDeadZone::Remap(0.19f,R),0.f);
 TestEqual(TEXT("Min maps to zero"),FTAGamepadDeadZone::Remap(0.2f,R),0.f);
 TestEqual(TEXT("Halfway is linear"),FTAGamepadDeadZone::Remap(0.55f,R),0.5f,1e-4f);
 TestEqual(TEXT("Outer dead zone start maps to one"),FTAGamepadDeadZone::Remap(0.9f,R),1.f,1e-4f);
 TestEqual(TEXT("Beyond outer dead zone is one"),FTAGamepadDeadZone::Remap(0.95f,R),1.f);
 const FVector2D Stick=FTAGamepadDeadZone::RemapStick(FVector2D(0.33f,0.44f),R); // radius 0.55
 TestEqual(TEXT("Radial remap scales magnitude"),static_cast<float>(Stick.Size()),0.5f,1e-4f);
 TestEqual(TEXT("Radial remap keeps direction"),static_cast<float>(Stick.X/Stick.Y),0.75f,1e-4f);

 FTAGamepadDeadZone Zone; Zone.SetRange(ETADeadZoneInput::LeftStick,R); Zone.SetRange(ETADeadZoneInput::LeftTrigger,{0.05f,0.05f});
 bool bIgnored=false;
 TestEqual(TEXT("Drifting X alone is filtered out"),Zone.Filter(EKeys::Gamepad_LeftX,0.1f,bIgnored),0.f);
 TestTrue(TEXT("Drift is ignored"),bIgnored);
 TestFalse(TEXT("Drifting stick is inactive"),Zone.IsActive(ETADeadZoneInput::LeftStick));
 Zone.Filter(EKeys::Gamepad_LeftY,0.5f,bIgnored);
 TestFalse(TEXT("Combined radius leaves the dead zone"),bIgnored);
 TestTrue(TEXT("Paired axis follows the radial result"),Zone.GetFilteredAxis(EKeys::Gamepad_LeftX)>0.f);
 TestEqual(TEXT("Raw value kept for the preview"),static_cast<float>(Zone.GetRaw(ETADeadZoneInput::LeftStick).X),0.1f);
 TestEqual(TEXT("Small trigger press is ignored"),Zone.Filter(EKeys::Gamepad_LeftTriggerAxis,0.04f,bIgnored),0.f);
 TestTrue(TEXT("Trigger drift is ignored"),bIgnored);
 TestEqual(TEXT("Full trigger is one"),Zone.Filter(EKeys::Gamepad_LeftTriggerAxis,0.96f,bIgnored),1.f);
 TestEqual(TEXT("Other keys pass through"),Zone.Filter(EKeys::MouseX,0.01f,bIgnored),0.01f);
 TestFalse(TEXT("Other keys are never ignored"),bIgnored);

 ETADeadZoneInput Input;
 TestTrue(TEXT("Stick direction key maps to its stick"),FTAGamepadDeadZone::FindVirtualKeyInput(EKeys::Gamepad_RightStick_Up,Input) && Input==ETADeadZoneInput::RightStick);
 TestTrue(TEXT("Digital trigger maps to its trigger"),FTAGamepadDeadZone::FindVirtualKeyInput(EKeys::Gamepad_RightTrigger,Input) && Input==ETADeadZoneInput::RightTrigger);
 TestFalse(TEXT("Face buttons are not dead-zone keys"),FTAGamepadDeadZone::FindVirtualKeyInput(EKeys::Gamepad_FaceButton_Bottom,Input));
 Zone.Reset();
 TestTrue(TEXT("Reset clears raw input"),Zone.GetRaw(ETADeadZoneInput::LeftStick).IsZero());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAGamepadDeadZoneSettingsTest,"TheAwakening.Input.DeadZone.Settings",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAGamepadDeadZoneSettingsTest::RunTest(const FString&)
{
 const auto Definitions=FTASettingsCatalog::Build(GetTransientPackage());
 TArray<FName> Order;
 for (const auto& D:Definitions) if (D.SettingId.ToString().Contains(TEXT("DeadZone"))) Order.Add(D.SettingId);
 const TArray<FName> Expected={TEXT("Controller.LeftTriggerDeadZoneMin"),TEXT("Controller.LeftTriggerDeadZoneMax"),
  TEXT("Controller.LeftStickDeadZoneMin"),TEXT("Controller.LeftStickDeadZoneMax"),TEXT("Controller.RightStickDeadZoneMin"),
  TEXT("Controller.RightStickDeadZoneMax"),TEXT("Controller.RightTriggerDeadZoneMin"),TEXT("Controller.RightTriggerDeadZoneMax")};
 TestTrue(TEXT("Eight settings in preview order"),Order==Expected);
 for (const FName Id:Expected)
 {
  const auto* D=Definitions.FindByPredicate([Id](const auto& Item){return Item.SettingId==Id;});
  if (!TestNotNull(*Id.ToString(),D)) continue;
  TestTrue(*(Id.ToString()+TEXT(" valid")),D->IsValidDefinition());
  TestEqual(*(Id.ToString()+TEXT(" minimum 1%")),D->ToSliderNumber(1),0.01f,1e-4f);
  TestEqual(*(Id.ToString()+TEXT(" maximum 45%")),D->ToSliderNumber(100),0.45f,1e-4f);
  TestTrue(*(Id.ToString()+TEXT(" in dead zone section")),D->BelongsTo(TEXT("Settings.Page.Controller"),TEXT("Settings.Section.Controller.DeadZone")));
  TestTrue(*(Id.ToString()+TEXT(" shows the preview")),D->DescriptionBlocks.Num()==2 && D->DescriptionBlocks[1].Type==ETASettingDescriptionBlockType::DeadZonePreview);
  const bool bStickMin=Id.ToString().Contains(TEXT("Stick")) && Id.ToString().EndsWith(TEXT("Min"));
  TestEqual(*(Id.ToString()+TEXT(" default")),D->ToSliderNumber(D->DefaultValue),bStickMin?0.2f:0.05f,1e-4f);
  // Every press changes the value by exactly one percent.
  int32 Position=1; float Previous=D->ToSliderNumber(Position); bool bOnePercent=true; int32 Steps=0;
  while (Previous<0.45f-1e-4f && Steps++<100)
  {
   Position=D->StepSliderPosition(Position,1);
   const float Value=D->ToSliderNumber(Position);
   bOnePercent&=FMath::IsNearlyEqual(Value-Previous,0.01f,1e-3f); Previous=Value;
  }
  TestTrue(*(Id.ToString()+TEXT(" steps by 1%")),bOnePercent && Steps==44);
  TestEqual(*(Id.ToString()+TEXT(" stays at 45%")),D->ToSliderNumber(D->StepSliderPosition(Position,1)),0.45f,1e-4f);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAGamepadDeadZonePreviewTest,"TheAwakening.Input.DeadZone.Preview",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAGamepadDeadZonePreviewTest::RunTest(const FString&)
{
 using W=UTADeadZonePreviewWidget;
 const FTADeadZoneRange R{0.2f,0.1f};
 const auto Stick=W::ComputeState(ETADeadZoneInput::LeftStick,R,FVector2D(0,1),50.f);
 TestEqual(TEXT("Stick min zone is the min dead zone"),Stick.MinZone,0.2f);
 TestEqual(TEXT("Stick max zone is the max dead zone"),Stick.MaxZone,0.1f);
 TestTrue(TEXT("Stick up moves the dot up"),FMath::IsNearlyEqual(Stick.DotOffset.Y,-50.f) && FMath::IsNearlyZero(Stick.DotOffset.X));
 TestEqual(TEXT("Dot stays inside the circle"),W::ComputeState(ETADeadZoneInput::RightStick,R,FVector2D(2,0),50.f).DotOffset.X,50.f,1e-4f);
 TestEqual(TEXT("Sticks have no fill"),Stick.Fill,0.f);
 const auto Trigger=W::ComputeState(ETADeadZoneInput::RightTrigger,R,FVector2D(0.35,0),50.f);
 TestEqual(TEXT("Trigger fill is the raw travel"),Trigger.Fill,0.35f,1e-5f);
 TestTrue(TEXT("Triggers do not move a dot"),Trigger.DotOffset.IsZero());
 TestEqual(TEXT("Trigger zones use the same values"),Trigger.MinZone,0.2f);
 TestEqual(TEXT("Full trigger fills completely"),W::ComputeState(ETADeadZoneInput::LeftTrigger,R,FVector2D(1.4,0),50.f).Fill,1.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAGamepadDeadZoneInputTest,"TheAwakening.Input.DeadZone.Ingress",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTAGamepadDeadZoneInputTest::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 UClass* PCClass=LoadClass<AThe_AwakeningPlayerController>(nullptr,
  TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PC=PCClass?World->SpawnActor<AThe_AwakeningPlayerController>(PCClass):nullptr;
 if (!TestNotNull(TEXT("Controller"),PC) || !FSlateApplication::IsInitialized()) { World->DestroyWorld(false); return false; }
 auto* Local=NewObject<ULocalPlayer>(GEngine);
 Local->PlayerController=PC; PC->Player=Local;
 const int32 User=Local->GetControllerId();
 using TAInputOwnershipAdapter::EState;
 PC->GamepadDeadZone.SetRange(ETADeadZoneInput::LeftStick,{0.2f,0.05f});

 PC->ObserveAnalogInput(EKeys::Gamepad_LeftX,0.1f,User,EState::Owned);
 TestTrue(TEXT("Drift records no held stick"),PC->ReadHeldKey(EKeys::Gamepad_Left2D).IsNearlyZero());
 TestTrue(TEXT("Cursor reads no drift"),PC->ReadCursorStick(EKeys::Gamepad_Left2D).IsNearlyZero());
 PC->ObserveAnalogInput(EKeys::Gamepad_LeftY,0.6f,User,EState::Owned);
 const FVector Held=PC->ReadHeldKey(EKeys::Gamepad_Left2D);
 TestTrue(TEXT("Leaving the dead zone records both filtered axes"),Held.X>0.f && Held.Y>0.f && Held.Size()<0.6f);

 FTAInputDeviceDetector Processor(PC);
 PC->ObserveAnalogInput(EKeys::Gamepad_LeftY,0.f,User,EState::Owned);
 PC->ObserveAnalogInput(EKeys::Gamepad_LeftX,0.f,User,EState::Owned);
 if (PC->OwnsPlayerInput())
 {
  FSlateApplication& Slate=FSlateApplication::Get();
  PC->ObserveAnalogInput(EKeys::Gamepad_LeftY,0.15f,User,EState::Owned);
  const FKeyEvent Press(EKeys::Gamepad_LeftStick_Up,FModifierKeysState(),User,false,0,0);
  TestTrue(TEXT("Stick direction inside the dead zone is consumed"),Processor.HandleKeyDownEvent(Slate,Press));
  TestTrue(TEXT("Ignored press is remembered"),Processor.DeadZoneIgnoredPresses.Contains(EKeys::Gamepad_LeftStick_Up));
  TestTrue(TEXT("Ignored press is not held"),PC->ReadHeldKey(EKeys::Gamepad_LeftStick_Up).IsNearlyZero());
  PC->ObserveAnalogInput(EKeys::Gamepad_LeftY,0.7f,User,EState::Owned);
  const FKeyEvent Repeat(EKeys::Gamepad_LeftStick_Up,FModifierKeysState(),User,true,0,0);
  Processor.HandleKeyDownEvent(Slate,Repeat);
  TestTrue(TEXT("First repeat outside the dead zone is accepted"),Processor.DeadZoneAcceptedPresses.Contains(EKeys::Gamepad_LeftStick_Up));
  TestFalse(TEXT("Accepted press is no longer ignored"),Processor.DeadZoneIgnoredPresses.Contains(EKeys::Gamepad_LeftStick_Up));
  TestFalse(TEXT("Held key is recorded"),PC->ReadHeldKey(EKeys::Gamepad_LeftStick_Up).IsNearlyZero());
  const FKeyEvent Release(EKeys::Gamepad_LeftStick_Up,FModifierKeysState(),User,false,0,0);
  Processor.HandleKeyUpEvent(Slate,Release);
  TestFalse(TEXT("Release clears acceptance"),Processor.DeadZoneAcceptedPresses.Contains(EKeys::Gamepad_LeftStick_Up));
 }
 else AddWarning(TEXT("Controller does not own player input in this environment; virtual key gating not exercised."));
 PC->InvalidateGamepadObservationOnDisconnect();
 TestTrue(TEXT("Disconnect clears raw dead-zone input"),PC->GetGamepadDeadZone().GetRaw(ETADeadZoneInput::LeftStick).IsZero());
 World->DestroyWorld(false);
 return true;
}
#endif
