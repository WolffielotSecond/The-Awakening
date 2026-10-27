#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/TAInventoryComponent.h"
#include "TAClothingPanelWidget.generated.h"

class UCanvasPanel;
class UImage;
class USizeBox;
class UHorizontalBox;
class UVerticalBox;
class UPanelWidget;
class UTAInventorySlotWidget;
class UTAItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTAClothingSlotHovered, UTAInventorySlotWidget*, Slot, UTAItemDefinition*, ItemDef);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTAClothingSlotUnhovered, UTAInventorySlotWidget*, Slot);

UCLASS()
class THE_AWAKENING_API UTAClothingPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 按衣服实例生成口袋格子。DisplayOverride：穿着的外套开启了 bOverrideInnerPocketDisplay 时传入，
	 * 用它的 InnerPocketDisplayOverrides 改写本面板（内衬）各口袋的摆放位置，只影响显示。
	 */
	UFUNCTION(BlueprintCallable, Category = "ClothingUI")
	void BuildFromClothing(const FTAClothingInstance& Instance, int32 FlatIndexStart = 0, const UTAClothingDefinition* DisplayOverride = nullptr);

	/** 生成的格子控件，按口袋顺序、口袋内格子顺序排列（与实例里的格子一一对应） */
	const TArray<TObjectPtr<UUserWidget>>& GetGeneratedSlots() const { return GeneratedSlots; }

	void SetInventoryComponent(UTAInventoryComponent* InInventory) { Inventory = InInventory; }

	UPROPERTY(BlueprintAssignable, Category = "ClothingUI|Hover")
	FOnTAClothingSlotHovered OnInventorySlotHovered;

	UPROPERTY(BlueprintAssignable, Category = "ClothingUI|Hover")
	FOnTAClothingSlotUnhovered OnInventorySlotUnhovered;

	UFUNCTION(BlueprintCallable, Category = "ClothingUI")
	void ClearPanel();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClothingUI")
	TSubclassOf<UUserWidget> SlotWidgetClass;

protected:
	virtual void NativeConstruct() override;

	void EnsureBindings();
	void SetClothingIcon(UTAClothingDefinition* Def);

	UPanelWidget* GetBoxForSide(ETAPocketSlotSide Side) const;

	void CreateSlotsForPocket(
		const FTAPocketRuntime& PocketRuntime,
		const FTAPocketDef& PocketDef,
		int32& InOutFlatIndex);

	UFUNCTION()
	void HandleSlotHovered(UTAInventorySlotWidget* HoveredSlot, UTAItemDefinition* ItemDef);

	UFUNCTION()
	void HandleSlotUnhovered(UTAInventorySlotWidget* HoveredSlot);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> SizeBox_Root;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> CanvasRoot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Clothing;

	/** Top / Bottom：HorizontalBox；Left / Right：VerticalBox */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> Box_Top;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> Box_Bottom;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> Box_Left;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> Box_Right;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClothingUI")
	float SlotSize = 64.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClothingUI")
	float SlotPadding = 6.f;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> GeneratedGroups;

	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> GeneratedSlots;

	UPROPERTY()
	TObjectPtr<UTAClothingDefinition> CurrentDef;

	UPROPERTY()
	TObjectPtr<UTAInventoryComponent> Inventory;

	UPROPERTY()
	TObjectPtr<UTAClothingDefinition> BuiltDef;

	UPROPERTY()
	int32 BuiltFlatStart = INDEX_NONE;

	/** 只用于判断布局是否需要重建 */
	TWeakObjectPtr<const UTAClothingDefinition> BuiltDisplayOverride;
};
