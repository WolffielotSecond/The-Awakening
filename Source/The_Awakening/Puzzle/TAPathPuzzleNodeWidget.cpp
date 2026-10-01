#include "Puzzle/TAPathPuzzleNodeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UTAPathPuzzleNodeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget)
	{
		Button_Node = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Node"));
		WidgetTree->RootWidget = Button_Node;
		FButtonStyle Style = Button_Node->GetStyle();
		Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.NormalPadding = FMargin(0); Style.PressedPadding = FMargin(0);
		Button_Node->SetStyle(Style);
		Button_Node->SetBackgroundColor(FLinearColor::White);
		Button_Node->SetColorAndOpacity(FLinearColor::White);
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
		Button_Node->AddChild(Layers);
		CastChecked<UButtonSlot>(Layers->Slot)->SetHorizontalAlignment(HAlign_Fill);
		CastChecked<UButtonSlot>(Layers->Slot)->SetVerticalAlignment(VAlign_Fill);
		Image_Node = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Node"));
		Layers->AddChildToOverlay(Image_Node)->SetHorizontalAlignment(HAlign_Fill);
		CastChecked<UOverlaySlot>(Image_Node->Slot)->SetVerticalAlignment(VAlign_Fill);
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
		UOverlaySlot* TextSlot = Layers->AddChildToOverlay(Texts);
		TextSlot->SetHorizontalAlignment(HAlign_Center); TextSlot->SetVerticalAlignment(VAlign_Center);
		Text_Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Label"));
		Text_Effect = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Effect"));
		Texts->AddChildToVerticalBox(Text_Label)->SetHorizontalAlignment(HAlign_Center);
		Texts->AddChildToVerticalBox(Text_Effect)->SetHorizontalAlignment(HAlign_Center);
		FSlateFontInfo Font = Text_Label->GetFont(); Font.Size = 14; Text_Label->SetFont(Font);
		Font.Size = 9; Text_Effect->SetFont(Font);
		Text_Label->SetColorAndOpacity(FLinearColor::Black); Text_Effect->SetColorAndOpacity(FLinearColor::Black);
		Image_Node->SetVisibility(ESlateVisibility::HitTestInvisible);
		Texts->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (Button_Node) Button_Node->OnClicked.AddUniqueDynamic(this, &UTAPathPuzzleNodeWidget::HandleClick);
}

void UTAPathPuzzleNodeWidget::Configure(int32 Index, const FText& Label, const FText& Effect, const FSlateBrush& Brush, bool bShowLabel)
{
	NodeIndex = Index;
	if (Image_Node) Image_Node->SetBrush(Brush);
	if (Text_Label)
	{
		Text_Label->SetText(Label);
		Text_Label->SetVisibility(bShowLabel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Text_Effect) Text_Effect->SetText(Effect);
}

void UTAPathPuzzleNodeWidget::HandleClick() { OnNodeClicked.Broadcast(NodeIndex); }

bool UTAPathPuzzleNodeWidget::ClickAtCursor(const FVector2D& ScreenPosition)
{
	if (!IsVisible() || !GetIsEnabled() || !Button_Node || !Button_Node->IsVisible() || !Button_Node->GetIsEnabled() ||
		!Button_Node->GetCachedGeometry().IsUnderLocation(ScreenPosition)) return false;
	HandleClick();
	return true;
}
