#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TANearbyPickupComponent.generated.h"

class ATAWorldItem;
class UInputAction;
class UTAPickupPanelWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNearbyPickupChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNearbyPickupResult, int32, AddedCount, FText, Message);

/** Gameplay owns discovery, selection and transactions; WBP owns layout. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THE_AWAKENING_API UTANearbyPickupComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UTANearbyPickupComponent();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    void Configure(float InRadius, UInputAction* InInteract, UInputAction* InPrevious, UInputAction* InNext);

    UFUNCTION(BlueprintCallable, Category="Pickup")
    void RefreshNearbyItems();
    UFUNCTION(BlueprintCallable, Category="Pickup")
    void SelectPrevious();
    UFUNCTION(BlueprintCallable, Category="Pickup")
    void SelectNext();
    /** True means this press belongs to the list, including a failed/stale pickup. */
    bool TryPickupSelected();

    UFUNCTION(BlueprintPure, Category="Pickup")
    TArray<ATAWorldItem*> GetNearbyItems() const;
    UFUNCTION(BlueprintPure, Category="Pickup")
    ATAWorldItem* GetSelectedItem() const;
    UFUNCTION(BlueprintPure, Category="Pickup")
    int32 GetSelectedIndex() const;
    UFUNCTION(BlueprintPure, Category="Pickup")
    bool IsPickupAvailable() const;
    bool HasItems() const { return Items.Num() > 0; }
    bool HasPanel() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup", meta=(ClampMin="1"))
    float PickupRadius = 200.f;
    /** Optional line of sight to the item's origin, disabled to preserve existing pickup rules. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup")
    bool bRequireLineOfSight = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pickup|UI")
    TSubclassOf<UTAPickupPanelWidget> PickupPanelClass;
    UPROPERTY(BlueprintReadOnly, Category="Pickup|Input")
    TObjectPtr<UInputAction> InteractAction;
    UPROPERTY(BlueprintReadOnly, Category="Pickup|Input")
    TObjectPtr<UInputAction> PreviousAction;
    UPROPERTY(BlueprintReadOnly, Category="Pickup|Input")
    TObjectPtr<UInputAction> NextAction;
    UPROPERTY(BlueprintAssignable, Category="Pickup")
    FOnNearbyPickupChanged OnNearbyItemsChanged;
    UPROPERTY(BlueprintAssignable, Category="Pickup")
    FOnNearbyPickupResult OnPickupResult;

private:
    bool IsEligible(ATAWorldItem* Item) const;
    void MoveSelection(int32 Delta);
    void UpdatePanel();
    // Weak identity survives removal of earlier rows without retaining destroyed world actors.
    TArray<TWeakObjectPtr<ATAWorldItem>> Items;
    TWeakObjectPtr<ATAWorldItem> SelectedItem;
    TArray<int32> ObservedCounts;
    TArray<TWeakObjectPtr<class UTAItemDefinition>> ObservedDefinitions;
    UPROPERTY(Transient)
    TObjectPtr<UTAPickupPanelWidget> Panel;
    bool bPickingUp = false;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FTANearbyPickupTest;
#endif
};
