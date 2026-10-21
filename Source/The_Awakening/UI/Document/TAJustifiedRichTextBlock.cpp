#include "UI/Document/TAJustifiedRichTextBlock.h"
#include "UI/Document/TAJustifiedTextLayout.h"
#include "Framework/Text/ILayoutBlock.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Styling/SlateStyle.h"
#include "Widgets/Text/SRichTextBlock.h"

void FTAJustifiedTextLayout::EndLayout()
{
	const float AvailableWidth = (WrappingWidth - Margin.Left - Margin.Right) * Scale;
	for (int32 LineIndex = 0; LineIndex + 1 < LineViews.Num() && AvailableWidth > 0.f; ++LineIndex)
	{
		FLineView& Line = LineViews[LineIndex];
		// Never stretch the last line of a paragraph, an explicit newline, or a mixed-direction line.
		if (Line.ModelIndex != LineViews[LineIndex + 1].ModelIndex || Line.TextBaseDirection != TextBiDi::ETextDirection::LeftToRight) continue;
		bool bPlainTextLine = true;
		for (const auto& Block : Line.Blocks)
		{
			bPlainTextLine &= EnumHasAnyFlags(Block->GetRun()->GetRunAttributes(), ERunAttributes::SupportsText)
				&& Block->GetTextContext().TextDirection == TextBiDi::ETextDirection::LeftToRight;
		}
		// Widget/image decorators stay intact with their original metrics and layout.
		if (!bPlainTextLine) continue;
		const FLineModel& Model = LineModels[Line.ModelIndex];
		int32 VisibleEnd = Line.Range.EndIndex;
		while (VisibleEnd > Line.Range.BeginIndex && FChar::IsWhitespace((*Model.Text)[VisibleEnd - 1])) --VisibleEnd;
		TArray<int32> Breaks;
		for (const FBreakCandidate& Candidate : Model.BreakCandidates)
		{
			const int32 End = Candidate.ActualRange.EndIndex;
			if (End > Line.Range.BeginIndex && End < VisibleEnd) Breaks.AddUnique(End);
		}
		const float ExtraWidth = AvailableWidth - Line.JustificationWidth;
		if (Breaks.IsEmpty() || ExtraWidth <= KINDA_SMALL_NUMBER) continue;
		const float Gap = ExtraWidth / Breaks.Num();
		TArray<TSharedRef<ILayoutBlock>> ExpandedBlocks;
		for (const auto& Block : Line.Blocks)
		{
			const FTextRange Range = Block->GetTextRange();
			const auto Run = Block->GetRun();
			const auto Context = Block->GetTextContext();
			int32 Start = Range.BeginIndex;
			while (Start < Range.EndIndex)
			{
				int32 End = Range.EndIndex;
				int32 GapsBefore = 0;
				for (int32 Break : Breaks)
				{
					if (Break <= Start) ++GapsBefore;
					else End = FMath::Min(End, Break);
				}
				// Prefix metrics preserve the original font shaping across rich-text style boundaries.
				const float Prefix = Start == Range.BeginIndex ? 0.f : Run->Measure(Range.BeginIndex, Start, Scale, Context).X;
				const float EndPosition = Run->Measure(Range.BeginIndex, End, Scale, Context).X;
				const auto Fragment = Run->CreateBlock(Start, End, FVector2D(EndPosition - Prefix, Block->GetSize().Y), Context, Block->GetRenderer());
				Fragment->SetLocationOffset(Block->GetLocationOffset() + FVector2D(Prefix + GapsBefore * Gap, 0.f));
				ExpandedBlocks.Add(Fragment);
				Start = End;
			}
		}
		Line.Blocks = MoveTemp(ExpandedBlocks);
		Line.Size.X = AvailableWidth;
		Line.JustificationWidth = AvailableWidth;
		TextLayoutSize.DrawWidth = FMath::Max(TextLayoutSize.DrawWidth, AvailableWidth + Margin.GetTotalSpaceAlong<Orient_Horizontal>() * Scale);
		TextLayoutSize.WrappedWidth = FMath::Max(TextLayoutSize.WrappedWidth, TextLayoutSize.DrawWidth);
	}
	FSlateTextLayout::EndLayout();
}

TSharedRef<SWidget> UTAJustifiedRichTextBlock::RebuildWidget()
{
	UpdateStyleData();
	TArray<TSharedRef<ITextDecorator>> Decorators;
	CreateDecorators(Decorators);
	const auto Marshaller = FRichTextLayoutMarshaller::Create(CreateMarkupParser(), CreateMarkupWriter(), Decorators, StyleInstance.Get());
	MyRichTextBlock = SNew(SRichTextBlock)
		.TextStyle(&GetCurrentDefaultTextStyle())
		.Marshaller(Marshaller)
		.CreateSlateTextLayout(FCreateSlateTextLayout::CreateLambda([](SWidget* Owner, const FTextBlockStyle& Style) -> TSharedRef<FSlateTextLayout>
		{
			return MakeShared<FTAJustifiedTextLayout>(Owner, Style);
		}));
	return MyRichTextBlock.ToSharedRef();
}
