#pragma once
#include "Commandlets/Commandlet.h"
#include "TAParkourSetupCommandlet.generated.h"

/** Updates only default parkour mappings and creates missing placeholder icons. Never resaves maps. */
UCLASS()
class UTAParkourSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTAParkourSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
