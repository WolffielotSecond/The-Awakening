#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "TAPathPuzzleNodeWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAOnPuzzleNodeClicked, int32, NodeIndex);

/** Image-based node with a transparent button hit target. Optional Blueprint art/layout override. */
UCLASS()
class THE_AWAKENING_API UTAPathPuzzleNodeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Configure(int32 Index, const FText& Label, const FText& Effect, const FSlateBrush& Brush, bool bShowLabel = false);
	bool ClickAtCursor(const FVector2D& ScreenPosition);
	UPROPERTY(BlueprintAssignable, Category="Puzzle") FTAOnPuzzleNodeClicked OnNodeClicked;
protected:
	virtual void NativeOnInitialized() override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Node;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_Node;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Label;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Effect;
private:
	void EnsureNativeWidgetTree();
	UFUNCTION() void HandleClick();
	int32 NodeIndex = INDEX_NONE;
};
