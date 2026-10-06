#include "UI/TASelectableMenuOptionWidget.h"

void UTASelectableMenuOptionWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	OnHovered.Broadcast(this);
}
