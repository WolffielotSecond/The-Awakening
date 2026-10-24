#include "UI/TAInteractionPromptAnchor.h"
#include "UI/TAPromptSettings.h"
#include "UI/TAPromptWidgetUtils.h"
#include "UI/TAWorldPromptWidget.h"
#include "UI/TAQTEWidget.h"
#include "Interaction/TAInteractableActor.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

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
	RestoreChord();
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

void UTAInteractionPromptAnchor::RestoreChord()
{
	if (ChordSizeBox && ChordOriginalImage)
	{
		ChordOriginalImage->RemoveFromParent();
		ChordSizeBox->SetContent(ChordOriginalImage);
	}
	ChordSizeBox = nullptr; ChordOriginalImage = nullptr;
}

void UTAInteractionPromptAnchor::ShowPromptChord(const TArray<UTexture2D*>& Icons,
	const TArray<FText>& KeyLabels, const FText& PromptText, const FText& FirstIconSuffix)
{
	ShowPrompt(nullptr, PromptText);
	auto* PromptControl = GetUserWidgetObject();
	if (!PromptControl || !PromptControl->WidgetTree) return;
	auto* Image = Cast<UImage>(PromptControl->GetWidgetFromName(TEXT("Image_Key")));
	auto* Box = Cast<USizeBox>(PromptControl->GetWidgetFromName(TEXT("SizeBox_Key")));
	if (!Box && Image) Box = Cast<USizeBox>(Image->GetParent());
	if (!Box && Image && Image->GetParent())
	{
		// Older WBP_InteractPrompt puts Image_Key directly in a horizontal box.
		// Rebuild that local panel with copied slot templates, preserving order and styling.
		auto* Parent = Image->GetParent();
		const auto Children = Parent->GetAllChildren();
		TArray<UPanelSlot*> Slots;
		for (auto* Child : Children) Slots.Add(Child->Slot);
		Box = PromptControl->WidgetTree->ConstructWidget<USizeBox>();
		Parent->ClearChildren();
		for (int32 I = 0; I < Children.Num(); ++I)
			Parent->AddChild(Children[I] == Image ? Box : Children[I], Slots[I]);
		Box->SetContent(Image);
	}
	if (!Image || !Box || Box->GetContent() != Image) return;
	ChordOriginalImage = Image; ChordSizeBox = Box;
	auto* Row = PromptControl->WidgetTree->ConstructWidget<UHorizontalBox>();
	Image->RemoveFromParent(); Box->SetContent(Row);
	Box->ClearWidthOverride(); Box->ClearHeightOverride();
	const auto* Style = Cast<UTextBlock>(PromptControl->GetWidgetFromName(TEXT("Text_Prompt")));
	auto AddText = [&](const FText& Text)
	{
		auto* Label = PromptControl->WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(Text);
		if (Style) { Label->SetFont(Style->GetFont()); Label->SetColorAndOpacity(Style->GetColorAndOpacity()); }
		Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
	};
	for (int32 I = 0; I < Icons.Num(); ++I)
	{
		if (I) AddText(FText::FromString(TEXT(" + ")));
		if (Icons[I])
		{
			auto* IconBox = PromptControl->WidgetTree->ConstructWidget<USizeBox>();
			auto* Icon = PromptControl->WidgetTree->ConstructWidget<UImage>(); IconBox->SetContent(Icon);
			FTAPromptWidgetUtils::ApplyKeyIcon(Icon, Icons[I], GetDefault<UTAPromptSettings>()->IconHeight, IconBox);
			Row->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);
		}
		else if (KeyLabels.IsValidIndex(I)) AddText(KeyLabels[I]);
		if (!I && !FirstIconSuffix.IsEmpty()) AddText(FirstIconSuffix);
	}
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
