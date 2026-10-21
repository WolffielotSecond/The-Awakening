#include "UI/Document/TADocumentReaderWidget.h"
#include "UI/Document/TAJustifiedRichTextBlock.h"
#include "UI/Document/TAJustifiedTextLayout.h"
#include "Framework/Text/ILayoutBlock.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAReaderTranslationLayoutTest, "TheAwakening.Document.Reader.TranslationPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAReaderTranslationLayoutTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	UClass* ReaderClass = LoadClass<UTADocumentReaderWidget>(nullptr,
		TEXT("/Game/UI/Document/WBP_DocumentReader.WBP_DocumentReader_C"));
	auto* Reader = ReaderClass ? CreateWidget<UTADocumentReaderWidget>(World, ReaderClass) : nullptr;
	if (TestNotNull(TEXT("Existing reader Blueprint initializes"), Reader))
	{
		Reader->SetupTranslationPresentation();
		auto* Border = Cast<UBorder>(Reader->Panel_Translation);
		auto* TextSlot = Reader->Panel_Translation ? Cast<UCanvasPanelSlot>(Reader->Panel_Translation->Slot) : nullptr;
		auto* BlurSlot = Reader->TranslationBlur ? Cast<UCanvasPanelSlot>(Reader->TranslationBlur->Slot) : nullptr;
		auto* DimSlot = Reader->TranslationDim ? Cast<UCanvasPanelSlot>(Reader->TranslationDim->Slot) : nullptr;
		auto* ControlsSlot = Reader->HorizontalBox_Controls ? Cast<UCanvasPanelSlot>(Reader->HorizontalBox_Controls->Slot) : nullptr;
		if (TestNotNull(TEXT("Original translation border"), Border) && TestNotNull(TEXT("Translation slot"), TextSlot)
			&& TestNotNull(TEXT("Blur slot"), BlurSlot) && TestNotNull(TEXT("Dim slot"), DimSlot)
			&& TestNotNull(TEXT("Controls slot"), ControlsSlot))
		{
			TestEqual(TEXT("Translation has no opaque box"), Border->GetBrushColor().A, 0.f);
			TestTrue(TEXT("Text area centered horizontally"), FMath::IsNearlyEqual(TextSlot->GetAnchors().Minimum.X + TextSlot->GetAnchors().Maximum.X, 1.f));
			TestTrue(TEXT("Text area centered vertically"), FMath::IsNearlyEqual(TextSlot->GetAnchors().Minimum.Y + TextSlot->GetAnchors().Maximum.Y, 1.f));
			TestTrue(TEXT("Text area is exactly one third of the screen width"), FMath::IsNearlyEqual(TextSlot->GetAnchors().Maximum.X - TextSlot->GetAnchors().Minimum.X, 1.f / 3.f));
			TestTrue(TEXT("Existing Blueprint body upgraded to fully justified text"), Reader->RichText_Body->IsA<UTAJustifiedRichTextBlock>());
			TestTrue(TEXT("Short translation content vertically centered"), Border->GetVerticalAlignment() == VAlign_Center);
			TestTrue(TEXT("Blur and dim below text and bottom controls"), BlurSlot->GetZOrder() < DimSlot->GetZOrder()
				&& DimSlot->GetZOrder() < TextSlot->GetZOrder() && TextSlot->GetZOrder() < ControlsSlot->GetZOrder());
			auto* Canvas = Cast<UCanvasPanel>(Reader->Panel_Translation->GetParent());
			UWidget* Model = Reader->Image_Model;
			while (Model && Model->GetParent() != Canvas) Model = Model->GetParent();
			auto* ModelSlot = Model ? Cast<UCanvasPanelSlot>(Model->Slot) : nullptr;
			TestTrue(TEXT("Blur covers paper preview too"), ModelSlot && ModelSlot->GetZOrder() < BlurSlot->GetZOrder());
			const int32 ChildCount = Canvas->GetChildrenCount();
			Reader->SetupTranslationPresentation();
			TestEqual(TEXT("Reconstruction reuses backdrop widgets"), Canvas->GetChildrenCount(), ChildCount);
			Reader->HorizontalBox_Controls->SetRenderOpacity(1.f);
			Reader->Phase = UTADocumentReaderWidget::EPhase::Open;
			Reader->TranslationFadeDuration = 0.2f;
			Reader->TickTranslation(0.f);
			TestTrue(TEXT("Backdrop initially collapsed"), Reader->TranslationBlur->GetVisibility() == ESlateVisibility::Collapsed);
			Reader->ToggleTranslation();
			Reader->TickTranslation(0.1f);
			TestEqual(TEXT("Text fades in halfway"), Reader->Panel_Translation->GetRenderOpacity(), 0.5f);
			TestEqual(TEXT("Blur synchronized with text"), Reader->TranslationBlur->GetRenderOpacity(), 0.5f);
			TestEqual(TEXT("Dim synchronized with text"), Reader->TranslationDim->GetRenderOpacity(), 0.5f);
			TestEqual(TEXT("Bottom controls keep full opacity"), Reader->HorizontalBox_Controls->GetRenderOpacity(), 1.f);
			TestTrue(TEXT("Backdrop cannot intercept control clicks"), Reader->TranslationDim->GetVisibility() == ESlateVisibility::HitTestInvisible);
			Reader->TickTranslation(0.1f);
			TestEqual(TEXT("Translation fully shown"), Reader->Panel_Translation->GetRenderOpacity(), 1.f);
			Reader->ToggleTranslation();
			Reader->TickTranslation(0.2f);
			TestTrue(TEXT("Text collapsed after fade out"), Reader->Panel_Translation->GetVisibility() == ESlateVisibility::Collapsed);
			TestTrue(TEXT("Blur removed after fade out"), Reader->TranslationBlur->GetVisibility() == ESlateVisibility::Collapsed);
			TestTrue(TEXT("Dim removed after fade out"), Reader->TranslationDim->GetVisibility() == ESlateVisibility::Collapsed);
			Reader->TranslationFadeDuration = 0.f;
			Reader->ToggleTranslation();
			TestEqual(TEXT("Zero-duration toggle applies immediately"), Reader->Panel_Translation->GetRenderOpacity(), 1.f);
			Reader->RequestClose();
			Reader->TickTranslation(0.f);
			TestTrue(TEXT("Close clears translation"), !Reader->IsTranslationVisible());
		}
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);

	// Check actual laid-out glyph positions, including English word gaps and Chinese line breaks.
	const auto Owner = SNew(SBox);
	const FTextBlockStyle Style = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
	for (const FString& Text : {FString(TEXT("The letter describes a long journey through the quiet forest and across the distant mountains. Every paragraph ends naturally.")),
		FString(TEXT("这是一封记录旅途见闻的信件，文字应当在自动换行后沿着左右边界整齐排列，同时保持标点符号和段落末行的自然间距。"))})
	{
		const auto Layout = MakeShared<FTAJustifiedTextLayout>(&Owner.Get(), Style);
		FPlainTextLayoutMarshaller::Create()->SetText(Text, *Layout);
		Layout->SetJustification(ETextJustify::Left);
		for (float Width : {200.f, 300.f})
		{
			Layout->SetWrappingWidth(Width);
			Layout->UpdateIfNeeded();
			const auto& Lines = Layout->GetLineViews();
			TestTrue(TEXT("Sample wraps into multiple lines"), Lines.Num() > 1);
			for (int32 I = 0; I < Lines.Num(); ++I)
			{
				const auto& Line = Lines[I];
				float RightEdge = 0.f;
				for (const auto& Block : Line.Blocks)
				{
					auto Range = Block->GetTextRange();
					const auto& ModelText = *Layout->GetLineModels()[Line.ModelIndex].Text;
					while (Range.EndIndex > Range.BeginIndex && FChar::IsWhitespace(ModelText[Range.EndIndex - 1])) --Range.EndIndex;
					if (Range.EndIndex > Range.BeginIndex)
					{
						RightEdge = FMath::Max(RightEdge, static_cast<float>(Block->GetLocationOffset().X
							+ Block->GetRun()->Measure(Range.BeginIndex, Range.EndIndex, 1.f, Block->GetTextContext()).X));
					}
				}
				if (I + 1 < Lines.Num()) TestTrue(TEXT("Wrapped line reaches the right edge without scaling glyphs"), FMath::IsNearlyEqual(RightEdge, Width, 0.5f));
				else TestTrue(TEXT("Paragraph ending keeps natural width"), RightEdge < Width);
			}
		}
	}
	return true;
}
#endif
