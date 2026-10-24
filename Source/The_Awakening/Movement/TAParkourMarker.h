#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/TAPromptTypes.h"
#include "TAParkourMarker.generated.h"

class UBoxComponent;
class UChildActorComponent;
class UTAInteractionPromptAnchor;
class USplineComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ETAParkourMarkerType : uint8
{
	JumpToPoint UMETA(DisplayName = "Legacy Upper"),
	DropDown    UMETA(DisplayName = "Legacy Lower")
};

UCLASS()
class THE_AWAKENING_API ATAParkourMarker : public AActor
{
	GENERATED_BODY()

public:
	ATAParkourMarker();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif

protected:
	bool bLoadedLegacyLanding = false;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentOverlappingActor;

public:
	// Keep all original subobject/property names: Blueprint and level overrides bind to them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Landings")
	bool bEnableUpper = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Landings")
	bool bEnableLower = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> AdditionalLandingSpline;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UChildActorComponent> AdditionalLandingTargetComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAInteractionPromptAnchor> AdditionalPromptWidget;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Upper", meta = (ClampMin = "0"))
	float UpperArcHeight = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Upper", meta = (ClampMin = "0.05"))
	float UpperJumpDuration = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Lower", meta = (ClampMin = "0"))
	float LowerArcHeight = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour|Lower", meta = (ClampMin = "0.05"))
	float LowerJumpDuration = 0.5f;
	/** Original component data is retained in place; this read-only flag identifies its logical slot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour|Legacy")
	bool bLegacySplineIsLower = false;

	bool IsLandingEnabled(bool bLower) const { return bLower ? bEnableLower : bEnableUpper; }
	bool HasValidLanding(bool bLower) const;
	UFUNCTION(BlueprintPure, Category = "Parkour|Landings")
	USplineComponent* GetLandingSpline(bool bLower) const;
	UFUNCTION(BlueprintPure, Category = "Parkour|Landings")
	UChildActorComponent* GetLandingTargetComponent(bool bLower) const;
	UFUNCTION(BlueprintPure, Category = "Parkour|Landings")
	UTAInteractionPromptAnchor* GetLandingPrompt(bool bLower) const;
	UFUNCTION(BlueprintPure, Category = "Parkour|Landings")
	FVector GetLandingLocationForSlot(const FVector& PlayerWorldLocation, bool bLower) const;
	float GetArcHeight(bool bLower) const { return bLower ? LowerArcHeight : UpperArcHeight; }
	float GetJumpDuration(bool bLower) const { return bLower ? LowerJumpDuration : UpperJumpDuration; }
	void UpdateLandingTargets(const FVector& PlayerLocation);
	void InitializeLandingConfiguration();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UChildActorComponent> LandingTargetComponent;

	/** 跑酷提示锚点（统一提示系统），挂在随玩家位置移动的落点上，所以提示显示在落点处。类型由逻辑 Upper/Lower 槽位设置。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAInteractionPromptAnchor> PromptWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> LandingSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour|Legacy")
	ETAParkourMarkerType MarkerType = ETAParkourMarkerType::JumpToPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour")
	TObjectPtr<AActor> LandingTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour|Legacy", meta = (ClampMin = "0"))
	float ArcHeight = 120.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parkour|Legacy", meta = (ClampMin = "0.05"))
	float JumpDuration = 0.5f;

	/** true: X为深度轴，false: Y为深度轴 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	bool bUseXAsDepthAxis = true;

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool IsValidMarker() const;

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	FVector GetLandingLocation(const FVector& PlayerWorldLocation) const;


protected:
	// Serialized migration flag; retain its historical name for existing Blueprint/level data.
	UPROPERTY() bool bDirectionalLandingConfigured = true;
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void BindLandingTargetFromChild();
	/** 落点只是位置锚点，游戏中始终隐藏它的图标 / 箭头。提示的显隐只由提示系统控制。 */
	void HideLandingAnchor();
	void SyncPromptType();
};
