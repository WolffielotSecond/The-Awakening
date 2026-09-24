// Source/The_Awakening/Story/TADialoguePortraitLayerWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TADialoguePortraitLayerWidget.generated.h"

class UCanvasPanel;
class UTAPortraitWidget;

/** Separate full-screen WBP layer that hosts dialogue portraits behind the main dialogue UI. */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTADialoguePortraitLayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	bool AddPortraitWidget(UTAPortraitWidget* PortraitWidget);

protected:
	void EnsureCanvasBinding();

	/** The root Canvas Panel of WBP_DialoguePortraitLayer. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> Root;
};
