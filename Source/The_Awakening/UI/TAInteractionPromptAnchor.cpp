#include "UI/TAInteractionPromptAnchor.h"
#include "UI/TAPromptSettings.h"
#include "UI/TAPromptWidgetUtils.h"
#include "UI/TAWorldPromptWidget.h"
#include "UI/TAQTEWidget.h"
#include "Interaction/TAInteractableActor.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"

UTAInteractionPromptAnchor::UTAInteractionPromptAnchor()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetVisibility(false);
	// 提示与冻结无关：冻结时由提示系统主动隐藏，不靠停 Tick
	ComponentTags.AddUnique(TEXT("FreezeExempt"));
}

FString UTAInteractionPromptAnchor::ResolveTextId() const
{
	if (!TextId.IsEmpty())
	{
		return TextId;
	}
	if (const ATAInteractableActor* Interactable = Cast<ATAInteractableActor>(GetOwner()))
	{
		const FString OwnerTextId = Interactable->GetPromptTextId();
		if (!OwnerTextId.IsEmpty())
		{
			return OwnerTextId;
		}
	}
	return GetDefault<UTAPromptSettings>()->GetDefaultTextId(PromptType);
}

bool UTAInteractionPromptAnchor::EnsurePromptWidget()
{
	if (!GetWidgetClass())
	{
		if (UClass* DefaultClass = GetDefault<UTAPromptSettings>()->DefaultPromptWidgetClass.LoadSynchronous())
		{
			SetWidgetClass(DefaultClass);
		}
	}
	if (!GetUserWidgetObject() && GetWidgetClass())
	{
		InitWidget();
	}
	return GetUserWidgetObject() != nullptr;
}

void UTAInteractionPromptAnchor::ShowPrompt(UTexture2D* KeyIcon, const FText& PromptText)
{
	RestoreNormalWidget();
	if (!EnsurePromptWidget())
	{
		return;
	}
	const float IconHeight = GetDefault<UTAPromptSettings>()->IconHeight;
	if (UTAWorldPromptWidget* PromptWidget = Cast<UTAWorldPromptWidget>(GetUserWidgetObject()))
	{
		PromptWidget->SetPrompt(KeyIcon, PromptText, IconHeight);
	}
	else
	{
		// 父类不是 UTAWorldPromptWidget 的旧 WBP：按控件名填充
		FTAPromptWidgetUtils::ApplyPrompt(GetUserWidgetObject(), KeyIcon, PromptText, IconHeight);
	}
	SetVisibility(true);
	bPromptShown = true;
}

void UTAInteractionPromptAnchor::ShowQTE(UTASpecialActionComponent* Action)
{
 if (!bQTEWidget)
 {
  PreviousWidgetClass = GetWidgetClass(); bQTEWidget = true;
  SetWidgetClass(UTAQTEWidget::StaticClass()); InitWidget();
 }
 if (auto* QTE = Cast<UTAQTEWidget>(GetUserWidgetObject())) QTE->Observe(Action);
 SetVisibility(true); bPromptShown = true;
}
void UTAInteractionPromptAnchor::RestoreNormalWidget()
{
 if (!bQTEWidget) return;
 bQTEWidget = false; SetWidgetClass(PreviousWidgetClass); PreviousWidgetClass = nullptr;
}
void UTAInteractionPromptAnchor::HidePrompt()
{
	SetVisibility(false);
	bPromptShown = false;
	RestoreNormalWidget();
}

UTAInteractionPromptAnchor* UTAInteractionPromptAnchor::FindAnchor(const AActor* Actor, ETAPromptType Type)
{
	if (!Actor)
	{
		return nullptr;
	}
	TInlineComponentArray<UTAInteractionPromptAnchor*> Anchors(Actor);
	for (UTAInteractionPromptAnchor* Anchor : Anchors)
	{
		if (Anchor && Anchor->PromptType == Type)
		{
			return Anchor;
		}
	}
	return nullptr;
}
