#pragma once
#include "Commandlets/Commandlet.h"
#include "TAInteractionSamplesCommandlet.generated.h"
/** Creates isolated interaction samples without overwriting existing assets. */
UCLASS()
class UTAInteractionSamplesCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UTAInteractionSamplesCommandlet();
 virtual int32 Main(const FString& Params) override;
};
