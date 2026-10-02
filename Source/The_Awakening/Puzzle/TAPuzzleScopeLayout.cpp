#include "Puzzle/TAPuzzleScopeLayout.h"
#include "Puzzle/TAPathPuzzleWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Materials/MaterialInterface.h"
#include "Brushes/SlateColorBrush.h"

void TAPuzzleScopeLayout::Build(UWidgetTree* Tree, const FTAPuzzleAppearance& Appearance)
{
	auto* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScreenRoot"));
	Tree->RootWidget = Root;
	auto Place = [](UCanvasPanel* Parent, UWidget* Widget, FVector2D Pos, FVector2D Size, int32 Z)
	{
		auto* Slot = Parent->AddChildToCanvas(Widget);
		Slot->SetPosition(Pos); Slot->SetSize(Size); Slot->SetZOrder(Z);
		return Slot;
	};
	auto Fill = [&Place](UCanvasPanel* Parent, UWidget* Widget, int32 Z)
	{
		auto* Slot = Place(Parent, Widget, FVector2D::ZeroVector, FVector2D::ZeroVector, Z);
		Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(FMargin(0));
	};
	auto Image = [Tree](const TCHAR* Name, FLinearColor Color, const TCHAR* MaterialPath = nullptr)
	{
		auto* Result = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Result->SetBrush(FSlateColorBrush(Color));
		if (MaterialPath) Result->SetBrushFromMaterial(LoadObject<UMaterialInterface>(nullptr, MaterialPath));
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	};
	Fill(Root, Image(TEXT("Image_Background"), FLinearColor(.025f,.035f,.055f,1)), 0);
	auto* Frame = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScopeFrame"));
	auto* FrameSlot = Place(Root, Frame, FVector2D::ZeroVector, FVector2D(600,600), 1);
	FrameSlot->SetAnchors(FAnchors(.5f)); FrameSlot->SetAlignment(FVector2D(.5f));
	auto* Board = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PuzzleCanvas"));
	Place(Frame, Board, FVector2D::ZeroVector, Appearance.BoardSize, 0);
	Board->SetVisibility(ESlateVisibility::HitTestInvisible);
	Board->SetRenderTransformPivot(FVector2D::ZeroVector);
	Fill(Root, Image(TEXT("Image_ScopeMask"), FLinearColor::Black, TEXT("/Game/UI/Minigame/Materials/M_PuzzleScopeMask.M_PuzzleScopeMask")), 2);
	// Instruments are a separate sibling, so the outside mask cannot cover the gauges or numbers.
	auto* Instruments = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScopeInstruments"));
	auto* InstrumentSlot = Place(Root, Instruments, FVector2D::ZeroVector, FVector2D(600,600), 3);
	InstrumentSlot->SetAnchors(FAnchors(.5f)); InstrumentSlot->SetAlignment(FVector2D(.5f));
	Fill(Instruments, Image(TEXT("Image_EnergyArc"), FLinearColor::White, TEXT("/Game/UI/Minigame/Materials/M_PuzzleEnergyArc.M_PuzzleEnergyArc")), 0);
	Fill(Instruments, Image(TEXT("Image_TimeArc"), FLinearColor::White, TEXT("/Game/UI/Minigame/Materials/M_PuzzleTimeArc.M_PuzzleTimeArc")), 0);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		auto* Number = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Side == 0 ? TEXT("Text_Energy") : TEXT("Text_Time"));
		Number->SetText(FText::FromString(Side == 0 ? TEXT("100") : TEXT("60")));
		Number->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Number->GetFont(); Font.Size = 24; Number->SetFont(Font);
		Number->SetColorAndOpacity(Side == 0 ? FLinearColor(1,.8f,.05f) : FLinearColor(.1f,.8f,.2f));
		Number->SetVisibility(ESlateVisibility::HitTestInvisible);
		// Place the values inside the lower ring gap; these remain editable in WBP.
		Place(Instruments, Number, FVector2D(Side == 0 ? 155 : 355, 485), FVector2D(90,45), 1);
	}
	// Optional, runtime-hidden until an undo is available. Position and size are
	// deliberately ordinary CanvasSlot values so they remain editable in WBP.
	auto* UndoButton = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Undo"));
	auto* UndoText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Undo"));
	UndoText->SetText(FText::FromString(TEXT("Undo")));
	UndoText->SetJustification(ETextJustify::Center);
	UndoButton->AddChild(UndoText);
	Place(Root, UndoButton, FVector2D::ZeroVector, FVector2D(170,50), 4)->SetAnchors(FAnchors(1,1));
	Cast<UCanvasPanelSlot>(UndoButton->Slot)->SetOffsets(FMargin(-200,-75,30,25));
	UndoButton->SetVisibility(ESlateVisibility::Collapsed);
	auto* Crosshair = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Crosshair"));
	auto* CrossSlot = Place(Instruments, Crosshair, FVector2D::ZeroVector, FVector2D(22,22), 2);
	CrossSlot->SetAnchors(FAnchors(.5f)); CrossSlot->SetAlignment(FVector2D(.5f));
	Crosshair->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Crosshair, Image(TEXT("CrosshairHorizontal"), FLinearColor::White), FVector2D(0,10), FVector2D(22,2), 0);
	Place(Crosshair, Image(TEXT("CrosshairVertical"), FLinearColor::White), FVector2D(10,0), FVector2D(2,22), 0);
	AddLocalizedPrompts(Tree);
}

void TAPuzzleScopeLayout::AddLocalizedPrompts(UWidgetTree* Tree)
{
	auto* Root = Cast<UCanvasPanel>(Tree->RootWidget);
	auto* Instruments = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("ScopeInstruments")));
	if (!Root || !Instruments) return;
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const FString Name = FString::Printf(TEXT("PromptRow_%d"), Row);
		if (Tree->FindWidget(FName(*Name))) continue;
		auto* Box = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*Name));
		Box->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* Slot = Root->AddChildToCanvas(Box);
		Slot->SetAnchors(FAnchors(1,1)); Slot->SetAlignment(FVector2D(1,1));
		Slot->SetPosition(FVector2D(-35,-35-(2-Row)*48)); Slot->SetSize(FVector2D(390,40)); Slot->SetZOrder(5);
		for (int32 I = 0; I < (Row == 1 ? 4 : 1); ++I)
		{
			auto* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("PromptIcon_%d_%d"), Row,I)));
			Box->AddChildToHorizontalBox(Icon)->SetPadding(FMargin(0,0,6,0));
		}
		auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("PromptText_%d"),Row)));
		Box->AddChildToHorizontalBox(Text)->SetVerticalAlignment(VAlign_Center);
	}
	for (int32 Side=0; Side<2; ++Side)
	{
		const FName Name = Side==0 ? TEXT("Text_EnergyLabel") : TEXT("Text_TimeLabel");
		if (Tree->FindWidget(Name)) continue;
		auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
		Text->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font=Text->GetFont(); Font.Size=16; Text->SetFont(Font);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* Slot=Instruments->AddChildToCanvas(Text);
		Slot->SetPosition(FVector2D(Side==0 ? 115 : 315,452)); Slot->SetSize(FVector2D(170,30)); Slot->SetZOrder(2);
	}
}
