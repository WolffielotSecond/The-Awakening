#include "Settings/TASettingsMenuWidget.h"
#include "Settings/TASettingRowWidget.h"
#include "Settings/TASettingsSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

UTASettingsMenuWidget::UTASettingsMenuWidget(const FObjectInitializer& O):Super(O)
{
 SetIsFocusable(true);
 static ConstructorHelpers::FObjectFinder<UInputAction> Prev(TEXT("/Game/Input/Actions/IA_ChoicePrevious"));
 static ConstructorHelpers::FObjectFinder<UInputAction> Next(TEXT("/Game/Input/Actions/IA_ChoiceNext"));
 static ConstructorHelpers::FObjectFinder<UInputAction> Confirm(TEXT("/Game/Input/Actions/IA_ChoiceConfirm"));
 PreviousAction=Prev.Object; NextAction=Next.Object; ConfirmAction=Confirm.Object;
}
void UTASettingsMenuWidget::InitializeMenu(AThe_AwakeningPlayerController* PC,UTASettingsMenuWidget* Parent,UTASettingsMenuDefinitionAsset* Menu)
{ InputController=PC; ParentMenu=Parent; MenuDefinition=Menu; }
void UTASettingsMenuWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (!InputController.IsValid()) InputController=Cast<AThe_AwakeningPlayerController>(GetOwningPlayer());
 if (GetOwningLocalPlayer()) SettingsSubsystem=GetOwningLocalPlayer()->GetSubsystem<UTASettingsSubsystem>();
 if (SettingsSubsystem) { SettingsSubsystem->OnSettingValueChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::ValueChanged); SettingsSubsystem->OnFavoritesChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::FavoritesChanged); }
 if (auto* GI=GetGameInstance()) if (auto* Loc=GI->GetSubsystem<UTALocalizeSubsystem>()) Loc->OnLanguageChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::LanguageChanged);
 if (Button_Game) Button_Game->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::GamePage);
 if (Button_Display) Button_Display->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::DisplayPage);
 if (Button_Audio) Button_Audio->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::AudioPage);
 if (Button_MouseKeyboard) Button_MouseKeyboard->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::MousePage);
 if (Button_Controller) Button_Controller->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::ControllerPage);
 if (Button_Favorites) Button_Favorites->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::FavoritesPage);
 if (Button_RestoreDefaults) Button_RestoreDefaults->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::RestoreClicked);
 if (Button_ConfirmVideoMode) Button_ConfirmVideoMode->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::ConfirmVideo);
 if (Button_RevertVideoMode) Button_RevertVideoMode->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::RevertVideo);
 if (MenuDefinition)
  for (UButton* B:{Button_Game.Get(),Button_Display.Get(),Button_Audio.Get(),Button_MouseKeyboard.Get(),Button_Controller.Get(),Button_Favorites.Get()}) if (B) B->SetVisibility(ESlateVisibility::Collapsed);
 RefreshPageLabels(); BuildSettingRows(); BuildPromptBar();
}
void UTASettingsMenuWidget::NativeDestruct()
{
 ReleaseRequest();
 if (SettingsSubsystem) { SettingsSubsystem->OnSettingValueChanged.RemoveDynamic(this,&UTASettingsMenuWidget::ValueChanged); SettingsSubsystem->OnFavoritesChanged.RemoveDynamic(this,&UTASettingsMenuWidget::FavoritesChanged); }
 if (auto* GI=GetGameInstance()) if (auto* Loc=GI->GetSubsystem<UTALocalizeSubsystem>()) Loc->OnLanguageChanged.RemoveDynamic(this,&UTASettingsMenuWidget::LanguageChanged);
 if (ActiveSubmenu) { auto* Child=ActiveSubmenu.Get(); ActiveSubmenu=nullptr; Child->RemoveFromParent(); }
 if (!ParentMenu.IsValid() && SettingsSubsystem) SettingsSubsystem->RevertVideoMode();
 Super::NativeDestruct();
}
void UTASettingsMenuWidget::ReleaseRequest()
{
 const auto H=InputRequestHandle; InputRequestHandle=0;
 if (H && InputController.IsValid()) InputController->ReleaseInputRequest(H);
}
void UTASettingsMenuWidget::RemoveFromParent()
{
 if (ParentMenu.IsValid() && ParentMenu->ActiveSubmenu==this) ParentMenu->ActiveSubmenu=nullptr;
 if (ActiveSubmenu) { auto* Child=ActiveSubmenu.Get(); ActiveSubmenu=nullptr; Child->RemoveFromParent(); }
 if (!ParentMenu.IsValid() && SettingsSubsystem) SettingsSubsystem->RevertVideoMode();
 ReleaseRequest(); Super::RemoveFromParent();
}
void UTASettingsMenuWidget::SetPlayerInputRequest(AThe_AwakeningPlayerController* PC,FTAInputRouter::FHandle H) { InputController=PC; InputRequestHandle=H; }
bool UTASettingsMenuWidget::Allows(ETAInputCapability C) const { return InputController.IsValid() && InputController->AllowsInputFor(InputRequestHandle,this,C); }
UWidget* UTASettingsMenuWidget::GetInitialFocusTarget() const { return const_cast<UTASettingsMenuWidget*>(this); }
FText UTASettingsMenuWidget::Text(const FString& Id) const
{
 if (auto* GI=GetGameInstance()) if (auto* L=GI->GetSubsystem<UTALocalizeSubsystem>()) return L->GetText(Id);
 return FText::FromString(Id);
}
UTASettingRowWidget* UTASettingsMenuWidget::AddRow(const FTASettingDefinition& D,FText Value)
{
 auto Class=OptionWidgetClass; if (const auto* Specific=RowWidgetClasses.Find(D.Type);Specific && *Specific) Class=*Specific;
 if (!Box_SettingsOptions || !Class) return nullptr;
 auto* W=CreateWidget<UTASettingRowWidget>(this,Class); if (!W) return nullptr;
 W->Configure(D,Text(D.NameTextId),Value,SettingsSubsystem?SettingsSubsystem->GetNumber(D.SettingId,0):0,SettingsSubsystem && SettingsSubsystem->IsFavorite(D.SettingId));
 W->OnHovered.AddDynamic(this,&UTASettingsMenuWidget::RowHovered);
 W->OnActivated.AddDynamic(this,&UTASettingsMenuWidget::RowActivated);
 W->OnAdjusted.AddDynamic(this,&UTASettingsMenuWidget::RowAdjusted);
 W->OnNumberChanged.AddDynamic(this,&UTASettingsMenuWidget::RowNumberChanged);
 W->OnFavorite.AddDynamic(this,&UTASettingsMenuWidget::RowFavorite);
 Box_SettingsOptions->AddChild(W); Rows.Add(W); return W;
}
void UTASettingsMenuWidget::BuildSettingRows()
{
 if (!Box_SettingsOptions) return; Box_SettingsOptions->ClearChildren(); Rows.Reset();
 if (SettingsSubsystem) for (const auto& D:SettingsSubsystem->GetDefinitions())
 {
  bool Show=MenuDefinition?MenuDefinition->SettingIds.Contains(D.SettingId):!D.bSubmenuOnly && (CurrentPage==ETAGameSettingsPage::Favorites?SettingsSubsystem->IsFavorite(D.SettingId):D.Page==CurrentPage);
  if (Show) AddRow(D,SettingsSubsystem->FormatValue(D));
 }
 if (!Rows.ContainsByPredicate([&](const auto& R){return R->GetSettingId()==SelectedSettingId;})) SelectedSettingId=Rows.IsEmpty()?NAME_None:Rows[0]->GetSettingId();
 if (Text_Empty) { Text_Empty->SetText(Text(CurrentPage==ETAGameSettingsPage::Audio?TEXT("Settings.Empty.Audio"):TEXT("Settings.Empty"))); Text_Empty->SetVisibility(Rows.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed); }
 RefreshRows();
}
void UTASettingsMenuWidget::RefreshRows()
{
 for (UTASettingRowWidget* R:Rows) if (R)
 {
  if (SettingsSubsystem) if (const auto* D=SettingsSubsystem->FindDefinition(R->GetSettingId())) R->Configure(*D,Text(D->NameTextId),SettingsSubsystem->FormatValue(*D),SettingsSubsystem->GetNumber(D->SettingId,0),SettingsSubsystem->IsFavorite(D->SettingId));
  R->SetHighlighted(R->GetSettingId()==SelectedSettingId);
 }
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindDefinition(SelectedSettingId):nullptr;
 if (Text_Description) Text_Description->SetText(D?Text(D->DescriptionTextId):FText::GetEmpty());
 const bool Pending=SettingsSubsystem && SettingsSubsystem->HasPendingVideoMode();
 if (Button_ConfirmVideoMode) Button_ConfirmVideoMode->SetVisibility(Pending?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if (Button_RevertVideoMode) Button_RevertVideoMode->SetVisibility(Pending?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UTASettingsMenuWidget::SelectPage(ETAGameSettingsPage P)
{ if (MenuDefinition || !Allows(ETAInputCapability::Navigate)) return; CurrentPage=P; SelectedSettingId=NAME_None; BuildSettingRows(); RefreshPageLabels(); }
void UTASettingsMenuWidget::MovePage(int32 Dir)
{ if (!MenuDefinition) SelectPage(static_cast<ETAGameSettingsPage>((static_cast<int32>(CurrentPage)+Dir+6)%6)); }
void UTASettingsMenuWidget::MoveSelection(int32 Dir)
{
 if (Rows.IsEmpty()) return;
 int32 I=Rows.IndexOfByPredicate([&](const auto& R){return R->GetSettingId()==SelectedSettingId;});
 I=I==INDEX_NONE?0:(I+Dir+Rows.Num())%Rows.Num(); SelectedSettingId=Rows[I]->GetSettingId(); RefreshRows();
}
void UTASettingsMenuWidget::AdjustSelectedValue(int32 Dir) { if (SettingsSubsystem) SettingsSubsystem->AdjustValue(SelectedSettingId,Dir); }
void UTASettingsMenuWidget::ConfirmSelection()
{
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindDefinition(SelectedSettingId):nullptr; if (!D) return;
 if (D->Type==ETASettingType::Submenu) OpenSubmenu(*D);
 else if (D->Type==ETASettingType::Toggle || D->Type==ETASettingType::Choice) SettingsSubsystem->AdjustValue(D->SettingId,1);
}
void UTASettingsMenuWidget::OpenSubmenu(const FTASettingDefinition& D)
{
 if (!InputController.IsValid() || ActiveSubmenu || !D.TargetMenuDefinition) return;
 auto Class=D.TargetMenuDefinition->MenuKind==ETASettingSubmenuTarget::Brightness?BrightnessMenuWidgetClass:KeyBindingsMenuWidgetClass;
 if (!Class) { UE_LOG(LogTemp,Error,TEXT("Configure Settings submenu Blueprint class for %s"),*D.SettingId.ToString()); return; }
 auto* Child=CreateWidget<UTASettingsMenuWidget>(InputController.Get(),Class); if (!Child) return;
 Child->InitializeMenu(InputController.Get(),this,D.TargetMenuDefinition); Child->AddToViewport(1100);
 FTAInputRequest Request; Request.Owner=Child; Request.Priority=610;
 Request.Allowed={ETAInputCapability::Navigate,ETAInputCapability::Cursor,ETAInputCapability::Confirm,ETAInputCapability::Close,ETAInputCapability::ToggleFavorite,ETAInputCapability::CaptureBinding};
 Request.Presentation.InputMode=ETAInputModeRequirement::GameAndUI; Request.Presentation.bShowCursor=true;
 Request.Presentation.Focus=ETAInputFocusRequirement::Target; Request.Presentation.FocusTarget=Child->GetInitialFocusTarget();
 const auto H=InputController->AcquireInputRequest(Request); if (!H) { Child->RemoveFromParent(); return; }
 Child->SetPlayerInputRequest(InputController.Get(),H); ActiveSubmenu=Child;
}
bool UTASettingsMenuWidget::HandleMenuBackRequested()
{
 if (!Allows(ETAInputCapability::Close)) return false;
 if (ParentMenu.IsValid()) { ParentMenu->ActiveSubmenu=nullptr; RemoveFromParent(); return true; }
 if (InputController.IsValid()) { InputController->CloseSettingsMenu(); return true; } return false;
}
void UTASettingsMenuWidget::RowHovered(UTASelectableMenuOptionWidget* W)
{ if (Allows(ETAInputCapability::Navigate)) if (auto* R=Cast<UTASettingRowWidget>(W)) { SelectedSettingId=R->GetSettingId(); RefreshRows(); } }
void UTASettingsMenuWidget::RowActivated(FName Id) { if (IsCapturingPlayerInput() || !Allows(ETAInputCapability::Confirm)) return; SelectedSettingId=Id; ConfirmSelection(); RefreshRows(); }
void UTASettingsMenuWidget::RowAdjusted(FName Id,int32 Dir) { if (!Allows(ETAInputCapability::Navigate)) return; SelectedSettingId=Id; AdjustSelectedValue(Dir); }
void UTASettingsMenuWidget::RowNumberChanged(FName Id,float N) { if (Allows(ETAInputCapability::Navigate) && SettingsSubsystem) SettingsSubsystem->SetValue(Id,FTASettingValue::Numeric(N)); }
void UTASettingsMenuWidget::RowFavorite(FName Id) { if (Allows(ETAInputCapability::ToggleFavorite) && SettingsSubsystem) SettingsSubsystem->SetFavorite(Id,!SettingsSubsystem->IsFavorite(Id)); }
void UTASettingsMenuWidget::ValueChanged(FName) { RefreshRows(); }
void UTASettingsMenuWidget::FavoritesChanged() { if (CurrentPage==ETAGameSettingsPage::Favorites) BuildSettingRows(); else RefreshRows(); }
void UTASettingsMenuWidget::LanguageChanged() { RefreshPageLabels(); RefreshRows(); }
void UTASettingsMenuWidget::RefreshPageLabels()
{
 UTextBlock* Labels[]={Text_Game,Text_Display,Text_Audio,Text_MouseKeyboard,Text_Controller,Text_Favorites};
 UButton* Buttons[]={Button_Game,Button_Display,Button_Audio,Button_MouseKeyboard,Button_Controller,Button_Favorites};
 const TCHAR* Names[]={TEXT("Game"),TEXT("Display"),TEXT("Audio"),TEXT("MouseKeyboard"),TEXT("Controller"),TEXT("Favorites")};
 for(int32 I=0;I<6;++I) { if (Labels[I]) Labels[I]->SetText(Text(FString(TEXT("Settings.Page."))+Names[I])); if (Buttons[I]) Buttons[I]->SetBackgroundColor(I==static_cast<int32>(CurrentPage)?FLinearColor(0.12f,0.42f,0.82f,1):FLinearColor::White); }
 if (Text_Title) Text_Title->SetText(Text(MenuDefinition?MenuDefinition->TitleTextId:TEXT("UI_Settings")));
}
void UTASettingsMenuWidget::BuildPromptBar()
{
 if (!HorizontalBox_Controls || !Prompts.IsEmpty() || !InputController.IsValid()) return;
 auto Add=[&](UInputAction* A,const TCHAR* Id) { if (auto* P=FTAPromptWidgetUtils::AddActionPrompt(this,HorizontalBox_Controls,ActionPromptWidgetClass,A,Id)) { P->OnPromptClicked.AddDynamic(this,&UTASettingsMenuWidget::PromptClicked); Prompts.Add(P); } };
 Add(PreviousAction,TEXT("UI_Settings_Previous")); Add(NextAction,TEXT("UI_Settings_Next")); Add(ConfirmAction,TEXT("UI_Settings_Confirm"));
 const bool BindingMenu=MenuDefinition && (MenuDefinition->MenuKind==ETASettingSubmenuTarget::KeyBindings || MenuDefinition->MenuKind==ETASettingSubmenuTarget::ControllerKeyBindings);
 if (!BindingMenu) for (const TCHAR* Name:{TEXT("AdjustLeft"),TEXT("AdjustRight")}) Add(InputController->GetSettingsAction(Name),*(FString(TEXT("UI_Settings_"))+Name));
 if (!MenuDefinition) for (const TCHAR* Name:{TEXT("PreviousPage"),TEXT("NextPage"),TEXT("Favorite")}) Add(InputController->GetSettingsAction(Name),*(FString(TEXT("UI_Settings_"))+Name));
 Add(InputController->GetUIBackAction(),TEXT("UI_Settings_Back"));
}
void UTASettingsMenuWidget::PromptClicked(UTAActionPromptWidget* P)
{
 if (!P || !InputController.IsValid()) return;
 auto* A=P->GetPromptAction();
 if (A==InputController->GetUIBackAction()) { HandleMenuBackRequested(); return; }
 if (IsCapturingPlayerInput()) return;
 if (A==ConfirmAction) { if (Allows(ETAInputCapability::Confirm)) ConfirmSelection(); return; }
 if (A==InputController->GetSettingsAction(TEXT("Favorite"))) { RowFavorite(SelectedSettingId); return; }
 if (!Allows(ETAInputCapability::Navigate)) return;
 if (A==PreviousAction) MoveSelection(-1); else if (A==NextAction) MoveSelection(1);
 else if (A==InputController->GetSettingsAction(TEXT("AdjustLeft"))) AdjustSelectedValue(-1);
 else if (A==InputController->GetSettingsAction(TEXT("AdjustRight"))) AdjustSelectedValue(1);
 else if (A==InputController->GetSettingsAction(TEXT("PreviousPage"))) MovePage(-1);
 else if (A==InputController->GetSettingsAction(TEXT("NextPage"))) MovePage(1);
}
TOptional<ETAInputCapability> UTASettingsMenuWidget::ResolvePlayerInput(FKey K) const
{
 if (!InputController.IsValid() || K.IsMouseButton()) return {};
 auto M=[&](const UInputAction* A){return InputController->IsKeyMappedToAction(K,A);};
 if (M(ConfirmAction)) return ETAInputCapability::Confirm;
 if (M(InputController->GetSettingsAction(TEXT("Favorite")))) return ETAInputCapability::ToggleFavorite;
 if (M(PreviousAction) || M(NextAction)) return ETAInputCapability::Navigate;
 for (const TCHAR* N:{TEXT("AdjustLeft"),TEXT("AdjustRight"),TEXT("PreviousPage"),TEXT("NextPage")}) if (M(InputController->GetSettingsAction(N))) return ETAInputCapability::Navigate;
 return {};
}
void UTASettingsMenuWidget::ExecutePlayerInput(FKey K,ETAInputCapability C)
{
 if (!Allows(C)) return;
 if (C==ETAInputCapability::Confirm) { ConfirmSelection(); return; }
 if (C==ETAInputCapability::ToggleFavorite) { RowFavorite(SelectedSettingId); return; }
 if (C!=ETAInputCapability::Navigate) return;
 if (InputController->IsKeyMappedToAction(K,PreviousAction)) MoveSelection(-1);
 else if (InputController->IsKeyMappedToAction(K,NextAction)) MoveSelection(1);
 else if (InputController->IsKeyMappedToAction(K,InputController->GetSettingsAction(TEXT("AdjustLeft")))) AdjustSelectedValue(-1);
 else if (InputController->IsKeyMappedToAction(K,InputController->GetSettingsAction(TEXT("AdjustRight")))) AdjustSelectedValue(1);
 else if (InputController->IsKeyMappedToAction(K,InputController->GetSettingsAction(TEXT("PreviousPage")))) MovePage(-1);
 else MovePage(1);
}
void UTASettingsMenuWidget::RestoreCurrentDefaults()
{
 if (!SettingsSubsystem) return;
 TArray<FName> Ids; for (const UTASettingRowWidget* R:Rows) Ids.Add(R->GetSettingId());
 for(FName Id:Ids) SettingsSubsystem->RestoreSettingDefault(Id);
}
void UTASettingsMenuWidget::RestoreClicked() { if (Allows(ETAInputCapability::Confirm)) RestoreCurrentDefaults(); }
void UTASettingsMenuWidget::ConfirmVideo() { if (Allows(ETAInputCapability::Confirm) && SettingsSubsystem) SettingsSubsystem->ConfirmVideoMode(); }
void UTASettingsMenuWidget::RevertVideo() { if (Allows(ETAInputCapability::Confirm) && SettingsSubsystem) SettingsSubsystem->RevertVideoMode(); }
void UTASettingsMenuWidget::GamePage() { SelectPage(ETAGameSettingsPage::Game); }
void UTASettingsMenuWidget::DisplayPage() { SelectPage(ETAGameSettingsPage::Display); }
void UTASettingsMenuWidget::AudioPage() { SelectPage(ETAGameSettingsPage::Audio); }
void UTASettingsMenuWidget::MousePage() { SelectPage(ETAGameSettingsPage::MouseKeyboard); }
void UTASettingsMenuWidget::ControllerPage() { SelectPage(ETAGameSettingsPage::Controller); }
void UTASettingsMenuWidget::FavoritesPage() { SelectPage(ETAGameSettingsPage::Favorites); }
