#pragma once
#include "CoreMinimal.h"

/** Visual return and a fresh uninterrupted hold are independent timelines. */
struct FTASettingsResetHold
{
 float HoldSeconds=0;
 float ReturnSeconds=1;
 float ReturnStart=0;
 float Display=0;
 bool bHeld=false;
 bool bTriggered=false;

 bool Tick(float DeltaTime,bool Held,float HoldDuration=3.f,float ReturnDuration=1.f)
 {
  HoldDuration=FMath::IsFinite(HoldDuration)?FMath::Max(0.01f,HoldDuration):3.f;
  ReturnDuration=FMath::IsFinite(ReturnDuration)?FMath::Max(0.01f,ReturnDuration):1.f;
  DeltaTime=FMath::Max(0.f,DeltaTime);
  if (!Held && bHeld)
  {
   // Completion already started its return; releasing must not restart it.
   if (!bTriggered) { ReturnStart=Display; ReturnSeconds=0; }
   HoldSeconds=0; bTriggered=false;
  }
  if (Held && !bHeld) { HoldSeconds=0; bTriggered=false; }
  bHeld=Held;
  ReturnSeconds=FMath::Min(ReturnDuration,ReturnSeconds+DeltaTime);
  // Ease-out cubic: fast at release, slowing towards zero.
  const float Returning=ReturnStart*FMath::Pow(1.f-ReturnSeconds/ReturnDuration,3.f);
  if (Held) HoldSeconds+=DeltaTime;
  Display=FMath::Max(Returning,Held && !bTriggered?FMath::Clamp(HoldSeconds/HoldDuration,0.f,1.f):0.f);
  if (Held && HoldSeconds>=HoldDuration && !bTriggered)
  {
   bTriggered=true; ReturnStart=1.f; ReturnSeconds=0; Display=1.f;
   return true;
  }
  return false;
 }
};
