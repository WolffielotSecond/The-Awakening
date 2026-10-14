#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAPickupPanelWidget.generated.h"

class UTANearbyPickupComponent;
class UTAPickupRowWidget;
class UScrollBox;
class UImage;
class UTextBlock;

/** Layout and styling are supplied by the user's WBP. No input/focus ownership. */
UCLASS(Abstract, Blueprintable)
class THE_AWAKENING_API UTAPickupPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Pickup")
    void SetPickupComponent(UTANearbyPickupComponent* Component);
    UFUNCTION(BlueprintCallable, Category="Pickup")
    void ClearFeedback();
    UPROPERTY(BlueprintReadOnly, Category="Pickup")
    TObjectPtr<UTANearbyPickupComponent> PickupComponent;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup")
    TSubclassOf<UTAPickupRowWidget> RowWidgetClass;
    /** Top and bottom padding of each generated ScrollBox row; negative values tighten spacing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetRowVerticalPadding, Category="Pickup|Layout")
    float RowVerticalPadding = -15.f;
    UFUNCTION(BlueprintCallable, Category="Pickup|Layout")
    void SetRowVerticalPadding(float InPadding);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    UFUNCTION() void RefreshRows();
    UFUNCTION() void RefreshPresentation();
    UFUNCTION() void HandlePickupResult(int32 AddedCount, FText Message);
    UFUNCTION(BlueprintImplementableEvent, Category="Pickup")
    void OnPickupFeedback(int32 AddedCount, const FText& Message);

    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> Scroll_Items;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_PreviousKey;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_NextKey;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Previous;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Next;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Feedback;
    UPROPERTY(Transient) TArray<TObjectPtr<UTAPickupRowWidget>> Rows;
private:
    void UnbindComponent();
    void BindComponent();
    void RefreshFeedbackText();
    bool bHasFailureFeedback = false;
};
