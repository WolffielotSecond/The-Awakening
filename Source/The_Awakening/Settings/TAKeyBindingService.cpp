#include "Settings/TAKeyBindingService.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Engine/LocalPlayer.h"
#include "UObject/UnrealType.h"

namespace
{
 FName Group(const FString& Action,const FString& Context)
 {
  if (Context!=TEXT("IMC_UI")) return TEXT("Gameplay");
  if (Action.Contains(TEXT("Inventory"))) return TEXT("Inventory");
  if (Action.Contains(TEXT("Puzzle"))) return TEXT("Puzzle");
  if (Action.Contains(TEXT("Dialogue"))) return TEXT("Dialogue");
  return TEXT("Menu");
 }
}
bool UTAKeyBindingService::GroupsOverlap(FName A,FName B)
{
 if (A==B) return true;
 return (A==TEXT("Menu") && B!=TEXT("Gameplay")) || (B==TEXT("Menu") && A!=TEXT("Gameplay"));
}
UInputMappingContext* UTAKeyBindingService::PrepareContext(UInputMappingContext* Source)
{
 if (!Source) return nullptr;
 if (auto* Existing=RuntimeContexts.Find(Source)) return *Existing;
 auto* Input=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
 auto* User=Input?Input->GetUserSettings():nullptr;
 if (!User) return Source;
 // A controller recreated during travel supplies a new UI source with the same
 // logical name. Retire its old transient copies without growing the registry.
 for (auto It=RuntimeContexts.CreateIterator();It;++It)
  if (It.Key() && It.Key()->GetFName()==Source->GetFName())
  { User->UnregisterInputMappingContext(It.Value()); It.RemoveCurrent(); }
 auto* Runtime=BuildRuntimeContext(Source,this,Bindings);
 RuntimeContexts.Add(Source,Runtime);
 User->RegisterInputMappingContext(Runtime);
 return Runtime;
}
UInputMappingContext* UTAKeyBindingService::BuildRuntimeContext(UInputMappingContext* Source,UObject* Outer,TArray<FTAKeyBindingDefinition>& OutBindings)
{
 if (!Source || !Outer) return nullptr;
 // Per-player copies retain authored actions, modifiers and triggers; assets are never mutated.
 auto* Runtime=DuplicateObject<UInputMappingContext>(Source,Outer,MakeUniqueObjectName(Outer,Source->GetClass(),Source->GetFName()));
 for (int32 I=0;I<Runtime->GetMappings().Num();++I)
 {
  auto& M=Runtime->GetMapping(I);
  if (!M.Action || M.Key.IsAnalog() || !M.Key.IsValid() || M.Action->GetName().Contains(TEXT("Debug"))) continue;
  const FString Device=M.Key.IsGamepadKey()?TEXT("Controller."):TEXT("MouseKeyboard.");
  const FString Id=Device+Source->GetName()+TEXT(".")+M.Action->GetName()+TEXT(".")+M.Key.GetFName().ToString();
  auto* Metadata=NewObject<UPlayerMappableKeySettings>(Runtime);
  Metadata->Name=FName(*Id); Metadata->DisplayName=FText::FromString(M.Action->GetName());
  // UE exposes these reflected fields as protected; set them on the transient copy only.
  auto* Behavior=FindFProperty<FEnumProperty>(FEnhancedActionKeyMapping::StaticStruct(),TEXT("SettingBehavior"));
  auto* Settings=FindFProperty<FObjectPropertyBase>(FEnhancedActionKeyMapping::StaticStruct(),TEXT("PlayerMappableKeySettings"));
  if (!Behavior || !Settings) continue;
  Behavior->GetUnderlyingProperty()->SetIntPropertyValue(Behavior->ContainerPtrToValuePtr<void>(&M),static_cast<int64>(EPlayerMappableKeySettingBehaviors::OverrideSettings));
  Settings->SetObjectPropertyValue_InContainer(&M,Metadata);
  FTAKeyBindingDefinition D; D.BindingId=Metadata->Name; D.MappingName=D.BindingId; D.Action=M.Action;
  D.NameTextId=TEXT("Settings.Binding.")+M.Action->GetName(); D.bController=M.Key.IsGamepadKey(); D.DefaultKey=M.Key; D.ConflictGroup=Group(M.Action->GetName(),Source->GetName());
  if (auto* Existing=OutBindings.FindByPredicate([&](const auto& B){return B.BindingId==D.BindingId;})) *Existing=D;
  else OutBindings.Add(D);
 }
 return Runtime;
}
FKey UTAKeyBindingService::GetKey(FName Id) const
{
 const auto* D=Bindings.FindByPredicate([Id](const auto& B){return B.BindingId==Id;}); if (!D) return EKeys::Invalid;
 const auto* Input=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
 const auto* User=Input?Input->GetUserSettings():nullptr;
 const auto* Profile=User?User->GetActiveKeyProfile():nullptr;
 const auto* M=Profile?Profile->FindKeyMappingRow(D->MappingName):nullptr;
 if (M) for (const auto& K:M->Mappings) return K.GetCurrentKey();
 return D->DefaultKey;
}
bool UTAKeyBindingService::Rebind(FName Id,FKey Key,FString& Error)
{
 const auto* D=Bindings.FindByPredicate([Id](const auto& B){return B.BindingId==Id;});
 if (!D || !Key.IsValid() || Key.IsAnalog() || Key.IsGamepadKey()!=D->bController)
 { Error=TEXT("Settings.Binding.InvalidDevice"); return false; }
 if (GetKey(Id)==Key) return true;
 for (const auto& B:Bindings)
  if (B.BindingId!=Id && B.bController==D->bController && GroupsOverlap(D->ConflictGroup,B.ConflictGroup) && GetKey(B.BindingId)==Key)
  { Error=TEXT("Settings.Binding.Conflict"); return false; }
 auto* Input=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(); auto* User=Input?Input->GetUserSettings():nullptr;
 if (!User) { Error=TEXT("Settings.Binding.Unavailable"); return false; }
 FMapPlayerKeyArgs Args; Args.MappingName=D->MappingName; Args.Slot=EPlayerMappableKeySlot::First; Args.NewKey=Key;
 if (const auto* Profile=User->GetActiveKeyProfile())
  if (const auto* Row=Profile->FindKeyMappingRow(D->MappingName))
   for (const auto& Mapping:Row->Mappings)
   { Args.Slot=Mapping.GetSlot(); Args.HardwareDeviceId=Mapping.GetHardwareDeviceId().HardwareDeviceIdentifier; break; }
 FGameplayTagContainer Failure; User->MapPlayerKey(Args,Failure);
 if (!Failure.IsEmpty()) { Error=TEXT("Settings.Binding.Failed"); return false; }
 RebuildAndSave(); return true;
}
void UTAKeyBindingService::RestoreDeviceDefaults(bool Controller)
{
 auto* Input=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(); auto* User=Input?Input->GetUserSettings():nullptr;
 if (!User || !User->GetActiveKeyProfile()) return;
 for (const auto& D:Bindings) if (D.bController==Controller) User->GetActiveKeyProfile()->ResetMappingToDefault(D.MappingName);
 RebuildAndSave();
}
void UTAKeyBindingService::RebuildAndSave()
{
 auto* Input=GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(); if (!Input || !Input->GetUserSettings()) return;
 Input->GetUserSettings()->ApplySettings(); Input->GetUserSettings()->SaveSettings();
 FModifyContextOptions O; O.bForceImmediately=true; O.bIgnoreAllPressedKeysUntilRelease=true;
 Input->RequestRebuildControlMappings(O); OnBindingsChanged.Broadcast();
}
