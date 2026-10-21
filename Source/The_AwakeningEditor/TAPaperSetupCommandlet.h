#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "TAPaperSetupCommandlet.generated.h"

/** -UnifyDefaults migrates legacy/default document visuals to DA_Document_67, backing up changed assets.
 *  -UnifyDefaults -Verify audits without saving. IDs and localized content are preserved. */
UCLASS()
class UTAPaperSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTAPaperSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
