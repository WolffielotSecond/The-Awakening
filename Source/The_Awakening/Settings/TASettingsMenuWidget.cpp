#include "Settings/TASettingsMenuWidget.h"
#include "Settings/TADeadZonePreviewWidget.h"
#include "Settings/TASettingRowWidget.h"
#include "Settings/TASettingsPageWidget.h"
#include "Settings/TASettingsSectionWidget.h"
#include "Settings/TASettingsDescriptionBlockWidget.h"
#include "Settings/TASettingsSubsystem.h"
#include "UI/TACommonDialog.h"
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
 SelectionArea=MenuDefinition?ESelectionArea::Details:ESelectionArea::Categories;
}
void UTASettingsMenuWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (!InputController.IsValid()) InputController=Cast<AThe_AwakeningPlayerController>(GetOwningPlayer());
 if (GetOwningLocalPlayer()) SettingsSubsystem=GetOwningLocalPlayer()->GetSubsystem<UTASettingsSubsystem>();
 if (SettingsSubsystem) { SettingsSubsystem->RefreshDisplayCandidates(); if (!ParentMenu.IsValid()) SettingsSubsystem->BeginEditing(); SettingsSubsystem->OnVideoModeResolved.AddUniqueDynamic(this,&UTASettingsMenuWidget::VideoResolved); }
 if (SettingsSubsystem) { SettingsSubsystem->OnSettingValueChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::ValueChanged); SettingsSubsystem->OnFavoritesChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::FavoritesChanged); }
 if (auto* GI=GetGameInstance()) if (auto* Loc=GI->GetSubsystem<UTALocalizeSubsystem>()) Loc->OnLanguageChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::LanguageChanged);
 if (Button_RestoreDefaults) Button_RestoreDefaults->OnPressed.AddUniqueDynamic(this,&UTASettingsMenuWidget::ResetPressed);
 InitializeResetHoldVisual();
 if (Button_ApplySettings) Button_ApplySettings->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::ApplyChanges);
 auto FindScroll=[](UWidget* Content)->UScrollBox* { for (UWidget* Parent=Content?Content->GetParent():nullptr;Parent;Parent=Parent->GetParent()) if (auto* Scroll=Cast<UScrollBox>(Parent)) return Scroll; return nullptr; };
 if (!ScrollBox_SettingsOptions) ScrollBox_SettingsOptions=FindScroll(Box_SettingsOptions);
 if (!ScrollBox_Description) ScrollBox_Description=FindScroll(Box_Description?static_cast<UWidget*>(Box_Description.Get()):Text_Description.Get());
 if (auto* GI=GetGameInstance()) if (auto* Icons=GI->GetSubsystem<UTAInputIconSubsystem>())
 {
  Icons->OnInputPromptsChanged.AddUniqueDynamic(this,&UTASettingsMenuWidget::InputPresentationChanged);
  bGamepadDevice=Icons->GetCurrentDeviceType()!=EInputDeviceType::KeyboardMouse;
 }
 if (bGamepadDevice) bPointerSelectionEnabled=false;
 BuildPageEntries(); RefreshPageLabels(); BuildSettingRows(); BuildPromptBar();
}
void UTASettingsMenuWidget::NativeDestruct()
{
 if (ActiveDialog) { ActiveDialog->Dismiss(); ActiveDialog=nullptr; }
 if (SettingsSubsystem) SettingsSubsystem->OnVideoModeResolved.RemoveDynamic(this,&UTASettingsMenuWidget::VideoResolved);
 ReleaseRequest();
 if (auto* GI=GetGameInstance()) if (auto* Icons=GI->GetSubsystem<UTAInputIconSubsystem>()) Icons->OnInputPromptsChanged.RemoveDynamic(this,&UTASettingsMenuWidget::InputPresentationChanged);
 if (SettingsSubsystem) { SettingsSubsystem->OnSettingValueChanged.RemoveDynamic(this,&UTASettingsMenuWidget::ValueChanged); SettingsSubsystem->OnFavoritesChanged.RemoveDynamic(this,&UTASettingsMenuWidget::FavoritesChanged); }
 if (auto* GI=GetGameInstance()) if (auto* Loc=GI->GetSubsystem<UTALocalizeSubsystem>()) Loc->OnLanguageChanged.RemoveDynamic(this,&UTASettingsMenuWidget::LanguageChanged);
 if (ActiveSubmenu) { auto* Child=ActiveSubmenu.Get(); ActiveSubmenu=nullptr; Child->RemoveFromParent(); }
 if (!ParentMenu.IsValid() && SettingsSubsystem) { SettingsSubsystem->RevertVideoMode(); SettingsSubsystem->EndEditing(); }
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
 PendingExit=EPendingExit::None;
 if (ActiveDialog) { ActiveDialog->Dismiss(); ActiveDialog=nullptr; }
 if (SettingsSubsystem) SettingsSubsystem->OnVideoModeResolved.RemoveDynamic(this,&UTASettingsMenuWidget::VideoResolved);
 if (ParentMenu.IsValid() && ParentMenu->ActiveSubmenu==this) ParentMenu->ActiveSubmenu=nullptr;
 if (ActiveSubmenu) { auto* Child=ActiveSubmenu.Get(); ActiveSubmenu=nullptr; Child->RemoveFromParent(); }
 if (!ParentMenu.IsValid() && SettingsSubsystem) { SettingsSubsystem->RevertVideoMode(); SettingsSubsystem->EndEditing(); }
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
 W->Configure(D,Text(D.NameTextId),Value,SettingsSubsystem?SettingsSubsystem->GetEditableInteger(D.SettingId,0):0,SettingsSubsystem && SettingsSubsystem->IsFavorite(D.SettingId),UsesEnglishLayout());
 W->OnHovered.AddDynamic(this,&UTASettingsMenuWidget::RowHovered);
 W->OnActivated.AddDynamic(this,&UTASettingsMenuWidget::RowActivated);
 W->OnAdjusted.AddDynamic(this,&UTASettingsMenuWidget::RowAdjusted);
 W->OnNumberChanged.AddDynamic(this,&UTASettingsMenuWidget::RowNumberChanged);
 W->OnFavorite.AddDynamic(this,&UTASettingsMenuWidget::RowFavorite);
 W->SetDirty(SettingsSubsystem && SettingsSubsystem->IsSettingDirty(D.SettingId));
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
    if (const auto* D=SettingsSubsystem->FindEditableDefinition(Id)) AddRow(*D,SettingsSubsystem->FormatValue(*D));
  }
  else
  {
   TSet<FName> Favorites; for (FName Id:SettingsSubsystem->GetFavoriteSettingIds()) Favorites.Add(Id);
   const auto Groups=FTASettingsCatalog::BuildViewGroups(SettingsSubsystem->GetPages(),SettingsSubsystem->GetEditableDefinitions(),CurrentPageId,Favorites);
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
     if (const auto* D=SettingsSubsystem->FindEditableDefinition(Id)) AddRow(*D,SettingsSubsystem->FormatValue(*D));
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
  if (SettingsSubsystem) if (const auto* D=SettingsSubsystem->FindEditableDefinition(R->GetSettingId())) R->Configure(*D,Text(D->NameTextId),SettingsSubsystem->FormatValue(*D),SettingsSubsystem->GetEditableInteger(D->SettingId,0),SettingsSubsystem->IsFavorite(D->SettingId),UsesEnglishLayout());
  R->SetDirty(SettingsSubsystem && SettingsSubsystem->IsSettingDirty(R->GetSettingId()));
  R->SetHighlighted(I==SelectedRowIndex && (bPointerSelectionEnabled || SelectionArea==ESelectionArea::Details));
 }
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindEditableDefinition(SelectedSettingId):nullptr;
 if (PromptSettingId!=SelectedSettingId) BuildPromptBar();
 RefreshSettingDetails(D);
 const bool Pending=false;
 if (Button_ApplySettings) Button_ApplySettings->SetIsEnabled(SettingsSubsystem && SettingsSubsystem->HasDraftChanges() && !SettingsSubsystem->HasPendingVideoMode());
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
 auto IsPreview=[&](int32 I){return Blocks[I].Type==ETASettingDescriptionBlockType::DeadZonePreview;};
 // Reuse blocks while changing values or hovering so the details scroll position stays stable.
 bool bRebuild=DescriptionEntries.Num()!=Count;
 for (int32 I=0;!bRebuild && I<Count;++I) bRebuild=!DescriptionEntries[I] || DescriptionEntries[I]->IsA<UTADeadZonePreviewWidget>()!=IsPreview(I);
 if (bRebuild)
 {
  Box_Description->ClearChildren(); DescriptionEntries.Reset();
  for (int32 I=0;I<Count;++I)
  {
   UUserWidget* Entry=IsPreview(I)
    ?static_cast<UUserWidget*>(CreateWidget<UTADeadZonePreviewWidget>(this,DeadZonePreviewWidgetClass?DeadZonePreviewWidgetClass.Get():UTADeadZonePreviewWidget::StaticClass()))
    :static_cast<UUserWidget*>(CreateWidget<UTASettingsDescriptionBlockWidget>(this,DescriptionBlockWidgetClass));
   if (Entry) Box_Description->AddChild(Entry);
   DescriptionEntries.Add(Entry);
  }
 }
 for (int32 I=0;I<DescriptionEntries.Num();++I)
  if (auto* Block=Cast<UTASettingsDescriptionBlockWidget>(DescriptionEntries[I]))
   Block->Configure(Blocks[I],Blocks[I].Type==ETASettingDescriptionBlockType::Text && !Blocks[I].TextId.IsEmpty()?Text(Blocks[I].TextId):FText::GetEmpty());
}
void UTASettingsMenuWidget::SelectPage(FName PageId)
{
 if (MenuDefinition || !Allows(ETAInputCapability::Navigate) || !PageDefinitions.ContainsByPredicate([&](const auto& D){return D.PageId==PageId;})) return;
 if (PageId!=CurrentPageId) RequestExit(EPendingExit::Page,PageId);
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
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindEditableDefinition(SelectedSettingId):nullptr;
 if (D && (D->Type==ETASettingType::Choice || D->Type==ETASettingType::Slider)) SettingsSubsystem->AdjustDraftValue(SelectedSettingId,Dir);
}
void UTASettingsMenuWidget::ConfirmSelection()
{
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindEditableDefinition(SelectedSettingId):nullptr; if (!D) return;
 if (D->Type==ETASettingType::Submenu) RequestExit(EPendingExit::Submenu,D->SettingId);
 else if (D->Type==ETASettingType::Toggle) SettingsSubsystem->AdjustDraftValue(D->SettingId,1);
}
void UTASettingsMenuWidget::EnterDetails()
{
 SelectionArea=ESelectionArea::Details;
 EnsureRowSelection(); RefreshRows(); BuildPromptBar();
 if (ScrollBox_SettingsOptions && Rows.IsValidIndex(SelectedRowIndex)) ScrollBox_SettingsOptions->ScrollWidgetIntoView(Rows[SelectedRowIndex],false);
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
 if (!MenuDefinition && bGamepadDevice && SelectionArea==ESelectionArea::Details)
 { RequestExit(EPendingExit::Categories); return true; }
 RequestClose(); return true;
}
void UTASettingsMenuWidget::RowHovered(UTASelectableMenuOptionWidget* W)
{ if (bPointerSelectionEnabled && !bSuppressHoverAfterScroll && Allows(ETAInputCapability::Navigate)) if (auto* R=Cast<UTASettingRowWidget>(W)) { SelectionArea=ESelectionArea::Details; SelectRow(Rows.IndexOfByKey(R)); RefreshRows(); BuildPromptBar(); } }
void UTASettingsMenuWidget::RowActivated(UTASettingRowWidget* R)
{ if (!R || IsCapturingPlayerInput() || !Allows(ETAInputCapability::Confirm)) return; SelectionArea=ESelectionArea::Details; bSuppressHoverAfterScroll=false; SelectRow(Rows.IndexOfByKey(R)); ConfirmSelection(); RefreshRows(); BuildPromptBar(); }
void UTASettingsMenuWidget::RowAdjusted(UTASettingRowWidget* R,int32 Dir)
{ if (!R || !Allows(ETAInputCapability::Navigate)) return; SelectionArea=ESelectionArea::Details; bSuppressHoverAfterScroll=false; SelectRow(Rows.IndexOfByKey(R)); AdjustSelectedValue(Dir); }
void UTASettingsMenuWidget::RowNumberChanged(UTASettingRowWidget* R,int32 N)
{ if (R && Allows(ETAInputCapability::Navigate) && SettingsSubsystem) { SelectionArea=ESelectionArea::Details; SelectRow(Rows.IndexOfByKey(R)); SettingsSubsystem->SetDraftValue(R->GetSettingId(),N); } }
void UTASettingsMenuWidget::RowFavorite(UTASettingRowWidget* R)
{ if (R && Allows(ETAInputCapability::ToggleFavorite) && SettingsSubsystem) { SelectionArea=ESelectionArea::Details; SettingsSubsystem->SetFavorite(R->GetSettingId(),!SettingsSubsystem->IsFavorite(R->GetSettingId())); } }
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
 if (Text_ApplySettings) Text_ApplySettings->SetText(Text(TEXT("UI_Settings_Apply")));
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
  {TEXT("FocusCategories"),ETAInputCapability::Navigate,true,true},
  {TEXT("FocusDetails"),ETAInputCapability::Navigate,true,true},
  {TEXT("AdjustLeft"),ETAInputCapability::Navigate,false,false},
  {TEXT("AdjustRight"),ETAInputCapability::Navigate,false,false},
  {TEXT("Apply"),ETAInputCapability::Confirm,false,false},
  {TEXT("Confirm"),ETAInputCapability::Confirm,true,false},
  {TEXT("Favorite"),ETAInputCapability::ToggleFavorite,false,false},
  {TEXT("ResetPage"),ETAInputCapability::Confirm,false,false},
  {TEXT("Back"),ETAInputCapability::Close,false,false},
  {TEXT("ScrollUp"),ETAInputCapability::Navigate,true,false},
  {TEXT("ScrollDown"),ETAInputCapability::Navigate,true,false}
 };
}
FString UTASettingsMenuWidget::GetSettingCommandTextId(FName Name) const
{
 // Entering a category remains available through IMC without a footer hint.
 if (Name==TEXT("FocusDetails")) return {};
 if (Name==TEXT("FocusCategories") || Name==TEXT("FocusDetails"))
 {
  if (!bGamepadDevice || MenuDefinition) return {};
  if (Name==TEXT("FocusCategories")) return SelectionArea==ESelectionArea::Details?TEXT("UI_Settings_FocusCategories"):FString();
  return {};
 }
 if (Name==TEXT("ScrollUp") || Name==TEXT("ScrollDown"))
 {
  if (!bGamepadDevice || !GetScrollTarget()) return {};
  return Name==TEXT("ScrollUp")?TEXT("UI_Settings_ScrollListUp"):TEXT("UI_Settings_ScrollListDown");
 }
 if (bGamepadDevice && !MenuDefinition && SelectionArea==ESelectionArea::Categories)
 {
  if (Name==TEXT("PreviousItem")) return TEXT("UI_Settings_PreviousCategory");
  if (Name==TEXT("NextItem")) return TEXT("UI_Settings_NextCategory");
  if (Name==TEXT("Confirm")) return {};
  if (Name!=TEXT("Back") && Name!=TEXT("ResetPage")) return {};
 }
 if (Name==TEXT("ResetPage") && MenuDefinition) return {};
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindEditableDefinition(SelectedSettingId):nullptr;
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
  if (bGamepadDevice && (Name==TEXT("PreviousPage") || Name==TEXT("NextPage"))) continue;
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
 RefreshScrollPromptVisibility();
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
  if (!(MenuDefinition && (FName(Command.Name)==TEXT("ResetPage") || FName(Command.Name)==TEXT("FocusCategories") || FName(Command.Name)==TEXT("FocusDetails"))) && (!Command.GamepadOnly || K.IsGamepadKey()) && InputController->IsKeyMappedToAction(K,InputController->GetSettingsAction(Command.Name))) return Command.Capability;
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
 if (Name==TEXT("FocusCategories") || Name==TEXT("FocusDetails"))
 {
  if (!MenuDefinition)
  {
   if (Name==TEXT("FocusCategories") && SelectionArea==ESelectionArea::Details) RequestExit(EPendingExit::Categories);
   else if (Name==TEXT("FocusDetails") && SelectionArea==ESelectionArea::Categories) EnterDetails();
  }
  return;
 }
 if (Name==TEXT("ScrollUp") || Name==TEXT("ScrollDown"))
 { ScrollCurrentRegion(Name==TEXT("ScrollUp")?-1:1); ScrollRepeatDelay=0.35f; return; }
 if (Name==TEXT("ResetPage") && !MenuDefinition) { ResetPressed(); return; }
 if (bGamepadDevice && !MenuDefinition && SelectionArea==ESelectionArea::Categories)
 {
  if (Name==TEXT("PreviousItem") || Name==TEXT("NextItem")) MovePage(Name==TEXT("PreviousItem")?-1:1);
  else if (Name==TEXT("Confirm")) EnterDetails();
  return;
 }
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
 else if (Name==TEXT("Apply")) ApplyChanges();
 else if (Name==TEXT("Confirm")) ConfirmSelection();
 else if (Name==TEXT("Favorite")) RowFavorite(Rows.IsValidIndex(SelectedRowIndex)?Rows[SelectedRowIndex].Get():nullptr);
}
void UTASettingsMenuWidget::BeginGamepadNavigation()
{
 bPointerSelectionEnabled=false;
 RefreshRows(); BuildPromptBar();
 if (InputController.IsValid()) InputController->SynchronizeInputPresentation();
}
void UTASettingsMenuWidget::InputPresentationChanged()
{
 if (auto* GI=GetGameInstance()) if (auto* Icons=GI->GetSubsystem<UTAInputIconSubsystem>()) bGamepadDevice=Icons->GetCurrentDeviceType()!=EInputDeviceType::KeyboardMouse;
 bPointerSelectionEnabled=!bGamepadDevice;
 if (!bGamepadDevice) bSuppressHoverAfterScroll=false;
 RefreshRows();
 BuildPromptBar();
 if (InputController.IsValid()) InputController->SynchronizeInputPresentation();
}
void UTASettingsMenuWidget::NotifyPlayerPointerMoved()
{
 if (bGamepadDevice || !Allows(ETAInputCapability::Cursor)) return;
 bPointerSelectionEnabled=true;
 bSuppressHoverAfterScroll=false;
 SelectPointerRow();
 if (InputController.IsValid()) InputController->SynchronizeInputPresentation();
}
UWidget* UTASettingsMenuWidget::GetPlayerPointerTarget() const
{
 if (SelectionArea==ESelectionArea::Categories)
 {
  const int32 Index=PageDefinitions.IndexOfByPredicate([&](const auto& Page){return Page.PageId==CurrentPageId;});
  return PageEntries.IsValidIndex(Index)?PageEntries[Index].Get():nullptr;
 }
 return Rows.IsValidIndex(SelectedRowIndex)?Rows[SelectedRowIndex].Get():nullptr;
}
void UTASettingsMenuWidget::SelectPointerRow()
{
 if (!bPointerSelectionEnabled || bSuppressHoverAfterScroll || !Allows(ETAInputCapability::Navigate) || !FSlateApplication::IsInitialized()) return;
 const FVector2D Position=FSlateApplication::Get().GetCursorPos();
 for (int32 I=0;I<Rows.Num();++I)
  if (Rows[I]->IsVisible() && Rows[I]->GetCachedGeometry().IsUnderLocation(Position) &&
      (!ScrollBox_SettingsOptions || ScrollBox_SettingsOptions->GetCachedGeometry().IsUnderLocation(Position)))
  { if (SelectedRowIndex!=I || SelectionArea!=ESelectionArea::Details) { SelectionArea=ESelectionArea::Details; SelectRow(I); RefreshRows(); BuildPromptBar(); } return; }
}
UScrollBox* UTASettingsMenuWidget::GetScrollTarget() const
{
 return ScrollBox_SettingsOptions.Get();
}
bool UTASettingsMenuWidget::CanScrollCurrentRegion() const
{
 auto* Scroll=GetScrollTarget();
 return Scroll && Scroll->IsVisible() && Scroll->GetScrollOffsetOfEnd()>KINDA_SMALL_NUMBER;
}
void UTASettingsMenuWidget::RefreshScrollPromptVisibility()
{
 if (!InputController.IsValid()) return;
 const bool Visible=bGamepadDevice && CanScrollCurrentRegion();
 for (UTAActionPromptWidget* Prompt:Prompts)
  if (Prompt && (Prompt->GetPromptAction()==InputController->GetSettingsAction(TEXT("ScrollUp")) ||
                 Prompt->GetPromptAction()==InputController->GetSettingsAction(TEXT("ScrollDown"))))
   Prompt->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UTASettingsMenuWidget::ScrollCurrentRegion(int32 Direction)
{
 if (!Allows(ETAInputCapability::Navigate) || !CanScrollCurrentRegion()) return;
 auto* Scroll=GetScrollTarget();
 // Scrolling rows underneath a stationary pointer must not select a row or enter details.
 if (SelectionArea==ESelectionArea::Categories) bSuppressHoverAfterScroll=true;
 if (AnimatedScroll.Get()!=Scroll) { AnimatedScroll=Scroll; ScrollTargetOffset=Scroll->GetScrollOffset(); }
 ScrollTargetOffset=FMath::Clamp(ScrollTargetOffset+Direction*60.f,0.f,Scroll->GetScrollOffsetOfEnd());
}
void UTASettingsMenuWidget::TickScrollInterpolation(float DeltaTime)
{
 auto* Scroll=AnimatedScroll.Get();
 if (!bGamepadDevice || !Allows(ETAInputCapability::Navigate) || IsCapturingPlayerInput() || Scroll!=GetScrollTarget() || !CanScrollCurrentRegion())
 { AnimatedScroll.Reset(); return; }
 if (!Scroll) return;
 ScrollTargetOffset=FMath::Clamp(ScrollTargetOffset,0.f,Scroll->GetScrollOffsetOfEnd());
 Scroll->SetScrollOffset(FMath::FInterpTo(Scroll->GetScrollOffset(),ScrollTargetOffset,DeltaTime,ScrollInterpolationSpeed));
 if (FMath::IsNearlyEqual(Scroll->GetScrollOffset(),ScrollTargetOffset,0.1f))
 { Scroll->SetScrollOffset(ScrollTargetOffset); AnimatedScroll.Reset(); }
}
void UTASettingsMenuWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
 Super::NativeTick(Geometry,DeltaTime);
 // Overflow becomes authoritative after Slate layout, including viewport resizes.
 // Update the existing hints' visibility; do not rebuild or poll layout parameters.
 RefreshScrollPromptVisibility();
 TickScrollInterpolation(DeltaTime);
 DisplayRefreshDelay-=DeltaTime;
 if (DisplayRefreshDelay<=0 && SettingsSubsystem && !MenuDefinition)
 { DisplayRefreshDelay=1.f; SettingsSubsystem->RefreshDisplayCandidates(); }
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
 if (ScrollRepeatDelay<=0) { ScrollCurrentRegion(Up?-1:1); ScrollRepeatDelay=0.12f; }
}
void UTASettingsMenuWidget::TickSliderAdjustment(float DeltaTime,bool Left,bool Right)
{
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindEditableDefinition(SelectedSettingId):nullptr;
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
  const auto* D=SettingsSubsystem->FindEditableDefinition(Id);
  if (!D || D->Type==ETASettingType::Submenu) continue;
  // A page reset must preserve the player's language, including language in Favorites.
  if (Id==TEXT("Game.Language") || (D && D->ApplyHandlerId==TEXT("Game.Language"))) continue;
  Ids.AddUnique(Id);
 }
 for(FName Id:Ids) SettingsSubsystem->StageDefault(Id);
 // A completed reset is its own scoped commit, not an ordinary unsaved edit.
 // Existing error and video confirmation flows apply without submitting other drafts.
 if (!SettingsSubsystem->ApplyDraftForSettings(Ids)) ShowApplyError();
 else if (SettingsSubsystem->HasPendingVideoMode()) ShowVideoDialog();
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

