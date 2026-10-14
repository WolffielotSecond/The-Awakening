#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAPickupRowWidget.generated.h"

class ATAWorldItem;
class UTANearbyPickupComponent;
class UImage;
class UTextBlock;
class UBorder;
class UVerticalBox;

UCLASS(Abstract, Blueprintable)
class THE_AWAKENING_API UTAPickupRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetItem(ATAWorldItem* Item, UTANearbyPickupComponent* Component, bool bSelected);
    UFUNCTION(BlueprintCallable, Category="Pickup") void RefreshPresentation();
    UPROPERTY(BlueprintReadOnly, Category="Pickup") TWeakObjectPtr<ATAWorldItem> WorldItem;
    UPROPERTY(BlueprintReadOnly, Category="Pickup") bool bIsSelected = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup") FLinearColor SelectedColor = FLinearColor(0.3f, 0.5f, 0.8f, 0.8f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup") FLinearColor NormalColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.7f);
protected:
    UFUNCTION(BlueprintImplementableEvent, Category="Pickup") void OnPresentationChanged();
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_Item;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_InteractKey;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> VerticalBox_Keys;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_PreviousKey;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_NextKey;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Name;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Count;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Interact;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_Selection;
private:
    TWeakObjectPtr<UTANearbyPickupComponent> PickupComponent;
};
