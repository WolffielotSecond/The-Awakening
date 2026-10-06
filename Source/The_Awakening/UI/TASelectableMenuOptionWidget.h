#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TASelectableMenuOptionWidget.generated.h"

class UTASelectableMenuOptionWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTASelectableMenuOptionHovered,
	UTASelectableMenuOptionWidget*,
	OptionWidget);

/** Reusable menu row base that reports pointer hover to its owning menu. */
UCLASS(Abstract, Blueprintable)
class THE_AWAKENING_API UTASelectableMenuOptionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Menu|Selection")
	FTASelectableMenuOptionHovered OnHovered;

protected:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};
