#pragma once
#include "CoreMinimal.h"
class UWidgetTree;
struct FTAPuzzleAppearance;
/** Shared by native fallback and the explicit WBP Designer migration. */
namespace TAPuzzleScopeLayout
{
	THE_AWAKENING_API void AddLocalizedPrompts(UWidgetTree* Tree);
	THE_AWAKENING_API void Build(UWidgetTree* Tree, const FTAPuzzleAppearance& Appearance);
}
