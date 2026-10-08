#include "Settings/TASettingsDescriptionBlockWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
void UTASettingsDescriptionBlockWidget::Configure(const FTASettingDescriptionBlock& Block,FText LocalizedText)
{
 const bool IsText=Block.Type==ETASettingDescriptionBlockType::Text;
 const bool HasImage=!IsText && Block.Image;
 SetVisibility(ESlateVisibility::HitTestInvisible);
 if (Text_Content)
 {
  Text_Content->SetText(IsText?LocalizedText:FText::GetEmpty());
  Text_Content->SetVisibility(IsText?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 }
 if (Image_Content)
 {
  // Native dimensions give the ScaleBox the actual aspect ratio of each texture.
  Image_Content->SetBrushFromTexture(IsText?nullptr:Block.Image.Get(),true);
  Image_Content->SetVisibility(HasImage?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 }
 if (ScaleBox_Image)
 {
  ScaleBox_Image->SetStretch(EStretch::ScaleToFitX);
  ScaleBox_Image->SetStretchDirection(EStretchDirection::Both);
  ScaleBox_Image->SetVisibility(HasImage?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 }
}
