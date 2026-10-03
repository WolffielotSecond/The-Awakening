#pragma once
#include "CoreMinimal.h"
#include "Core/TAInputRouter.h"
#include "UObject/Interface.h"
#include "TAPlayerInputReceiver.generated.h"

/** Optional input endpoint on the request owner. No registration or receiver state in Router. */
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UTAPlayerInputReceiver : public UInterface
{
	GENERATED_BODY()
};

class THE_AWAKENING_API ITAPlayerInputReceiver
{
	GENERATED_BODY()
public:
	virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const = 0;
	virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const = 0;
	virtual void ExecutePlayerInput(FKey Key, ETAInputCapability Capability) = 0;
};
