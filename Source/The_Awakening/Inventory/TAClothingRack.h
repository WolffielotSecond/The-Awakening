#pragma once

#include "CoreMinimal.h"
#include "Interaction/TAInteractableActor.h"
#include "Inventory/TAInventoryComponent.h"
#include "Scan/TAScanTypes.h"
#include "TAClothingRack.generated.h"

class UStaticMesh;
class UTAClothingDefinition;
class UTAScannableComponent;
class UTAClothingSwapWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTAClothingRackChanged);

/**
 * 衣架：可以挂一件外套（连同口袋里的物品）。交互打开换装界面，在界面里穿上 / 换下外套、
 * 在衣架外套的口袋和身上的口袋之间拿放物品。
 * 有外套时显示 CoatMesh，空衣架显示 EmptyMesh（都不填时保持蓝图里 MeshComponent 的模型）。
 */
UCLASS()
class THE_AWAKENING_API ATAClothingRack : public ATAInteractableActor
{
	GENERATED_BODY()

public:
	ATAClothingRack();

	virtual void BeginPlay() override;

	// ITAInteractable
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	UFUNCTION(BlueprintPure, Category = "ClothingRack")
	bool HasClothing() const { return StoredClothing.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "ClothingRack")
	const FTAClothingInstance& GetStoredClothing() const { return StoredClothing; }

	/** 换装界面直接改衣架上的外套（换衣服、拿放口袋物品），改完调用 NotifyClothingChanged */
	FTAClothingInstance& GetStoredClothingMutable() { return StoredClothing; }

	/** 刷新模型和扫描信息，并广播 OnRackChanged */
	UFUNCTION(BlueprintCallable, Category = "ClothingRack")
	void NotifyClothingChanged();

	/** 回滚记录期间写入一条撤销：恢复衣架当前挂着的外套。改动前调用 */
	void RecordUndo();

	UPROPERTY(BlueprintAssignable, Category = "ClothingRack")
	FOnTAClothingRackChanged OnRackChanged;

protected:
	void ApplyMesh();
	void ApplyScanInfo();

	/** 关卡里摆放时衣架上挂的外套（Layer 必须是 Outer），可为空 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClothingRack")
	TObjectPtr<UTAClothingDefinition> InitialClothing;

	/** 初始外套口袋里的物品，按口袋顺序、格子顺序依次放入；超出格子数的忽略 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClothingRack")
	TArray<FTAInventorySlot> InitialPocketItems;

	/** 挂着外套时的模型 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClothingRack|Mesh")
	TObjectPtr<UStaticMesh> CoatMesh;

	/** 空衣架的模型 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClothingRack|Mesh")
	TObjectPtr<UStaticMesh> EmptyMesh;

	/** 换装界面；为空时用 /Game/UI/Inventory/WBP_ClothingSwap，再没有时用 C++ 临时布局 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClothingRack|UI")
	TSubclassOf<UTAClothingSwapWidget> SwapWidgetClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAScannableComponent> ScannableComponent;

	/**
	 * 衣架当前挂着的外套。
	 * TODO(存档)：衣架状态是否 / 如何存档尚未确定，暂不保存；读档或重新进入关卡后恢复为 InitialClothing。
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ClothingRack")
	FTAClothingInstance StoredClothing;

	/** 编辑器里配置的扫描信息；空衣架时用它 */
	FTAScanTargetInfo ConfiguredScanInfo;
};
