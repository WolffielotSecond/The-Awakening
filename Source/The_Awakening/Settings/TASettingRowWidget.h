#pragma once
#include "CoreMinimal.h"
#include "UI/TASelectableMenuOptionWidget.h"
#include "Settings/TASettingsTypes.h"
#include "TASettingRowWidget.generated.h"
class UButton;
class UTextBlock;
class USlider;
class UTexture2D;
class UTASettingRowWidget;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTASettingRowActivated,UTASettingRowWidget*,Row);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTASettingRowAdjusted,UTASettingRowWidget*,Row,int32,Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTASettingRowNumberChanged,UTASettingRowWidget*,Row,int32,Number);
UCLASS(Abstract,Blueprintable)
class THE_AWAKENING_API UTASettingRowWidget : public UTASelectableMenuOptionWidget
{
 GENERATED_BODY()
public:
 void Configure(const FTASettingDefinition& Definition,FText Name,FText Value,int32 Number,bool bFavorite);
 void SetHighlighted(bool B);
 UFUNCTION(BlueprintPure,Category="Settings") FName GetSettingId() const { return Definition.SettingId; }
 UWidget* GetFocusTarget() const;
 UPROPERTY(BlueprintAssignable) FTASettingRowActivated OnActivated;
 UPROPERTY(BlueprintAssignable) FTASettingRowActivated OnFavorite;
 UPROPERTY(BlueprintAssignable) FTASettingRowAdjusted OnAdjusted;
 UPROPERTY(BlueprintAssignable) FTASettingRowNumberChanged OnNumberChanged;
protected:
 virtual void NativeOnInitialized() override;
 // Assign both textures in the row Blueprint to show a clickable star in either state.
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Settings|Favorite") TObjectPtr<UTexture2D> FavoriteIconTexture;
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Settings|Favorite") TObjectPtr<UTexture2D> NotFavoriteIconTexture;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Option;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Name;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Value;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Decrease;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Increase;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Favorite;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<USlider> Slider_Value;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> Image_Highlight;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> Image_Favorite;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> Image_ToggleThumb;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UImage> Image_ToggleBackground;
private:
 UFUNCTION() void Activate();
 UFUNCTION() void Decrease();
 UFUNCTION() void Increase();
 UFUNCTION() void Favorite();
 UFUNCTION() void NumberChanged(float Value);
 FTASettingDefinition Definition;
 bool bRefreshing=false;
 FLinearColor NormalColor=FLinearColor::White;
};
// Separate Blueprint templates can be assigned for each type, sharing the interaction contract.
UCLASS(Abstract,Blueprintable) class THE_AWAKENING_API UTAToggleSettingRowWidget : public UTASettingRowWidget { GENERATED_BODY() };
UCLASS(Abstract,Blueprintable) class THE_AWAKENING_API UTAChoiceSettingRowWidget : public UTASettingRowWidget { GENERATED_BODY() };
UCLASS(Abstract,Blueprintable) class THE_AWAKENING_API UTASliderSettingRowWidget : public UTASettingRowWidget { GENERATED_BODY() };
UCLASS(Abstract,Blueprintable) class THE_AWAKENING_API UTASubmenuSettingRowWidget : public UTASettingRowWidget { GENERATED_BODY() };
