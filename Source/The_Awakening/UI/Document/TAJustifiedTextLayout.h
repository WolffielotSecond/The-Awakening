#pragma once

#include "Framework/Text/SlateTextLayout.h"

/** Expand legal line-break gaps on wrapped LTR lines; paragraph endings retain natural spacing. */
class FTAJustifiedTextLayout : public FSlateTextLayout
{
public:
	FTAJustifiedTextLayout(SWidget* Owner, const FTextBlockStyle& Style) : FSlateTextLayout(Owner, Style) {}
	virtual void EndLayout() override;

protected:
	// UE 5.8 doesn't export the base implementation (missing SLATE_API), so subclasses outside Slate must provide it.
	virtual void HandleAllFontFacesFinishLoading() override;
};
