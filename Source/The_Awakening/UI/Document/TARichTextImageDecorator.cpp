#include "UI/Document/TARichTextImageDecorator.h"
#include "UI/Document/TADocumentReaderWidget.h"
#include "Document/TADocumentDefinition.h"
#include "Components/RichTextBlock.h"
#include "Engine/Texture2D.h"
#include "Framework/Text/ITextDecorator.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogTARichTextImage, Log, All);

namespace
{
	class FTARichImageDecorator : public FRichTextDecorator
	{
	public:
		FTARichImageDecorator(URichTextBlock* InOwner, UTARichTextImageDecorator* InDecorator)
			: FRichTextDecorator(InOwner), Decorator(InDecorator)
		{
		}

		virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override
		{
			return RunParseResult.Name == TEXT("img") && RunParseResult.MetaData.Contains(TEXT("id"));
		}

	protected:
		virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FTextRunInfo& RunInfo, const FTextBlockStyle& TextStyle) const override
		{
			const FString Id = RunInfo.MetaData.FindRef(TEXT("id"));
			UTARichTextImageDecorator* Source = Decorator.Get();
			const FSlateBrush* Brush = Source ? Source->FindImageBrush(Owner, FName(*Id)) : nullptr;
			if (!Brush)
			{
				return SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("[image: %s]"), *Id))).TextStyle(&TextStyle);
			}
			return SNew(SBox)
				.WidthOverride(Brush->ImageSize.X)
				.HeightOverride(Brush->ImageSize.Y)
				[
					SNew(SImage).Image(Brush)
				];
		}

	private:
		TWeakObjectPtr<UTARichTextImageDecorator> Decorator;
	};
}

TSharedPtr<ITextDecorator> UTARichTextImageDecorator::CreateDecorator(URichTextBlock* InOwner)
{
	return MakeShared<FTARichImageDecorator>(InOwner, this);
}

const FSlateBrush* UTARichTextImageDecorator::FindImageBrush(URichTextBlock* Owner, FName ImageId)
{
	const UTADocumentReaderWidget* Reader = Owner ? Owner->GetTypedOuter<UTADocumentReaderWidget>() : nullptr;
	const UTADocumentDefinition* Document = Reader ? Reader->GetDocument() : nullptr;
	const TSoftObjectPtr<UTexture2D>* Entry = Document ? Document->InlineImages.Find(ImageId) : nullptr;
	UTexture2D* Texture = Entry ? Entry->LoadSynchronous() : nullptr;
	if (!Texture)
	{
		UE_LOG(LogTARichTextImage, Warning, TEXT("Image '%s' not found in document '%s' InlineImages."),
			*ImageId.ToString(), Document ? *Document->DocumentId.ToString() : TEXT("(none)"));
		return nullptr;
	}

	if (const TSharedPtr<FSlateBrush>* Existing = Brushes.Find(Texture))
	{
		return Existing->Get();
	}
	LoadedTextures.AddUnique(Texture);
	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	FVector2D Size(Texture->GetSizeX(), Texture->GetSizeY());
	if (Size.X > MaxImageWidth && Size.X > 0.f)
	{
		Size *= MaxImageWidth / Size.X;
	}
	Brush->ImageSize = Size;
	Brushes.Add(Texture, Brush);
	return Brush.Get();
}
