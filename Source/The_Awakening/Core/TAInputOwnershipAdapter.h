#pragma once
#include "CoreMinimal.h"
class APlayerController;
/** Sole platform/Slate/PIE ownership boundary. No gameplay behavior. */
namespace TAInputOwnershipAdapter
{
	// Transition is an unresolved focus gap in the active game window, not permission.
	enum class EState : uint8 { Owned, Transition, Lost };
	EState GetState(const APlayerController* Player, const FVector2D* PointerPosition = nullptr);
	bool CanApplyPresentation(const APlayerController* Player);
	bool OwnsInput(const APlayerController* Player, const FVector2D* PointerPosition = nullptr);
}
