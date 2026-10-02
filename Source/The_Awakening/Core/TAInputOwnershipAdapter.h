#pragma once
#include "CoreMinimal.h"
class APlayerController;
/** Sole platform/Slate/PIE ownership boundary. No gameplay behavior. */
namespace TAInputOwnershipAdapter
{
	bool OwnsInput(const APlayerController* Player, const FVector2D* PointerPosition = nullptr);
}
