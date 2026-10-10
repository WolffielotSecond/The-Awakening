#include "Settings/TASettingRowWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "Blueprint/WidgetTree.h"
void UTASettingRowWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 if (Button_Option) Button_Option->OnClicked.AddDynamic(this,&UTASettingRowWidget::Activate);
 if (Button_Decrease) Button_Decrease->OnClicked.AddDynamic(this,&UTASettingRowWidget::Decrease);
 if (Button_Increase) Button_Increase->OnClicked.AddDynamic(this,&UTASettingRowWidget::Increase);
 if (Button_Favorite) Button_Favorite->OnClicked.AddDynamic(this,&UTASettingRowWidget::Favorite);
 // Keep each authored Normal brush for pressed state; selection is a separate highlight.
 for (UButton* Button:{Button_Option.Get(),Button_Decrease.Get(),Button_Increase.Get(),Button_Favorite.Get()})
  if (Button) { auto Style=Button->GetStyle(); Style.Pressed=Style.Normal; Style.Hovered=Style.Normal; Button->SetStyle(Style); }
 if (Slider_Value) Slider_Value->OnValueChanged.AddDynamic(this,&UTASettingRowWidget::NumberChanged);
}
void UTASettingRowWidget::Configure(const FTASettingDefinition& D,FText Name,FText Value,int32 Number,bool B,bool bEnglishLayout)
{
 ApplyLanguageLayout(bEnglishLayout);
 TGuardValue<bool> Guard(bRefreshing,true); Definition=D;
 if (Text_Name) Text_Name->SetText(Name);
 if (Text_Value) Text_Value->SetText(Value);
 if (D.Type==ETASettingType::Toggle)
 {
  const bool Enabled=Number==1;
  if (Image_ToggleThumb)
  {
   if (auto* ThumbSlot=Cast<UOverlaySlot>(Image_ToggleThumb->Slot))
    ThumbSlot->SetHorizontalAlignment(Enabled?HAlign_Right:HAlign_Left);
   Image_ToggleThumb->SetVisibility(ESlateVisibility::HitTestInvisible);
  }
  if (Image_ToggleBackground) Image_ToggleBackground->SetVisibility(Enabled?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
 }
 const bool Adjustable=D.Type==ETASettingType::Choice || D.Type==ETASettingType::Slider;
 if (Button_Decrease) Button_Decrease->SetVisibility(Adjustable?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if (Button_Increase) Button_Increase->SetVisibility(Adjustable?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if (Slider_Value)
 {
  Slider_Value->SetVisibility(D.Type==ETASettingType::Slider?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
  if (D.Type==ETASettingType::Slider) { Slider_Value->SetMinValue(1); Slider_Value->SetMaxValue(100); Slider_Value->SetStepSize(D.Step); Slider_Value->SetValue(Number); }
 }
 const bool CanFavorite=D.bCanFavorite && !D.bSubmenuOnly;
 if (Image_Favorite)
 {
  if (FavoriteIconTexture && NotFavoriteIconTexture)
  {
   Image_Favorite->SetBrushFromTexture(B?FavoriteIconTexture.Get():NotFavoriteIconTexture.Get(),false);
   Image_Favorite->SetVisibility(CanFavorite?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
  }
  else Image_Favorite->SetVisibility(CanFavorite && B?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 }
 if (Button_Favorite) Button_Favorite->SetVisibility(CanFavorite?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void UTASettingRowWidget::ApplyLanguageLayout(bool bEnglish)
{
 if (!Text_Name) return;
 auto* NameSlot=Cast<UHorizontalBoxSlot>(Text_Name->Slot);
 auto* Row=Cast<UHorizontalBox>(Text_Name->GetParent());
 if (!NameSlot || !Row) return;
 // Sliders use the same width constraints in every language. Other row types
 // retain their existing English-only adjustments.
 const bool bConstrained=bEnglish || Slider_Value!=nullptr;
 auto* Spacer=Cast<USpacer>(GetWidgetFromName(TEXT("Spacer_143")));
 if (!AuthoredNameLayout.IsSet())
 {
  AuthoredNameLayout=FNameLayout{NameSlot->GetSize(),NameSlot->GetPadding(),NameSlot->GetHorizontalAlignment(),NameSlot->GetVerticalAlignment()};
  bAuthoredNameWrap=Text_Name->GetAutoWrapText();
  if (Spacer) AuthoredSpacerVisibility=Spacer->GetVisibility();
  for (UWidget* Child:Row->GetAllChildren())
   if (Child!=Text_Name && Child!=Spacer)
    if (auto* ControlSlot=Cast<UHorizontalBoxSlot>(Child->Slot)) AuthoredControlAlignment.Add(ControlSlot,ControlSlot->GetVerticalAlignment());
 }
 const auto& Original=AuthoredNameLayout.GetValue();
 Text_Name->SetAutoWrapText(bConstrained || bAuthoredNameWrap);
 NameSlot->SetSize(bConstrained?FSlateChildSize(ESlateSizeRule::Fill):Original.Size);
 FMargin NamePadding=Original.Padding;
 if (bConstrained) NamePadding.Right=FMath::Max(NamePadding.Right,24.f);
 if (Slider_Value)
 {
  NamePadding.Top=FMath::Max(NamePadding.Top,12.f);
  NamePadding.Bottom=FMath::Max(NamePadding.Bottom,12.f);
 }
 NameSlot->SetPadding(NamePadding);
 NameSlot->SetHorizontalAlignment(bConstrained?HAlign_Fill:Original.Horizontal);
 NameSlot->SetVerticalAlignment(bConstrained?VAlign_Center:Original.Vertical);
 if (Spacer) Spacer->SetVisibility(bConstrained?ESlateVisibility::Collapsed:AuthoredSpacerVisibility);
 for (const auto& Entry:AuthoredControlAlignment)
  if (auto* ControlSlot=Entry.Key.Get()) ControlSlot->SetVerticalAlignment(bConstrained?VAlign_Center:Entry.Value);
 if (Slider_Value) ApplySliderLayout();
}
void UTASettingRowWidget::ApplySliderLayout()
{
 auto* Root=Cast<USizeBox>(GetRootWidget());
 auto* Operations=Cast<UOverlay>(GetWidgetFromName(TEXT("Overlay_125")));
 auto* Controls=Cast<UHorizontalBox>(GetWidgetFromName(TEXT("HorizontalBox_150")));
 auto* SliderBox=Cast<USizeBox>(GetWidgetFromName(TEXT("SizeBox_2")));
 auto* OperationsSlot=Operations?Cast<UHorizontalBoxSlot>(Operations->Slot):nullptr;
 auto* ControlsSlot=Controls?Cast<UOverlaySlot>(Controls->Slot):nullptr;
 auto* ValueSlot=Text_Value?Cast<UHorizontalBoxSlot>(Text_Value->Slot):nullptr;
 auto* SliderSlot=SliderBox?Cast<UHorizontalBoxSlot>(SliderBox->Slot):nullptr;
 if (!Root || !OperationsSlot || !ControlsSlot || !ValueSlot || !SliderSlot || !Image_Highlight || !WidgetTree) return;
 if (!SliderContentLayout)
 {
  auto* Canvas=Cast<UCanvasPanel>(Root->GetContent());
  auto* ButtonSlot=Cast<UCanvasPanelSlot>(Button_Option->Slot);
  auto* HighlightSlot=Cast<UCanvasPanelSlot>(Image_Highlight->Slot);
  if (!Canvas || !ButtonSlot || !HighlightSlot) return;
  const auto ButtonOffsets=ButtonSlot->GetOffsets();
  const auto RootPadding=Cast<USizeBoxSlot>(Root->GetContent()->Slot)->GetPadding();
  SliderContentLayout=WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("SliderContentLayout"));
  SliderContentLayout->SetVisibility(Canvas->GetVisibility());
  SliderContentLayout->SetCursor(Canvas->GetCursor());
  Button_Option->RemoveFromParent(); Image_Highlight->RemoveFromParent();
  auto* NewHighlightSlot=SliderContentLayout->AddChildToOverlay(Image_Highlight);
  NewHighlightSlot->SetHorizontalAlignment(HAlign_Fill); NewHighlightSlot->SetVerticalAlignment(VAlign_Fill);
  auto* ButtonOverlaySlot=SliderContentLayout->AddChildToOverlay(Button_Option);
  ButtonOverlaySlot->SetHorizontalAlignment(HAlign_Fill); ButtonOverlaySlot->SetVerticalAlignment(VAlign_Fill);
  ButtonOverlaySlot->SetPadding(FMargin(ButtonOffsets.Left,ButtonOffsets.Top,ButtonOffsets.Right,ButtonOffsets.Bottom));
  Root->SetContent(SliderContentLayout);
  Cast<USizeBoxSlot>(Root->GetContent()->Slot)->SetPadding(RootPadding);
 }
  // Split the space after the star: label 1 share, operations 2 shares.
  // Within operations, arrows remain Auto; value gets two shares and slider one.
  FSlateChildSize OperationsFill(ESlateSizeRule::Fill); OperationsFill.Value=2.f;
  OperationsSlot->SetSize(OperationsFill);
  ControlsSlot->SetHorizontalAlignment(HAlign_Fill);
  FSlateChildSize ValueFill(ESlateSizeRule::Fill); ValueFill.Value=2.f;
  ValueSlot->SetSize(ValueFill); SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  ValueSlot->SetHorizontalAlignment(HAlign_Fill); SliderSlot->SetHorizontalAlignment(HAlign_Fill);
  Text_Value->SetAutoWrapText(true);
  Text_Value->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
  SliderBox->ClearWidthOverride(); Root->ClearHeightOverride();
}
void UTASettingRowWidget::SetHighlighted(bool B)
{
 if (Image_Highlight) Image_Highlight->SetVisibility(B?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
}
UWidget* UTASettingRowWidget::GetFocusTarget() const { return Button_Option; }
void UTASettingRowWidget::Activate() { OnActivated.Broadcast(this); }
void UTASettingRowWidget::Decrease() { OnAdjusted.Broadcast(this,-1); }
void UTASettingRowWidget::Increase() { OnAdjusted.Broadcast(this,1); }
void UTASettingRowWidget::Favorite() { OnFavorite.Broadcast(this); }
void UTASettingRowWidget::NumberChanged(float V) { if (!bRefreshing && FMath::IsFinite(V)) OnNumberChanged.Broadcast(this,FMath::Clamp(FMath::RoundToInt(V),1,100)); }
