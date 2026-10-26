#pragma once
#include "Commandlets/Commandlet.h"
#include "TALocationRevealSetupCommandlet.generated.h"

/** Idempotent import into the existing global composite, plus a tunable widget BP. */
UCLASS()
class UTALocationRevealSetupCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 virtual int32 Main(const FString& Params) override;
};
