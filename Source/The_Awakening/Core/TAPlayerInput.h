#pragma once

#include "CoreMinimal.h"
#include "EnhancedPlayerInput.h"
#include "TAPlayerInput.generated.h"

/** Exposes the player's resolved mappings for continuous movement / parkour intent. */
UCLASS()
class THE_AWAKENING_API UTAPlayerInput : public UEnhancedPlayerInput
{
	GENERATED_BODY()
public:
	const TArray<FEnhancedActionKeyMapping>& GetHeldActionMappings() const { return GetEnhancedActionMappings(); }
};
