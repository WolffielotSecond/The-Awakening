#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/TAPromptTypes.h"
#include "TAParkourMarker.generated.h"

class UBoxComponent;
class UChildActorComponent;
class UTAInteractionPromptAnchor;
class UTexture2D;
class USplineComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ETAParkourMarkerType : uint8
{
	JumpToPoint UMETA(DisplayName = "跨台跳(空格)"),
	DropDown    UMETA(DisplayName = "高台跳下(Ctrl)")
};

UCLASS()
class THE_AWAKENING_API ATAParkourMarker : public AActor
{
	GENERATED_BODY()

public:
	ATAParkourMarker();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentOverlappingActor;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UChildActorComponent> LandingTargetComponent;

	/** 跑酷提示锚点（统一提示系统），挂在随玩家位置移动的落点上，所以提示显示在落点处。类型随 MarkerType 自动设置。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAInteractionPromptAnchor> PromptWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> LandingSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour")
	ETAParkourMarkerType MarkerType = ETAParkourMarkerType::JumpToPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour")
	TObjectPtr<AActor> LandingTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour", meta = (ClampMin = "0"))
	float ArcHeight = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parkour", meta = (ClampMin = "0.05"))
	float JumpDuration = 0.5f;

	/** true: X为深度轴，false: Y为深度轴 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parkour")
	bool bUseXAsDepthAxis = true;

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	bool IsValidMarker() const;

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	FVector GetLandingLocation(const FVector& PlayerWorldLocation) const;

	UTAInteractionPromptAnchor* GetPromptAnchor() const { return PromptWidget; }
	/** 跨台跳 → 跨越提示，高台跳下 → 跳下提示。 */
	ETAPromptType GetPromptType() const
	{
		return MarkerType == ETAParkourMarkerType::DropDown ? ETAPromptType::ParkourDrop : ETAPromptType::ParkourJump;
	}

protected:
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
