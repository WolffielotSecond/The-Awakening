#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/TAInteractable.h"
#include "TAWorldItem.generated.h"

class UTAItemDefinition;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UTexture2D;
class UTAInventoryComponent;
class UTAScannableComponent;

UCLASS()
class THE_AWAKENING_API ATAWorldItem : public AActor, public ITAInteractable
{
	GENERATED_BODY()

public:
	ATAWorldItem();

	virtual void BeginPlay() override;

	// ITAInteractable
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractText_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetupItem(UTAItemDefinition* InDef, int32 InCount);

	UFUNCTION(BlueprintCallable, Category = "Item")
	UTAItemDefinition* GetItemDef() const { return ItemDef; }

	UFUNCTION(BlueprintCallable, Category = "Item")
	int32 GetCount() const { return Count; }

	UFUNCTION(BlueprintCallable, Category = "Item")
	bool IsCurrencyPickup() const;

	FString GetPromptTextId() const;

	UWidgetComponent* GetPromptWidgetComponent() const { return PromptWidget; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetPromptVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshPrompt(UTexture2D* KeyIcon, const FText& PromptText);

protected:
	bool TryPickupAsCurrency(AActor* Interactor);
	bool TryPickupAsInventory(AActor* Interactor, UTAInventoryComponent* Inventory);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> PromptWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAScannableComponent> ScannableComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTAItemDefinition> ItemDef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FString PromptTextId = TEXT("Interact_PickUp");
};
