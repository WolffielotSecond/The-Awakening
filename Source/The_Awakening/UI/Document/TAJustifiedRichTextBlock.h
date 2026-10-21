#pragma once

#include "Components/RichTextBlock.h"
#include "TAJustifiedRichTextBlock.generated.h"

/** Reader body with full justification, retaining the standard rich-text parser and decorators. */
UCLASS()
class THE_AWAKENING_API UTAJustifiedRichTextBlock : public URichTextBlock
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
