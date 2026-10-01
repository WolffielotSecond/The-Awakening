#pragma once
#include "Commandlets/Commandlet.h"
#include "TAPuzzleLayoutCommandlet.generated.h"

/** Explicit editor-only migration. Refuses to replace an existing Designer layout. */
UCLASS()
class UTAPuzzleLayoutCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTAPuzzleLayoutCommandlet();
	virtual int32 Main(const FString& Params) override;
};
