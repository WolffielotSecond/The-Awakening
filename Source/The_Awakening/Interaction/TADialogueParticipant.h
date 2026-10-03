#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TADialogueParticipant.generated.h"

/** Eligibility for normal dialogue interaction; not a general actor state query. */
UINTERFACE(MinimalAPI, Blueprintable)
class UTADialogueParticipant : public UInterface
{
	GENERATED_BODY()
};

class THE_AWAKENING_API ITADialogueParticipant
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Dialogue")
	bool CanParticipateInDialogue() const;
};