namespace
{
 FTACommonDialogOption DialogOption(FName Result,const TCHAR* Label,int32 Slot)
 { FTACommonDialogOption O; O.ResultId=Result; O.LabelTextId=Label; O.InputSlot=Slot; return O; }
}
void UTASettingsMenuWidget::RequestClose() { RequestExit(EPendingExit::Close); }
void UTASettingsMenuWidget::RequestExit(EPendingExit Kind,FName Target)
{
 if (!SettingsSubsystem || ActiveDialog || SettingsSubsystem->HasPendingVideoMode()) return;
 PendingExit=Kind; PendingTarget=Target; CancelResetHold(); SliderRepeatDirection=0;
 if (!SettingsSubsystem->HasDraftChanges()) { CompleteExit(); return; }
 FTACommonDialogDefinition D;
 D.bAllowGamepadCursor=false;
 D.TitleTextId=TEXT("Dialog.Settings.Unsaved.Title"); D.MessageTextId=TEXT("Dialog.Settings.Unsaved.Message");
 if (Kind==EPendingExit::Categories)
  D.Options={DialogOption(TEXT("Apply"),TEXT("Dialog.Apply"),1),DialogOption(TEXT("Discard"),TEXT("Dialog.Discard"),2)};
 else D.Options={DialogOption(TEXT("Apply"),TEXT("Dialog.Apply"),1),DialogOption(TEXT("Discard"),TEXT("Dialog.Discard"),2),DialogOption(TEXT("Cancel"),TEXT("Dialog.Cancel"),3)};
 auto* LP=GetOwningLocalPlayer(); auto* S=LP?LP->GetSubsystem<UTACommonDialogSubsystem>():nullptr;
 ActiveDialog=S?S->ShowDialog(D):nullptr;
 if (ActiveDialog) ActiveDialog->OnResult.AddDynamic(this,&UTASettingsMenuWidget::DialogResult);
 else { PendingExit=EPendingExit::None; if (InputController.IsValid()) InputController->CancelSettingsCloseRequest(); }
}
void UTASettingsMenuWidget::CompleteExit()
{
 const auto Kind=PendingExit; const FName Target=PendingTarget; PendingExit=EPendingExit::None; PendingTarget=NAME_None;
 if (Kind==EPendingExit::Page)
 {
  CurrentPageId=Target; bSuppressHoverAfterScroll=false; SelectedSettingId=NAME_None; SelectedRowIndex=INDEX_NONE;
  BuildSettingRows(); RefreshPageLabels();
 }
 else if (Kind==EPendingExit::Categories)
 {
  SelectionArea=ESelectionArea::Categories;
  CancelResetHold(); SliderRepeatDirection=0;
  RefreshRows(); RefreshPageLabels(); BuildPromptBar();
 }
 else if (Kind==EPendingExit::Submenu)
 {
  if (const auto* D=SettingsSubsystem?SettingsSubsystem->FindEditableDefinition(Target):nullptr) OpenSubmenu(*D);
 }
 else if (Kind==EPendingExit::Close)
 {
  if (ParentMenu.IsValid()) { ParentMenu->ActiveSubmenu=nullptr; RemoveFromParent(); }
  else if (InputController.IsValid()) InputController->CloseSettingsMenu(true);
 }
}
void UTASettingsMenuWidget::ApplyChanges()
{
 if (!SettingsSubsystem || ActiveDialog || !Allows(ETAInputCapability::Confirm)) return;
 ApplyDraftAndContinue();
}
void UTASettingsMenuWidget::ApplyDraftAndContinue()
{
 if (!SettingsSubsystem || ActiveDialog) return;
 if (!SettingsSubsystem->ApplyDraft())
 {
  ShowApplyError();
  return;
 }
 if (SettingsSubsystem->HasPendingVideoMode()) ShowVideoDialog(); else CompleteExit();
}
void UTASettingsMenuWidget::ShowApplyError()
{
 PendingExit=EPendingExit::None;
 if (InputController.IsValid()) InputController->CancelSettingsCloseRequest();
 FTACommonDialogDefinition D; D.bAllowGamepadCursor=false; D.TitleTextId=TEXT("Dialog.Settings.Error.Title"); D.MessageTextId=TEXT("Dialog.Settings.Error.Message");
 D.Options={DialogOption(TEXT("Cancel"),TEXT("Dialog.OK"),1)};
 if (auto* LP=GetOwningLocalPlayer()) if (auto* S=LP->GetSubsystem<UTACommonDialogSubsystem>()) ActiveDialog=S->ShowDialog(D);
 if (ActiveDialog) ActiveDialog->OnResult.AddDynamic(this,&UTASettingsMenuWidget::DialogResult);
}
void UTASettingsMenuWidget::ShowVideoDialog()
{
 FTACommonDialogDefinition D; D.bAllowGamepadCursor=false; D.TitleTextId=TEXT("Dialog.Settings.Video.Title"); D.MessageTextId=TEXT("Dialog.Settings.Video.Message");
 D.Options={DialogOption(TEXT("Keep"),TEXT("Dialog.Keep"),1),DialogOption(TEXT("Revert"),TEXT("Dialog.Revert"),3)};
 D.TimeoutSeconds=15; D.TimeoutResultId=TEXT("Revert"); bVideoDialog=true;
 if (auto* LP=GetOwningLocalPlayer()) if (auto* S=LP->GetSubsystem<UTACommonDialogSubsystem>()) ActiveDialog=S->ShowDialog(D);
 if (ActiveDialog) ActiveDialog->OnResult.AddDynamic(this,&UTASettingsMenuWidget::DialogResult);
 else SettingsSubsystem->RevertVideoMode();
}
void UTASettingsMenuWidget::DialogResult(FName Result)
{
 ActiveDialog=nullptr;
 if (bVideoDialog)
 {
  if (Result==TEXT("Keep")) SettingsSubsystem->ConfirmVideoMode(); else SettingsSubsystem->RevertVideoMode();
  return;
 }
 if (Result==TEXT("Apply")) ApplyDraftAndContinue();
 else if (Result==TEXT("Discard")) { SettingsSubsystem->DiscardDraft(); CompleteExit(); }
 else { PendingExit=EPendingExit::None; PendingTarget=NAME_None; if (InputController.IsValid()) InputController->CancelSettingsCloseRequest(); }
}
void UTASettingsMenuWidget::VideoResolved(bool)
{
 if (!bVideoDialog) return;
 bVideoDialog=false;
 if (ActiveDialog) { ActiveDialog->Dismiss(); ActiveDialog=nullptr; }
 RefreshRows(); CompleteExit();
}
