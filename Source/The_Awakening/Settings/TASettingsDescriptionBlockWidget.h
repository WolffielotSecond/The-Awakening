#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/TASettingsTypes.h"
#include "TASettingsDescriptionBlockWidget.generated.h"
class UTextBlock;
class UImage;
class UScaleBox;
/** Blueprint owns typography and spacing; images scale to the available width. */
UCLASS(Abstract,Blueprintable)
class THE_AWAKENING_API UTASettingsDescriptionBlockWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void Configure(const FTASettingDescriptionBlock& Block,FText LocalizedText);
protected:
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Content;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UImage> Image_Content;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UScaleBox> ScaleBox_Image;
};
