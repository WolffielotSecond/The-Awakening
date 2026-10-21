#pragma once
#include "CoreMinimal.h"
#include "Core/TAInputRouter.h"
#include "UObject/Interface.h"
#include "TAPlayerInputReceiver.generated.h"

class UInputAction;
struct FInputActionInstance;
enum class ETriggerEvent : uint8;

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
	virtual bool UsesEnhancedUIActions() const { return false; }
	virtual void ExecutePlayerAction(const UInputAction* Action, ETriggerEvent Event, const FInputActionInstance& Instance) {}
	/** Called for the shared UI Back action while this receiver owns input. */
	virtual bool HandleMenuBackRequested() { return false; }
 virtual bool UsesSharedBackAction() const { return true; }
 virtual bool IsCapturingPlayerInput() const { return false; }
 virtual bool CapturePlayerInput(FKey Key, bool bRepeat) { return false; }
 virtual bool ShouldShowPlayerCursor() const { return true; }
 /** Consumer policy; physical stick observation is unaffected. */
 virtual bool AllowsVirtualCursor() const { return true; }
 /** Optional selected control for returning from directional navigation to a real pointer. */
 virtual UWidget* GetPlayerPointerTarget() const { return nullptr; }
 virtual void NotifyPlayerPointerMoved() {}
};
