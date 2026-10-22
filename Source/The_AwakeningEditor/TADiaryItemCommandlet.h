#pragma once
#include "Commandlets/Commandlet.h"
#include "TADiaryItemCommandlet.generated.h"

/** -run=TADiaryItem -AllowCommandletRendering [-Verify] [-Render] */
UCLASS()
class UTADiaryItemCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTADiaryItemCommandlet();
	virtual int32 Main(const FString& Params) override;
};
