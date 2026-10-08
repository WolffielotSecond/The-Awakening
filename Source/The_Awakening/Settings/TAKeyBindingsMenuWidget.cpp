#include "Settings/TAKeyBindingsMenuWidget.h"
#include "Settings/TAKeyBindingService.h"
#include "Settings/TASettingRowWidget.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
bool UTAKeyBindingsMenuWidget::IsControllerMenu() const { return MenuDefinition && MenuDefinition->MenuKind==ETASettingSubmenuTarget::ControllerKeyBindings; }
void UTAKeyBindingsMenuWidget::NativeConstruct()
{
 if (GetOwningLocalPlayer()) BindingService=GetOwningLocalPlayer()->GetSubsystem<UTAKeyBindingService>();
 Super::NativeConstruct();
 if (BindingService) BindingService->OnBindingsChanged.AddUniqueDynamic(this,&UTAKeyBindingsMenuWidget::BindingsChanged);
 if (InputController.IsValid()) OwnershipLostHandle=InputController->OnPlayerInputOwnershipLost.AddUObject(this,&UTAKeyBindingsMenuWidget::CancelCapture);
 if (Button_CancelCapture) Button_CancelCapture->OnClicked.AddUniqueDynamic(this,&UTAKeyBindingsMenuWidget::CancelCapture);
}
void UTAKeyBindingsMenuWidget::NativeDestruct()
{
 if (BindingService) BindingService->OnBindingsChanged.RemoveDynamic(this,&UTAKeyBindingsMenuWidget::BindingsChanged);
 if (InputController.IsValid()) InputController->OnPlayerInputOwnershipLost.Remove(OwnershipLostHandle);
 CapturingId=NAME_None; Super::NativeDestruct();
}
void UTAKeyBindingsMenuWidget::BuildSettingRows()
{
 if (!Box_SettingsOptions) return; Box_SettingsOptions->ClearChildren(); Rows.Reset();
 if (BindingService) for(const auto& B:BindingService->GetBindings()) if(B.bController==IsControllerMenu())
 {
  FTASettingDefinition D; D.SettingId=B.BindingId; D.Type=ETASettingType::Submenu; D.NameTextId=B.NameTextId; D.bCanFavorite=false;
  AddRow(D,BindingService->GetKey(B.BindingId).GetDisplayName());
 }
 if (!Rows.ContainsByPredicate([&](const auto& R){return R->GetSettingId()==SelectedSettingId;})) SelectedSettingId=Rows.IsEmpty()?NAME_None:Rows[0]->GetSettingId();
 RefreshRows();
}
void UTAKeyBindingsMenuWidget::RefreshRows()
{
 EnsureRowSelection();
 if (BindingService) for (UTASettingRowWidget* R:Rows)
 {
  const auto* B=BindingService->GetBindings().FindByPredicate([&](const auto& D){return D.BindingId==R->GetSettingId();});
  if (!B) continue; FTASettingDefinition D; D.SettingId=B->BindingId; D.Type=ETASettingType::Submenu; D.NameTextId=B->NameTextId; D.bCanFavorite=false;
  R->Configure(D,Text(B->NameTextId),BindingService->GetKey(B->BindingId).GetDisplayName(),0,false); R->SetHighlighted(B->BindingId==SelectedSettingId);
 }
 FTASettingDefinition Details; Details.DescriptionTextId=TEXT("Settings.Binding.Description");
 if (BindingService) if (const auto* B=BindingService->GetBindings().FindByPredicate([&](const auto& Binding){return Binding.BindingId==SelectedSettingId;})) Details.NameTextId=B->NameTextId;
 RefreshSettingDetails(SelectedSettingId.IsNone()?nullptr:&Details);
 if (Text_CaptureStatus) Text_CaptureStatus->SetText(StatusTextId.IsEmpty()?FText::GetEmpty():Text(StatusTextId));
 if (Button_CancelCapture) Button_CancelCapture->SetVisibility(IsCapturingPlayerInput()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UTAKeyBindingsMenuWidget::ConfirmSelection()
{
 if (!BindingService || IsCapturingPlayerInput() || SelectedSettingId.IsNone()) return;
 CapturingId=SelectedSettingId; StatusTextId=TEXT("Settings.Binding.PressKey"); RefreshRows();
}
bool UTAKeyBindingsMenuWidget::CapturePlayerInput(FKey Key,bool Repeat)
{
 if (!IsCapturingPlayerInput() || !Allows(ETAInputCapability::CaptureBinding)) return false;
 if (Repeat) return true;
 if (Key==EKeys::LeftMouseButton && Button_CancelCapture && Button_CancelCapture->IsVisible() &&
  FSlateApplication::IsInitialized() && Button_CancelCapture->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos()))
 { CancelCapture(); return true; }
 // A dedicated cancellation key stays available even when the menu mappings change.
 if (Key==EKeys::Escape || Key==EKeys::Gamepad_Special_Right) { CancelCapture(); return true; }
 if (!BindingService) { CancelCapture(); return true; }
 FString Error; const FName Id=CapturingId;
 if (BindingService->Rebind(Id,Key,Error)) { CapturingId=NAME_None; StatusTextId=TEXT("Settings.Binding.Success"); }
 else StatusTextId=Error;
 RefreshRows(); return true;
}
void UTAKeyBindingsMenuWidget::CancelCapture() { CapturingId=NAME_None; StatusTextId.Empty(); RefreshRows(); }
bool UTAKeyBindingsMenuWidget::HandleMenuBackRequested()
{
 if (!Allows(ETAInputCapability::Close)) return false;
 if (IsCapturingPlayerInput()) { CancelCapture(); return true; }
 return Super::HandleMenuBackRequested();
}
void UTAKeyBindingsMenuWidget::RestoreCurrentDefaults() { if(BindingService && !IsCapturingPlayerInput()) BindingService->RestoreDeviceDefaults(IsControllerMenu()); }
void UTAKeyBindingsMenuWidget::BindingsChanged() { RefreshRows(); }
