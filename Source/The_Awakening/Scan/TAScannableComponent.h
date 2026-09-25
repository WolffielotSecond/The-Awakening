#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Scan/TAScanTypes.h"
#include "TAScannableComponent.generated.h"

class UPrimitiveComponent;

UCLASS(ClassGroup=(Scan), meta=(BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAScannableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTAScannableComponent();

	UFUNCTION(BlueprintPure, Category = "Scan")
	const FTAScanTargetInfo& GetScanInfo() const { return ScanInfo; }

	UFUNCTION(BlueprintCallable, Category = "Scan")
	void SetScanInfo(const FTAScanTargetInfo& NewInfo) { ScanInfo = NewInfo; }

	UFUNCTION(BlueprintPure, Category = "Scan")
	bool CanBeScanned() const { return bCanBeScanned; }

	UFUNCTION(BlueprintCallable, Category = "Scan")
	void SetScanHighlightEnabled(bool bEnabled);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Info")
	FTAScanTargetInfo ScanInfo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Rules")
	bool bCanBeScanned = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Rules", meta=(ClampMin="0.0"))
	float HighlightDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Rules", meta=(ClampMin="0.0"))
	float InformationDistance = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Rules")
	bool bAllowHighlightThroughWalls = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Rules")
	bool bAllowInformationThroughWalls = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Highlight", meta=(ClampMin="0", ClampMax="255"))
	int32 CustomDepthStencilValue = 2;

	/** Item=2, Terminal=3, Enemy=4. Disable to use CustomDepthStencilValue directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Highlight")
	bool bUseTargetTypeStencil = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan|Highlight")
	TArray<TObjectPtr<UPrimitiveComponent>> HighlightComponents;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FOriginalRenderState
	{
		bool bRenderCustomDepth = false;
		int32 StencilValue = 0;
	};

	TMap<TWeakObjectPtr<UPrimitiveComponent>, FOriginalRenderState> OriginalRenderStates;
};
