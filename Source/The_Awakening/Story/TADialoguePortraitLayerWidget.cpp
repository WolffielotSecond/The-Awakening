// Source/The_Awakening/Story/TADialoguePortraitLayerWidget.cpp
#include "Story/TADialoguePortraitLayerWidget.h"
#include "Story/TAPortraitWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

void UTADialoguePortraitLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureCanvasBinding();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UTADialoguePortraitLayerWidget::EnsureCanvasBinding()
{
	if (!Root)
	{
		Root = Cast<UCanvasPanel>(GetRootWidget());
	}
	if (!Root)
	{
		Root = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("Root")));
	}
	if (!Root)
	{
		UE_LOG(LogTemp, Error, TEXT("[Dialogue] WBP_DialoguePortraitLayer 的根控件必须是 Canvas Panel（名称 Root）"));
	}
}

bool UTADialoguePortraitLayerWidget::AddPortraitWidget(UTAPortraitWidget* PortraitWidget)
{
	if (!PortraitWidget)
	{
		return false;
	}
	EnsureCanvasBinding();
	if (!Root)
	{
		return false;
	}

	return Root->AddChildToCanvas(PortraitWidget) != nullptr;
}
