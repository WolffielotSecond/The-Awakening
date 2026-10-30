#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Fonts/SlateFontInfo.h"
#include "TAInventoryTabBar.generated.h"

class SInventoryTabBar;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAInventoryTabRequested, int32, Page);

/** Inventory header presentation only. Input mappings and page ownership stay in the panel. */
UCLASS()
class THE_AWAKENING_API UTAInventoryTabBar : public UWidget
{
	GENERATED_BODY()
public:
	UTAInventoryTabBar(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Tabs")
	FTAInventoryTabRequested OnTabRequested;

	UPROPERTY(EditAnywhere, Category = "Inventory|Tabs")
	FSlateFontInfo LabelFont;

	UPROPERTY(EditAnywhere, Category = "Inventory|Tabs", meta = (ClampMin = "0.01"))
	float TransitionDuration = 0.32f;

	UPROPERTY(EditAnywhere, Category = "Inventory|Tabs")
	FLinearColor DarkColor = FLinearColor(FColor(0x26, 0x34, 0x3A));

	UPROPERTY(EditAnywhere, Category = "Inventory|Tabs")
	FLinearColor LightColor = FLinearColor(FColor(0xB9, 0xD7, 0xD8));

	/** UI material inverts RGB and maps it to the header palette, preserving alpha. */
	UPROPERTY(EditAnywhere, Category = "Inventory|Tabs")
	TObjectPtr<UMaterialInterface> InvertIconMaterial;

	void SetPage(int32 Page, bool bAnimate = true);
	void SetLabels(const FText& Inventory, const FText& Skills, const FSlateFontInfo& Font);
	void SetInputIcons(UTexture2D* Previous, UTexture2D* Next);
	void SetStatusLabels(const FText& Cells, const FText& Health);
	const FText& GetCellsLabel() const { return CellsLabel; }
	const FText& GetHealthLabel() const { return HealthLabel; }
	int32 GetSelectedPage() const { return SelectedPage; }
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	void RequestPage(int32 Page);
	void UpdateIconBrush(UTexture2D* Texture, TObjectPtr<UMaterialInstanceDynamic>& Material, FSlateBrush& Brush);
	TSharedPtr<SInventoryTabBar> SlateTabBar;
	FText InventoryLabel;
	FText SkillsLabel;
	FText CellsLabel;
	FText HealthLabel;
	int32 SelectedPage = 0;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PreviousIconMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> NextIconMaterial;
	UPROPERTY(Transient) FSlateBrush PreviousIconBrush;
	UPROPERTY(Transient) FSlateBrush NextIconBrush;
};
