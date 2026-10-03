#pragma once
#include "Commandlets/Commandlet.h"
#include "TAInputBlueprintMigrationCommandlet.generated.h"

/** One-time, explicit migration of the existing B-key debug call; no runtime input responsibilities. */
UCLASS()
class UTAInputBlueprintMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTAInputBlueprintMigrationCommandlet();
	virtual int32 Main(const FString& Params) override;
};
