#pragma once

#include "CoreMinimal.h"

namespace TAPocketConnectionLayout
{
	/** Rects are in the same UI space. Image UV uses bottom-left origin; Slate uses top-left. */
	inline TArray<FVector2D> Calculate(FVector2D ImageMin, FVector2D ImageMax,
		FVector2D GridMin, FVector2D GridMax, FVector2D PocketUV, bool bGridOnLeft)
	{
		const double MiddleY = (GridMin.Y + GridMax.Y) * .5;
		const FVector2D End(bGridOnLeft ? GridMax.X : GridMin.X, MiddleY);
		const FVector2D Bend(bGridOnLeft ? ImageMin.X : ImageMax.X, MiddleY);
		const FVector2D UV(FMath::Clamp(PocketUV.X, 0., 1.), FMath::Clamp(PocketUV.Y, 0., 1.));
		const FVector2D Pocket = ImageMin + (ImageMax - ImageMin) * FVector2D(UV.X, 1. - UV.Y);
		return { End, Bend, Pocket };
	}
}
