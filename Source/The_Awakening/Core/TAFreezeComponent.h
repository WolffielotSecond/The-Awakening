#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TAFreezeComponent.generated.h"

class UTAFreezeSubsystem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAOnFreezeChanged, bool, bFrozen);

/** Add to an actor to opt into scan freeze. Does not pause world time or physics. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAFreezeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTAFreezeComponent();

	UFUNCTION(BlueprintPure, Category="Freeze")
	bool IsFrozen() const { return bFrozen; }
	UFUNCTION(BlueprintPure, Category="Freeze")
	float GetFreezeStrength() const { return FreezeStrength; }

	/** For external interactions, which still execute even when ticks are disabled. */
	UFUNCTION(BlueprintPure, Category="Freeze")
	static bool IsActorFrozen(const AActor* Actor);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze")
	bool bFreezeActorTick = true;

	/** Components with this Component Tag keep ticking. Set before freezing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Freeze")
	FName ExemptComponentTag = TEXT("FreezeExempt");

	/** Extension point for custom timers and other non-tick gameplay. */
	UPROPERTY(BlueprintAssignable, Category="Freeze")
	FTAOnFreezeChanged OnFreezeChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UTAFreezeSubsystem;
	void SetFreezeStrength(float Strength);
	float FreezeStrength = 0.f;
	float SavedTimeDilation = 1.f;
	void SetFrozen(bool bInFrozen);
	bool bFrozen = false;
	bool bSavedActorTickEnabled = false;
	bool bDidFreezeActorTick = false;
	TMap<TWeakObjectPtr<UActorComponent>, bool> SavedComponentTicks;
};
