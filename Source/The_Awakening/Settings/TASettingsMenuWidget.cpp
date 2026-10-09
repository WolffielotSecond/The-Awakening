#include "Settings/TASettingsMenuWidget.h"
#include "Settings/TASettingRowWidget.h"
#include "Settings/TASettingsPageWidget.h"
#include "Settings/TASettingsSectionWidget.h"
#include "Settings/TASettingsDescriptionBlockWidget.h"
#include "Settings/TASettingsSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ProgressBar.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ButtonSlot.h"
#include "Brushes/SlateColorBrush.h"
#include "Blueprint/WidgetTree.h"
#include "Core/TAInputIconSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

UTASettingsMenuWidget::UTASettingsMenuWidget(const FObjectInitializer& O):Super(O)
{
 SetIsFocusable(true);
}
void UTASettingsMenuWidget::InitializeMenu(AThe_AwakeningPlayerController* PC,UTASettingsMenuWidget* Parent,UTASettingsMenuDefinitionAsset* Menu)
{
 InputController=PC; ParentMenu=Parent; MenuDefinition=Menu?Menu:DefaultMenuDefinition.Get();
 if (Parent && !MenuDefinition)
 {
  MenuDefinition=NewObject<UTASettingsMenuDefinitionAsset>(this);
  MenuDefinition->MenuId=TEXT("Menu.Custom"); MenuDefinition->TitleTextId=TEXT("UI_Settings");
 }
}
void UTASettingsMenuWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (!InputController.IsValid()) InputController=Cast<AThe_AwakeningPlayerController>(GetOwningPlayer());
 if (GetOwningLocalPlayer()) SettingsSubsystem=GetOwningLocalPlayer()->GetSubsystem<UTASettingsSubsystem>();
 if (SettingsSubsystem) { SettingsSubsystem->OnSettingValueChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::ValueChanged); SettingsSubsystem->OnFavoritesChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::FavoritesChanged); }
 if (auto* GI=GetGameInstance()) if (auto* Loc=GI->GetSubsystem<UTALocalizeSubsystem>()) Loc->OnLanguageChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::LanguageChanged);
 if (Button_RestoreDefaults) Button_RestoreDefaults->OnPressed.AddUniqueDynamic(this,&UTASettingsMenuWidget::ResetPressed);
 InitializeResetHoldVisual();
 if (Button_ConfirmVideoMode) Button_ConfirmVideoMode->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::ConfirmVideo);
 if (Button_RevertVideoMode) Button_RevertVideoMode->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::RevertVideo);
 auto FindScroll=[](UWidget* Content)->UScrollBox* { for (UWidget* Parent=Content?Content->GetParent():nullptr;Parent;Parent=Parent->GetParent()) if (auto* Scroll=Cast<UScrollBox>(Parent)) return Scroll; return nullptr; };
 if (!ScrollBox_SettingsOptions) ScrollBox_SettingsOptions=FindScroll(Box_SettingsOptions);
 if (!ScrollBox_Description) ScrollBox_Description=FindScroll(Box_Description?static_cast<UWidget*>(Box_Description.Get()):Text_Description.Get());
 if (auto* GI=GetGameInstance()) if (auto* Icons=GI->GetSubsystem<UTAInputIconSubsystem>())
 {
  Icons->OnInputPromptsChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::InputPresentationChanged);
  bGamepadDevice=Icons->GetCurrentDeviceType()!=EInputDeviceType::KeyboardMouse;
 }
 BuildPageEntries(); RefreshPageLabels(); BuildSettingRows(); BuildPromptBar();
}
void UTASettingsMenuWidget::NativeDestruct()
{
 ReleaseRequest();
 if (auto* GI=GetGameInstance()) if (auto* Icons=GI->GetSubsystem<UTAInputIconSubsystem>()) Icons->OnInputPromptsChanged.RemoveDynamic(this,&UTASettingsMenuWidget::InputPresentationChanged);
 if (SettingsSubsystem) { SettingsSubsystem->OnSettingValueChanged.RemoveDynamic(this,&UTASettingsMenuWidget::ValueChanged); SettingsSubsystem->OnFavoritesChanged.RemoveDynamic(this,&UTASettingsMenuWidget::FavoritesChanged); }
 if (auto* GI=GetGameInstance()) if (auto* Loc=GI->GetSubsystem<UTALocalizeSubsystem>()) Loc->OnLanguageChanged.RemoveDynamic(this,&UTASettingsMenuWidget::LanguageChanged);
 if (ActiveSubmenu) { auto* Child=ActiveSubmenu.Get(); ActiveSubmenu=nullptr; Child->RemoveFromParent(); }
 if (!ParentMenu.IsValid() && SettingsSubsystem) SettingsSubsystem->RevertVideoMode();
 Super::NativeDestruct();
}
void UTASettingsMenuWidget::ReleaseRequest()
{
 CancelResetHold();
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
 W->Configure(D,Text(D.NameTextId),Value,SettingsSubsystem?SettingsSubsystem->GetInteger(D.SettingId,0):0,SettingsSubsystem && SettingsSubsystem->IsFavorite(D.SettingId),UsesEnglishLayout());
 W->OnHovered.AddDynamic(this,&UTASettingsMenuWidget::RowHovered);
 W->OnActivated.AddDynamic(this,&UTASettingsMenuWidget::RowActivated);
 W->OnAdjusted.AddDynamic(this,&UTASettingsMenuWidget::RowAdjusted);
 W->OnNumberChanged.AddDynamic(this,&UTASettingsMenuWidget::RowNumberChanged);
 W->OnFavorite.AddDynamic(this,&UTASettingsMenuWidget::RowFavorite);
 Box_SettingsOptions->AddChild(W); Rows.Add(W); return W;
}
void UTASettingsMenuWidget::BuildSettingRows()
{
 if (!Box_SettingsOptions) return; Box_SettingsOptions->ClearChildren(); Rows.Reset(); SectionHeadings.Reset(); SectionHeadingIds.Reset();
 if (SettingsSubsystem)
 {
  if (MenuDefinition)
  {
   for (FName Id:MenuDefinition->SettingIds)
    if (const auto* D=SettingsSubsystem->FindDefinition(Id)) AddRow(*D,SettingsSubsystem->FormatValue(*D));
  }
  else
  {
   TSet<FName> Favorites; for (FName Id:SettingsSubsystem->GetFavoriteSettingIds()) Favorites.Add(Id);
   const auto Groups=FTASettingsCatalog::BuildViewGroups(SettingsSubsystem->GetPages(),SettingsSubsystem->GetDefinitions(),CurrentPageId,Favorites);
   for (const auto& G:Groups)
   {
    if (SectionWidgetClass)
    {
     if (auto* Heading=CreateWidget<UTASettingsSectionWidget>(this,SectionWidgetClass))
     {
      Box_SettingsOptions->AddChild(Heading); Heading->SetTitle(Text(G.HeadingId.ToString()));
      SectionHeadings.Add(Heading); SectionHeadingIds.Add(G.HeadingId);
     }
    }
    else UE_LOG(LogTemp,Warning,TEXT("Configure SectionWidgetClass on the main Settings WBP."));
    for (FName Id:G.SettingIds)
     if (const auto* D=SettingsSubsystem->FindDefinition(Id)) AddRow(*D,SettingsSubsystem->FormatValue(*D));
   }
  }
 }
 SelectedRowIndex=Rows.IndexOfByPredicate([&](const auto& R){return R->GetSettingId()==SelectedSettingId;});
 EnsureRowSelection();
 if (Text_Empty) { Text_Empty->SetText(Text(TEXT("Settings.Empty"))); Text_Empty->SetVisibility(Rows.IsEmpty()?ESlateVisibility::Visible:ESlateVisibility::Collapsed); }
 RefreshRows();
}
void UTASettingsMenuWidget::RefreshRows()
{
 EnsureRowSelection();
 for (int32 I=0;I<Rows.Num();++I) if (auto* R=Rows[I].Get())
 {
  if (SettingsSubsystem) if (const auto* D=SettingsSubsystem->FindDefinition(R->GetSettingId())) R->Configure(*D,Text(D->NameTextId),SettingsSubsystem->FormatValue(*D),SettingsSubsystem->GetInteger(D->SettingId,0),SettingsSubsystem->IsFavorite(D->SettingId),UsesEnglishLayout());
  R->SetHighlighted(I==SelectedRowIndex);
 }
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindDefinition(SelectedSettingId):nullptr;
 if (PromptSettingId!=SelectedSettingId) BuildPromptBar();
 RefreshSettingDetails(D);
 const bool Pending=SettingsSubsystem && SettingsSubsystem->HasPendingVideoMode();
 if (Button_ConfirmVideoMode) Button_ConfirmVideoMode->SetVisibility(Pending?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if (Button_RevertVideoMode) Button_RevertVideoMode->SetVisibility(Pending?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UTASettingsMenuWidget::RefreshSettingDetails(const FTASettingDefinition* D)
{
 if (Text_SettingTitle) Text_SettingTitle->SetText(D?Text(D->NameTextId):FText::GetEmpty());
 TArray<FTASettingDescriptionBlock> Blocks;
 if (D)
 {
  Blocks=D->DescriptionBlocks;
  if (Blocks.IsEmpty() && !D->DescriptionTextId.IsEmpty())
  {
   FTASettingDescriptionBlock Block; Block.TextId=D->DescriptionTextId; Blocks.Add(Block);
  }
 }
 const bool UseBlocks=Box_Description && DescriptionBlockWidgetClass;
 if (Text_Description)
 {
  Text_Description->SetText(D && !D->DescriptionTextId.IsEmpty()?Text(D->DescriptionTextId):FText::GetEmpty());
  Text_Description->SetVisibility(UseBlocks?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
 }
 if (!Box_Description) return;
 Box_Description->SetVisibility(UseBlocks && !Blocks.IsEmpty()?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
 const int32 Count=UseBlocks?Blocks.Num():0;
 // Reuse blocks while changing values or hovering so the details scroll position stays stable.
 if (DescriptionEntries.Num()!=Count)
 {
  Box_Description->ClearChildren(); DescriptionEntries.Reset();
  for (int32 I=0;I<Count;++I)
   if (auto* Entry=CreateWidget<UTASettingsDescriptionBlockWidget>(this,DescriptionBlockWidgetClass))
   { Box_Description->AddChild(Entry); DescriptionEntries.Add(Entry); }
 }
 for (int32 I=0;I<DescriptionEntries.Num();++I)
  DescriptionEntries[I]->Configure(Blocks[I],Blocks[I].Type==ETASettingDescriptionBlockType::Text && !Blocks[I].TextId.IsEmpty()?Text(Blocks[I].TextId):FText::GetEmpty());
}
void UTASettingsMenuWidget::SelectPage(FName PageId)
{
 if (MenuDefinition || !Allows(ETAInputCapability::Navigate) || !PageDefinitions.ContainsByPredicate([&](const auto& D){return D.PageId==PageId;})) return;
 CancelResetHold(); CurrentPageId=PageId; bSuppressHoverAfterScroll=false; SelectedSettingId=NAME_None; SelectedRowIndex=INDEX_NONE; BuildSettingRows(); RefreshPageLabels();
}
void UTASettingsMenuWidget::MovePage(int32 Dir)
{
 if (MenuDefinition || PageDefinitions.IsEmpty()) return;
 const int32 I=PageDefinitions.IndexOfByPredicate([&](const auto& D){return D.PageId==CurrentPageId;});
 SelectPage(PageDefinitions[(FMath::Max(I,0)+Dir+PageDefinitions.Num())%PageDefinitions.Num()].PageId);
}
void UTASettingsMenuWidget::EnsureRowSelection()
{
 if (!Rows.IsValidIndex(SelectedRowIndex) || Rows[SelectedRowIndex]->GetSettingId()!=SelectedSettingId)
 {
  SelectedRowIndex=Rows.IndexOfByPredicate([&](const auto& R){return R->GetSettingId()==SelectedSettingId;});
  if (SelectedRowIndex==INDEX_NONE && !Rows.IsEmpty()) SelectedRowIndex=0;
 }
 SelectedSettingId=Rows.IsValidIndex(SelectedRowIndex)?Rows[SelectedRowIndex]->GetSettingId():NAME_None;
}
void UTASettingsMenuWidget::SelectRow(int32 Index)
{
 if (!Rows.IsValidIndex(Index)) return;
 if (SelectedSettingId!=Rows[Index]->GetSettingId()) SliderRepeatDirection=0;
 SelectedRowIndex=Index; SelectedSettingId=Rows[Index]->GetSettingId();
 if (PromptSettingId!=SelectedSettingId) BuildPromptBar();
}
void UTASettingsMenuWidget::MoveSelection(int32 Dir)
{
 if (Rows.IsEmpty()) return;
 EnsureRowSelection(); SelectRow((SelectedRowIndex+Dir+Rows.Num())%Rows.Num()); RefreshRows();
 if (ScrollBox_SettingsOptions && Rows.IsValidIndex(SelectedRowIndex)) ScrollBox_SettingsOptions->ScrollWidgetIntoView(Rows[SelectedRowIndex],false);
}
void UTASettingsMenuWidget::AdjustSelectedValue(int32 Dir)
{
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindDefinition(SelectedSettingId):nullptr;
 if (D && (D->Type==ETASettingType::Choice || D->Type==ETASettingType::Slider)) SettingsSubsystem->AdjustValue(SelectedSettingId,Dir);
}
void UTASettingsMenuWidget::ConfirmSelection()
{
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindDefinition(SelectedSettingId):nullptr; if (!D) return;
 if (D->Type==ETASettingType::Submenu) OpenSubmenu(*D);
 else if (D->Type==ETASettingType::Toggle) SettingsSubsystem->AdjustValue(D->SettingId,1);
}
void UTASettingsMenuWidget::OpenSubmenu(const FTASettingDefinition& D)
{
 if (!InputController.IsValid() || ActiveSubmenu) return;
 auto Class=D.TargetMenuWidgetClass;
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
{ if (bPointerSelectionEnabled && !bSuppressHoverAfterScroll && Allows(ETAInputCapability::Navigate)) if (auto* R=Cast<UTASettingRowWidget>(W)) { SelectRow(Rows.IndexOfByKey(R)); RefreshRows(); } }
void UTASettingsMenuWidget::RowActivated(UTASettingRowWidget* R)
{ if (!R || IsCapturingPlayerInput() || !Allows(ETAInputCapability::Confirm)) return; bSuppressHoverAfterScroll=false; SelectRow(Rows.IndexOfByKey(R)); ConfirmSelection(); RefreshRows(); }
void UTASettingsMenuWidget::RowAdjusted(UTASettingRowWidget* R,int32 Dir)
{ if (!R || !Allows(ETAInputCapability::Navigate)) return; bSuppressHoverAfterScroll=false; SelectRow(Rows.IndexOfByKey(R)); AdjustSelectedValue(Dir); }
void UTASettingsMenuWidget::RowNumberChanged(UTASettingRowWidget* R,int32 N)
{ if (R && Allows(ETAInputCapability::Navigate) && SettingsSubsystem) { SelectRow(Rows.IndexOfByKey(R)); SettingsSubsystem->SetValue(R->GetSettingId(),N); } }
void UTASettingsMenuWidget::RowFavorite(UTASettingRowWidget* R)
{ if (R && Allows(ETAInputCapability::ToggleFavorite) && SettingsSubsystem) SettingsSubsystem->SetFavorite(R->GetSettingId(),!SettingsSubsystem->IsFavorite(R->GetSettingId())); }
void UTASettingsMenuWidget::ValueChanged(FName) { RefreshRows(); }
void UTASettingsMenuWidget::FavoritesChanged() { if (CurrentPageId==FTASettingsCatalog::FavoritesPageId()) BuildSettingRows(); else RefreshRows(); }
void UTASettingsMenuWidget::LanguageChanged() { RefreshPageLabels(); for (int32 I=0;I<SectionHeadings.Num();++I) SectionHeadings[I]->SetTitle(Text(SectionHeadingIds[I].ToString())); RefreshRows(); BuildPromptBar(); }
void UTASettingsMenuWidget::BuildPageEntries()
{
 PageEntries.Reset(); PageDefinitions.Reset();
 if (Box_Pages) { Box_Pages->ClearChildren(); Box_Pages->SetVisibility(MenuDefinition?ESlateVisibility::Collapsed:ESlateVisibility::Visible); }
 if (MenuDefinition) return;
 PageDefinitions=FTASettingsCatalog::BuildNavigationPages(SettingsSubsystem?SettingsSubsystem->GetPages():FTASettingsCatalog::BuildPages());
 if (!PageDefinitions.ContainsByPredicate([&](const auto& D){return D.PageId==CurrentPageId;})) CurrentPageId=FTASettingsCatalog::FavoritesPageId();
 if (!Box_Pages || !PageWidgetClass)
 { UE_LOG(LogTemp,Warning,TEXT("Configure Box_Pages and PageWidgetClass on the main Settings Blueprint.")); return; }
 for (const auto& D:PageDefinitions)
 {
  auto* Entry=CreateWidget<UTASettingsPageWidget>(this,PageWidgetClass);
  PageEntries.Add(Entry); if (!Entry) continue;
  Box_Pages->AddChild(Entry);
  Entry->Configure(D,Text(D.PageId.ToString()),D.PageId==CurrentPageId,UsesEnglishLayout());
  Entry->OnPageActivated.AddDynamic(this,&UTASettingsMenuWidget::SelectPage);
 }
}
void UTASettingsMenuWidget::RefreshPageLabels()
{
 ApplyLanguageLayout(UsesEnglishLayout());
 if (Text_RestoreDefaults) Text_RestoreDefaults->SetText(Text(TEXT("Settings.RestoreDefaults")));
 for (int32 I=0;I<PageEntries.Num();++I)
 {
  if (!PageEntries[I]) continue;
  // Entries are generated in the same order as definitions.
  const auto& D=PageDefinitions[I];
  PageEntries[I]->Configure(D,Text(D.PageId.ToString()),D.PageId==CurrentPageId,UsesEnglishLayout());
 }
 if (Text_Title) Text_Title->SetText(Text(MenuDefinition?MenuDefinition->TitleTextId:TEXT("UI_Settings")));
}
bool UTASettingsMenuWidget::UsesEnglishLayout() const
{
 if (auto* GI=GetGameInstance())
  if (auto* Loc=GI->GetSubsystem<UTALocalizeSubsystem>()) return Loc->GetCurrentLanguage()==TEXT("en");
 return false;
}
void UTASettingsMenuWidget::ApplyLanguageLayout(bool English)
{
 for (UButton* Button:{Button_RestoreDefaults.Get(),Button_ConfirmVideoMode.Get(),Button_RevertVideoMode.Get()})
 {
  if (!Button) continue;
  auto* Outer=Cast<UHorizontalBoxSlot>(Button->Slot);
  auto* Content=Button->GetContent();
  auto* Inner=Content?Cast<UButtonSlot>(Content->Slot):nullptr;
  if (!Outer || !Inner) continue;
  if (!AuthoredFooterLayout.Contains(Button))
   AuthoredFooterLayout.Add(Button,FFooterLayout{Outer->GetPadding(),Inner->GetPadding(),Outer->GetVerticalAlignment()});
  const auto& Original=AuthoredFooterLayout.FindChecked(Button);
  FMargin OuterPadding=Original.OuterPadding;
  if (English) OuterPadding.Right=FMath::Max(OuterPadding.Right,16.f);
  Outer->SetPadding(OuterPadding);
  Outer->SetVerticalAlignment(English?VAlign_Center:Original.Vertical);
  Inner->SetPadding(English?FMargin(12.f,8.f):Original.ContentPadding);
 }
}
namespace
{
 struct FSettingsCommand { const TCHAR* Name; ETAInputCapability Capability; bool GamepadOnly; bool Navigation; };
 const FSettingsCommand SettingsCommands[]={
  {TEXT("PreviousPage"),ETAInputCapability::Navigate,false,true},
  {TEXT("NextPage"),ETAInputCapability::Navigate,false,true},
  {TEXT("PreviousItem"),ETAInputCapability::Navigate,true,true},
  {TEXT("NextItem"),ETAInputCapability::Navigate,true,true},
  {TEXT("AdjustLeft"),ETAInputCapability::Navigate,false,false},
  {TEXT("AdjustRight"),ETAInputCapability::Navigate,false,false},
  {TEXT("Confirm"),ETAInputCapability::Confirm,true,false},
  {TEXT("Favorite"),ETAInputCapability::ToggleFavorite,false,false},
  {TEXT("ResetPage"),ETAInputCapability::Confirm,false,false},
  {TEXT("Back"),ETAInputCapability::Close,false,false},
  {TEXT("ScrollUp"),ETAInputCapability::Navigate,true,false},
  {TEXT("ScrollDown"),ETAInputCapability::Navigate,true,false},
  {TEXT("PointerClick"),ETAInputCapability::Confirm,true,false}
 };
}
FString UTASettingsMenuWidget::GetSettingCommandTextId(FName Name) const
{
 if (Name==TEXT("ScrollUp") || Name==TEXT("ScrollDown") || (Name==TEXT("ResetPage") && MenuDefinition)) return {};
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindDefinition(SelectedSettingId):nullptr;
 if (Name==TEXT("AdjustLeft") || Name==TEXT("AdjustRight"))
 {
  if (!D || (D->Type!=ETASettingType::Choice && D->Type!=ETASettingType::Slider)) return {};
  if (D->Type==ETASettingType::Choice) return Name==TEXT("AdjustLeft")?TEXT("UI_Settings_PreviousChoice"):TEXT("UI_Settings_NextChoice");
 }
 if (Name==TEXT("Confirm") && (!D || (D->Type!=ETASettingType::Toggle && D->Type!=ETASettingType::Submenu))) return {};
 return FString(TEXT("UI_Settings_"))+Name.ToString();
}
void UTASettingsMenuWidget::BuildPromptBar()
{
 PromptSettingId=SelectedSettingId;
 if (!HorizontalBox_Controls || !InputController.IsValid()) return;
 HorizontalBox_Controls->ClearChildren(); Prompts.Reset();
 const bool BindingMenu=MenuDefinition && (MenuDefinition->MenuKind==ETASettingSubmenuTarget::KeyBindings || MenuDefinition->MenuKind==ETASettingSubmenuTarget::ControllerKeyBindings);
 for (const auto& Command:SettingsCommands)
 {
  const FName Name(Command.Name);
  if (Command.GamepadOnly && !bGamepadDevice) continue;
  if (Name==TEXT("PointerClick")) continue;
  if (MenuDefinition && (Name==TEXT("PreviousPage") || Name==TEXT("NextPage"))) continue;
  if (BindingMenu && (Name==TEXT("AdjustLeft") || Name==TEXT("AdjustRight") || Name==TEXT("Favorite"))) continue;
  const FString Label=GetSettingCommandTextId(Name);
  if (Label.IsEmpty()) continue;
  if (auto* P=FTAPromptWidgetUtils::AddActionPrompt(this,HorizontalBox_Controls,ActionPromptWidgetClass,InputController->GetSettingsAction(Name),*Label))
  {
   if (Name==TEXT("ResetPage"))
   {
    FNumberFormattingOptions Format; Format.SetMaximumFractionalDigits(2);
    const float Seconds=FMath::IsFinite(ResetHoldDuration)?FMath::Max(0.01f,ResetHoldDuration):3.f;
    P->ConfigurePrompt(InputController->GetSettingsAction(Name),FText::Format(Text(Label),FText::AsNumber(Seconds,&Format)));
   }
   P->OnPromptClicked.AddDynamic(this,&UTASettingsMenuWidget::PromptClicked); Prompts.Add(P);
  }
 }
}
void UTASettingsMenuWidget::PromptClicked(UTAActionPromptWidget* P)
{
 if (!P || !InputController.IsValid()) return;
 // A prompt click is not a three-second hold; use the reset button or IMC key.
 if (P->GetPromptAction()==InputController->GetSettingsAction(TEXT("ResetPage"))) return;
 for (const auto& Command:SettingsCommands)
  if (P->GetPromptAction()==InputController->GetSettingsAction(Command.Name) && Allows(Command.Capability))
  { ExecuteSettingsAction(Command.Name,false); return; }
}
TOptional<ETAInputCapability> UTASettingsMenuWidget::ResolvePlayerInput(FKey K) const
{
 if (!InputController.IsValid() || K.IsMouseButton()) return {};
 for (const auto& Command:SettingsCommands)
  if (!(MenuDefinition && FName(Command.Name)==TEXT("ResetPage")) && (!Command.GamepadOnly || K.IsGamepadKey()) && InputController->IsKeyMappedToAction(K,InputController->GetSettingsAction(Command.Name))) return Command.Capability;
 return {};
}
void UTASettingsMenuWidget::ExecutePlayerInput(FKey K,ETAInputCapability C)
{
 if (!Allows(C)) return;
 for (const auto& Command:SettingsCommands)
  if (Command.Capability==C && (!Command.GamepadOnly || K.IsGamepadKey()) && InputController->IsKeyMappedToAction(K,InputController->GetSettingsAction(Command.Name)))
  { ExecuteSettingsAction(Command.Name,K.IsGamepadKey() && Command.Navigation); return; }
}
void UTASettingsMenuWidget::ExecuteSettingsAction(FName Name,bool GamepadNavigation)
{
 if (Name==TEXT("Back")) { HandleMenuBackRequested(); return; }
 if (IsCapturingPlayerInput()) return;
 if (GamepadNavigation) BeginGamepadNavigation();
 if (Name==TEXT("PreviousPage")) MovePage(-1);
 else if (Name==TEXT("NextPage")) MovePage(1);
 else if (Name==TEXT("PreviousItem")) MoveSelection(-1);
 else if (Name==TEXT("NextItem")) MoveSelection(1);
 else if (Name==TEXT("AdjustLeft") || Name==TEXT("AdjustRight"))
 {
  const int32 Direction=Name==TEXT("AdjustLeft")?-1:1;
  SliderRepeatDirection=Direction; SliderRepeatSettingId=SelectedSettingId; SliderRepeatDelay=0.35f;
  AdjustSelectedValue(Direction);
 }
 else if (Name==TEXT("Confirm")) ConfirmSelection();
 else if (Name==TEXT("ResetPage") && !MenuDefinition) ResetPressed();
 else if (Name==TEXT("Favorite")) RowFavorite(Rows.IsValidIndex(SelectedRowIndex)?Rows[SelectedRowIndex].Get():nullptr);
 else if (Name==TEXT("PointerClick")) { if (bPointerSelectionEnabled && InputController.IsValid()) InputController->SimulateSyntheticLeftMouseClick(); }
 else if (Name==TEXT("ScrollUp") || Name==TEXT("ScrollDown")) { ScrollAtPointer(Name==TEXT("ScrollUp")?-1:1); ScrollRepeatDelay=0.35f; }
}
void UTASettingsMenuWidget::BeginGamepadNavigation()
{
 bPointerSelectionEnabled=false;
 if (InputController.IsValid()) InputController->SynchronizeInputPresentation();
}
void UTASettingsMenuWidget::InputPresentationChanged()
{
 if (auto* GI=GetGameInstance()) if (auto* Icons=GI->GetSubsystem<UTAInputIconSubsystem>()) bGamepadDevice=Icons->GetCurrentDeviceType()!=EInputDeviceType::KeyboardMouse;
 if (!bGamepadDevice) { bPointerSelectionEnabled=true; bSuppressHoverAfterScroll=false; }
 BuildPromptBar();
 if (InputController.IsValid()) InputController->SynchronizeInputPresentation();
}
void UTASettingsMenuWidget::NotifyPlayerPointerMoved()
{
 if (!Allows(ETAInputCapability::Cursor)) return;
 bPointerSelectionEnabled=true;
 bSuppressHoverAfterScroll=false;
 SelectPointerRow();
 if (InputController.IsValid()) InputController->SynchronizeInputPresentation();
}
void UTASettingsMenuWidget::SelectPointerRow()
{
 if (!bPointerSelectionEnabled || bSuppressHoverAfterScroll || !Allows(ETAInputCapability::Navigate) || !FSlateApplication::IsInitialized()) return;
 const FVector2D Position=FSlateApplication::Get().GetCursorPos();
 for (int32 I=0;I<Rows.Num();++I)
  if (Rows[I]->IsVisible() && Rows[I]->GetCachedGeometry().IsUnderLocation(Position) &&
      (!ScrollBox_SettingsOptions || ScrollBox_SettingsOptions->GetCachedGeometry().IsUnderLocation(Position)))
  { if (SelectedRowIndex!=I) { SelectRow(I); RefreshRows(); } return; }
}
void UTASettingsMenuWidget::ScrollAtPointer(int32 Direction)
{
 if (!Allows(ETAInputCapability::Navigate) || !FSlateApplication::IsInitialized()) return;
 const FVector2D Position=FSlateApplication::Get().GetCursorPos();
 for (UScrollBox* Scroll:{ScrollBox_SettingsOptions.Get(),ScrollBox_Description.Get()})
  if (Scroll && Scroll->IsVisible() && Scroll->GetCachedGeometry().IsUnderLocation(Position))
  { bSuppressHoverAfterScroll=true; Scroll->SetScrollOffset(FMath::Clamp(Scroll->GetScrollOffset()+Direction*60.f,0.f,Scroll->GetScrollOffsetOfEnd())); return; }
}
void UTASettingsMenuWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
 Super::NativeTick(Geometry,DeltaTime);
 TickResetHold(DeltaTime);
 if (!InputController.IsValid() || !Allows(ETAInputCapability::Navigate) || IsCapturingPlayerInput()) { SliderRepeatDirection=0; return; }
 TickSliderAdjustment(DeltaTime,
  InputController->ReadHeldAction(InputController->GetSettingsAction(TEXT("AdjustLeft"))).Get<bool>(),
  InputController->ReadHeldAction(InputController->GetSettingsAction(TEXT("AdjustRight"))).Get<bool>());
 if (!bGamepadDevice) return;
 const bool Up=InputController->ReadHeldAction(InputController->GetSettingsAction(TEXT("ScrollUp"))).Get<bool>();
 const bool Down=InputController->ReadHeldAction(InputController->GetSettingsAction(TEXT("ScrollDown"))).Get<bool>();
 if (Up==Down) { ScrollRepeatDelay=0; return; }
 ScrollRepeatDelay-=DeltaTime;
 if (ScrollRepeatDelay<=0) { ScrollAtPointer(Up?-1:1); ScrollRepeatDelay=0.12f; }
}
void UTASettingsMenuWidget::TickSliderAdjustment(float DeltaTime,bool Left,bool Right)
{
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindDefinition(SelectedSettingId):nullptr;
 const int32 Direction=Left==Right?0:(Left?-1:1);
 if (!D || D->Type!=ETASettingType::Slider || SliderRepeatSettingId!=SelectedSettingId || Direction!=SliderRepeatDirection || !Direction)
 { SliderRepeatDirection=0; return; }
 SliderRepeatDelay-=DeltaTime;
 while (SliderRepeatDelay<=0)
 {
  SliderRepeatDelay+=0.08f;
  AdjustSelectedValue(Direction);
 }
}
FReply UTASettingsMenuWidget::NativeOnPreviewKeyDown(const FGeometry&,const FKeyEvent&)
{
 // The controller's IMC router runs before Slate. Prevent its unbound fallback
 // from navigating buttons or activating a choice/slider via Enter or Space.
 return FReply::Handled();
}
void UTASettingsMenuWidget::RestoreCurrentDefaults()
{
 if (!SettingsSubsystem) return;
 TArray<FName> Ids;
 for (const UTASettingRowWidget* R:Rows)
 {
  const FName Id=R->GetSettingId();
  const auto* D=SettingsSubsystem->FindDefinition(Id);
  // A page reset must preserve the player's language, including language in Favorites.
  if (Id==TEXT("Game.Language") || (D && D->ApplyHandlerId==TEXT("Game.Language"))) continue;
  Ids.AddUnique(Id);
 }
 for(FName Id:Ids) SettingsSubsystem->RestoreSettingDefault(Id);
}
void UTASettingsMenuWidget::InitializeResetHoldVisual()
{
 // Prefer the authored widget even if the optional generated binding was stale.
 if (!ProgressBar_ResetHold)
 {
  if (UWidget* Authored=GetWidgetFromName(TEXT("ProgressBar_ResetHold")))
  {
   ProgressBar_ResetHold=Cast<UProgressBar>(Authored);
   if (!ProgressBar_ResetHold)
   { UE_LOG(LogTemp,Error,TEXT("ProgressBar_ResetHold must be a ProgressBar; refusing to create a duplicate.")); return; }
  }
 }
 if (!ProgressBar_ResetHold && Button_RestoreDefaults && WidgetTree)
 {
  UWidget* Content=Button_RestoreDefaults->GetContent();
  const auto* OldSlot=Content?Cast<UButtonSlot>(Content->Slot):nullptr;
  const EHorizontalAlignment Alignment=OldSlot?OldSlot->GetHorizontalAlignment():HAlign_Center;
  const EVerticalAlignment VerticalAlignment=OldSlot?OldSlot->GetVerticalAlignment():VAlign_Center;
  const FMargin ContentPadding=OldSlot?OldSlot->GetPadding():FMargin(0);
  Button_RestoreDefaults->ClearChildren();
  auto* Overlay=WidgetTree->ConstructWidget<UOverlay>();
  Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
  ProgressBar_ResetHold=WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("ProgressBar_ResetHold"));
  bGeneratedResetHoldVisual=true;
  auto FillStyle=ProgressBar_ResetHold->GetWidgetStyle();
  FillStyle.FillImage=FSlateColorBrush(FLinearColor::White);
  ProgressBar_ResetHold->SetWidgetStyle(FillStyle);
  auto* FillSlot=Overlay->AddChildToOverlay(ProgressBar_ResetHold);
  FillSlot->SetHorizontalAlignment(HAlign_Fill); FillSlot->SetVerticalAlignment(VAlign_Fill);
  if (Content) { auto* ContentSlot=Overlay->AddChildToOverlay(Content); ContentSlot->SetHorizontalAlignment(Alignment); ContentSlot->SetVerticalAlignment(VerticalAlignment); ContentSlot->SetPadding(ContentPadding); }
  Button_RestoreDefaults->SetContent(Overlay);
  if (auto* ButtonSlot=Cast<UButtonSlot>(Overlay->Slot)) { ButtonSlot->SetHorizontalAlignment(HAlign_Fill); ButtonSlot->SetVerticalAlignment(VAlign_Fill); ButtonSlot->SetPadding(FMargin(0)); }
 }
 if (ProgressBar_ResetHold && bGeneratedResetHoldVisual)
 {
  // SButton adds style padding to its content slot padding. Both must be zero.
  if (Button_RestoreDefaults)
  {
   auto ButtonStyle=Button_RestoreDefaults->GetStyle();
   ButtonStyle.NormalPadding=FMargin(0); ButtonStyle.PressedPadding=FMargin(0);
   Button_RestoreDefaults->SetStyle(ButtonStyle);
   if (UWidget* Content=Button_RestoreDefaults->GetContent()) if (auto* ContentSlot=Cast<UButtonSlot>(Content->Slot))
   { ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill); ContentSlot->SetPadding(FMargin(0)); }
  }
  if (auto* FillSlot=Cast<UOverlaySlot>(ProgressBar_ResetHold->Slot))
  { FillSlot->SetHorizontalAlignment(HAlign_Fill); FillSlot->SetVerticalAlignment(VAlign_Fill); FillSlot->SetPadding(FMargin(0)); }
  auto Style=ProgressBar_ResetHold->GetWidgetStyle();
  Style.BackgroundImage.DrawAs=ESlateBrushDrawType::NoDrawType;
  ProgressBar_ResetHold->SetWidgetStyle(Style);
  ProgressBar_ResetHold->SetBarFillType(EProgressBarFillType::LeftToRight);
  ProgressBar_ResetHold->SetBarFillStyle(EProgressBarFillStyle::Scale);
  ProgressBar_ResetHold->SetBorderPadding(FVector2D::ZeroVector);
  ProgressBar_ResetHold->SetFillColorAndOpacity(ResetHoldFillColor);
 }
 if (ProgressBar_ResetHold)
 {
  // Authored colors, brushes, fill style and layout belong to the Blueprint.
  ProgressBar_ResetHold->SetVisibility(ESlateVisibility::HitTestInvisible);
  ProgressBar_ResetHold->SetPercent(0);
 }
 ResetHold={}; bResetHoldArmed=false;
}
void UTASettingsMenuWidget::ResetPressed()
{
 if (!MenuDefinition && !IsCapturingPlayerInput() && Allows(ETAInputCapability::Confirm)) bResetHoldArmed=true;
}
void UTASettingsMenuWidget::CancelResetHold()
{
 bResetHoldArmed=false; ResetHold.Tick(0,false,ResetHoldDuration,ResetReturnDuration);
}
void UTASettingsMenuWidget::TickResetHold(float DeltaTime)
{
 const bool Allowed=!MenuDefinition && !IsCapturingPlayerInput() && Allows(ETAInputCapability::Confirm);
 const bool Held=Allowed && bResetHoldArmed && ((Button_RestoreDefaults && Button_RestoreDefaults->IsPressed()) ||
  (InputController.IsValid() && InputController->ReadHeldAction(InputController->GetSettingsAction(TEXT("ResetPage"))).Get<bool>()));
 if (!Held) bResetHoldArmed=false;
 if (ResetHold.Tick(DeltaTime,Held,ResetHoldDuration,ResetReturnDuration)) { SliderRepeatDirection=0; RestoreCurrentDefaults(); }
 if (ProgressBar_ResetHold) ProgressBar_ResetHold->SetPercent(ResetHold.Display);
}
void UTASettingsMenuWidget::ConfirmVideo() { if (Allows(ETAInputCapability::Confirm) && SettingsSubsystem) SettingsSubsystem->ConfirmVideoMode(); }
void UTASettingsMenuWidget::RevertVideo() { if (Allows(ETAInputCapability::Confirm) && SettingsSubsystem) SettingsSubsystem->RevertVideoMode(); }
