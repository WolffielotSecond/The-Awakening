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
 if (Button_RestoreDefaults) Button_RestoreDefaults->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::RestoreClicked);
 if (Button_ConfirmVideoMode) Button_ConfirmVideoMode->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::ConfirmVideo);
 if (Button_RevertVideoMode) Button_RevertVideoMode->OnClicked.AddUniqueDynamic(this,&UTASettingsMenuWidget::RevertVideo);
 BuildPageEntries(); RefreshPageLabels(); BuildSettingRows(); BuildPromptBar();
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
 W->Configure(D,Text(D.NameTextId),Value,SettingsSubsystem?SettingsSubsystem->GetInteger(D.SettingId,0):0,SettingsSubsystem && SettingsSubsystem->IsFavorite(D.SettingId));
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
  if (SettingsSubsystem) if (const auto* D=SettingsSubsystem->FindDefinition(R->GetSettingId())) R->Configure(*D,Text(D->NameTextId),SettingsSubsystem->FormatValue(*D),SettingsSubsystem->GetInteger(D->SettingId,0),SettingsSubsystem->IsFavorite(D->SettingId));
  R->SetHighlighted(I==SelectedRowIndex);
 }
 const auto* D=SettingsSubsystem?SettingsSubsystem->FindDefinition(SelectedSettingId):nullptr;
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
 CurrentPageId=PageId; SelectedSettingId=NAME_None; SelectedRowIndex=INDEX_NONE; BuildSettingRows(); RefreshPageLabels();
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
 SelectedRowIndex=Index; SelectedSettingId=Rows[Index]->GetSettingId();
}
void UTASettingsMenuWidget::MoveSelection(int32 Dir)
{
 if (Rows.IsEmpty()) return;
 EnsureRowSelection(); SelectRow((SelectedRowIndex+Dir+Rows.Num())%Rows.Num()); RefreshRows();
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
{ if (Allows(ETAInputCapability::Navigate)) if (auto* R=Cast<UTASettingRowWidget>(W)) { SelectRow(Rows.IndexOfByKey(R)); RefreshRows(); } }
void UTASettingsMenuWidget::RowActivated(UTASettingRowWidget* R)
{ if (!R || IsCapturingPlayerInput() || !Allows(ETAInputCapability::Confirm)) return; SelectRow(Rows.IndexOfByKey(R)); ConfirmSelection(); RefreshRows(); }
void UTASettingsMenuWidget::RowAdjusted(UTASettingRowWidget* R,int32 Dir)
{ if (!R || !Allows(ETAInputCapability::Navigate)) return; SelectRow(Rows.IndexOfByKey(R)); AdjustSelectedValue(Dir); }
void UTASettingsMenuWidget::RowNumberChanged(UTASettingRowWidget* R,int32 N)
{ if (R && Allows(ETAInputCapability::Navigate) && SettingsSubsystem) { SelectRow(Rows.IndexOfByKey(R)); SettingsSubsystem->SetValue(R->GetSettingId(),N); } }
void UTASettingsMenuWidget::RowFavorite(UTASettingRowWidget* R)
{ if (R && Allows(ETAInputCapability::ToggleFavorite) && SettingsSubsystem) SettingsSubsystem->SetFavorite(R->GetSettingId(),!SettingsSubsystem->IsFavorite(R->GetSettingId())); }
void UTASettingsMenuWidget::ValueChanged(FName) { RefreshRows(); }
void UTASettingsMenuWidget::FavoritesChanged() { if (CurrentPageId==FTASettingsCatalog::FavoritesPageId()) BuildSettingRows(); else RefreshRows(); }
void UTASettingsMenuWidget::LanguageChanged() { RefreshPageLabels(); for (int32 I=0;I<SectionHeadings.Num();++I) SectionHeadings[I]->SetTitle(Text(SectionHeadingIds[I].ToString())); RefreshRows(); }
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
  Entry->Configure(D,Text(D.PageId.ToString()),D.PageId==CurrentPageId);
  Entry->OnPageActivated.AddDynamic(this,&UTASettingsMenuWidget::SelectPage);
 }
}
void UTASettingsMenuWidget::RefreshPageLabels()
{
 for (int32 I=0;I<PageEntries.Num();++I)
 {
  if (!PageEntries[I]) continue;
  // Entries are generated in the same order as definitions.
  const auto& D=PageDefinitions[I];
  PageEntries[I]->Configure(D,Text(D.PageId.ToString()),D.PageId==CurrentPageId);
 }
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
 if (A==InputController->GetSettingsAction(TEXT("Favorite"))) { RowFavorite(Rows.IsValidIndex(SelectedRowIndex)?Rows[SelectedRowIndex].Get():nullptr); return; }
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
 if (C==ETAInputCapability::ToggleFavorite) { RowFavorite(Rows.IsValidIndex(SelectedRowIndex)?Rows[SelectedRowIndex].Get():nullptr); return; }
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
 TArray<FName> Ids; for (const UTASettingRowWidget* R:Rows) Ids.AddUnique(R->GetSettingId());
 for(FName Id:Ids) SettingsSubsystem->RestoreSettingDefault(Id);
}
void UTASettingsMenuWidget::RestoreClicked() { if (Allows(ETAInputCapability::Confirm)) RestoreCurrentDefaults(); }
void UTASettingsMenuWidget::ConfirmVideo() { if (Allows(ETAInputCapability::Confirm) && SettingsSubsystem) SettingsSubsystem->ConfirmVideoMode(); }
void UTASettingsMenuWidget::RevertVideo() { if (Allows(ETAInputCapability::Confirm) && SettingsSubsystem) SettingsSubsystem->RevertVideoMode(); }
