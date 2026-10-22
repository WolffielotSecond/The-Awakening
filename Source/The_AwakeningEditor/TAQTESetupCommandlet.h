#pragma once
#include "Commandlets/Commandlet.h"
#include "TAQTESetupCommandlet.generated.h"
UCLASS()
class UTAQTESetupCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UTAQTESetupCommandlet();
 virtual int32 Main(const FString& Params) override;
};
