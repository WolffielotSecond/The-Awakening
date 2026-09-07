#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TAParkourMarker.generated.h"

class UBoxComponent;
class UChildActorComponent;
class UWidgetComponent;
class UTexture2D;
class USplineComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> PromptWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> LandingSpline;

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

	UWidgetComponent* GetPromptWidgetComponent() const { return PromptWidget; }

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void SetPromptVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Parkour")
	void RefreshPrompt(UTexture2D* KeyIcon, const FText& PromptText);

protected:
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void BindLandingTargetFromChild();
};