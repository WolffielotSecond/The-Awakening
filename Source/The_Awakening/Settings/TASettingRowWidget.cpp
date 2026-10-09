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
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
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
 // The existing row has an Auto name, a Fill spacer and Auto controls. Constrain
 // only the English name; controls retain their authored widths and slot sizing.
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
 Text_Name->SetAutoWrapText(bEnglish || bAuthoredNameWrap);
 NameSlot->SetSize(bEnglish?FSlateChildSize(ESlateSizeRule::Fill):Original.Size);
 FMargin NamePadding=Original.Padding;
 if (bEnglish) NamePadding.Right=FMath::Max(NamePadding.Right,24.f);
 if (bEnglish && Slider_Value)
 {
  NamePadding.Top=FMath::Max(NamePadding.Top,12.f);
  NamePadding.Bottom=FMath::Max(NamePadding.Bottom,12.f);
 }
 NameSlot->SetPadding(NamePadding);
 NameSlot->SetHorizontalAlignment(bEnglish?HAlign_Fill:Original.Horizontal);
 NameSlot->SetVerticalAlignment(bEnglish?VAlign_Center:Original.Vertical);
 if (Spacer) Spacer->SetVisibility(bEnglish?ESlateVisibility::Collapsed:AuthoredSpacerVisibility);
 for (const auto& Entry:AuthoredControlAlignment)
  if (auto* ControlSlot=Entry.Key.Get()) ControlSlot->SetVerticalAlignment(bEnglish?VAlign_Center:Entry.Value);
 // The stretch Canvas does not propagate its text's desired height. Reserve
 // three font lines in the existing slider SizeBox; keep the control widths.
 if (Slider_Value)
  if (auto* Root=Cast<USizeBox>(GetRootWidget()); Root && Root->IsHeightOverride())
  {
   if (!AuthoredSliderHeight.IsSet()) AuthoredSliderHeight=Root->GetHeightOverride();
   float Height=AuthoredSliderHeight.GetValue();
   if (bEnglish && FSlateApplication::IsInitialized())
   {
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    float Insets=NamePadding.Top+NamePadding.Bottom;
    if (auto* RootSlot=Cast<USizeBoxSlot>(Root->GetContent()->Slot)) Insets+=RootSlot->GetPadding().Top+RootSlot->GetPadding().Bottom;
    if (auto* CanvasSlot=Cast<UCanvasPanelSlot>(Button_Option->Slot)) Insets+=CanvasSlot->GetOffsets().Top+CanvasSlot->GetOffsets().Bottom;
    if (auto* ContentSlot=Cast<UButtonSlot>(Row->Slot)) Insets+=ContentSlot->GetPadding().Top+ContentSlot->GetPadding().Bottom;
    Insets+=Button_Option->GetStyle().NormalPadding.Top+Button_Option->GetStyle().NormalPadding.Bottom;
    const float LineHeight=Measure->GetMaxCharacterHeight(Text_Name->GetFont());
    Height=FMath::Max(Height,3.f*LineHeight+Insets);
   }
   Root->SetHeightOverride(Height);
  }
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
