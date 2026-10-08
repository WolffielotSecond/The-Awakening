#include "Settings/TASettingRowWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
void UTASettingRowWidget::NativeOnInitialized()
{
 Super::NativeOnInitialized();
 if (Button_Option) { NormalColor=Button_Option->GetBackgroundColor(); Button_Option->OnClicked.AddDynamic(this,&UTASettingRowWidget::Activate); }
 if (Button_Decrease) Button_Decrease->OnClicked.AddDynamic(this,&UTASettingRowWidget::Decrease);
 if (Button_Increase) Button_Increase->OnClicked.AddDynamic(this,&UTASettingRowWidget::Increase);
 if (Button_Favorite) Button_Favorite->OnClicked.AddDynamic(this,&UTASettingRowWidget::Favorite);
 if (Slider_Value) Slider_Value->OnValueChanged.AddDynamic(this,&UTASettingRowWidget::NumberChanged);
}
void UTASettingRowWidget::Configure(const FTASettingDefinition& D,FText Name,FText Value,int32 Number,bool B)
{
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
void UTASettingRowWidget::SetHighlighted(bool B)
{
 if (Button_Option) Button_Option->SetBackgroundColor(B?FLinearColor(0.12f,0.42f,0.82f,1):NormalColor);
 if (Image_Highlight) Image_Highlight->SetVisibility(B?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
}
UWidget* UTASettingRowWidget::GetFocusTarget() const { return Button_Option; }
void UTASettingRowWidget::Activate() { OnActivated.Broadcast(this); }
void UTASettingRowWidget::Decrease() { OnAdjusted.Broadcast(this,-1); }
void UTASettingRowWidget::Increase() { OnAdjusted.Broadcast(this,1); }
void UTASettingRowWidget::Favorite() { OnFavorite.Broadcast(this); }
void UTASettingRowWidget::NumberChanged(float V) { if (!bRefreshing && FMath::IsFinite(V)) OnNumberChanged.Broadcast(this,FMath::Clamp(FMath::RoundToInt(V),1,100)); }
