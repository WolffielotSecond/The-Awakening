#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Camera/TARoomCameraProfile.h"
#include "TARoomViewComponent.generated.h"

class ATARoomRegion;
class ATARoomPortal;
class USpringArmComponent;
class UCameraComponent;
class UMaterialInterface;

/**
 * 本地玩家的空间视图协调：根据角色脚下位置解析当前区域与门口，决定哪些房间黑场，混合镜头参数，
 * 并作为唯一写入 Camera Boom 的 TargetArmLength / SocketOffset / TargetOffset 的地方。
 * 在 Boom 之前 Tick，使用世界时间（扫描冻结不卡住，暂停时不推进）。
 */
UCLASS(ClassGroup = Camera, meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTARoomViewComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTARoomViewComponent();

	/** 绑定要驱动的 Boom 与相机，并设置没有区域时使用的参数（来自角色蓝图）。 */
	void Initialize(USpringArmComponent* InBoom, UCameraComponent* InCamera, const FTARoomCameraSettings& InFallback);

	/**
	 * 已应用灵敏度 / 反转的观察输入（cm）。沿用原 DoLook 的语义：水平量写世界 Y，垂直量写世界 Z（向上为正）。
	 * 左右限位按相机实际朝向判断 +Y 在屏幕哪一侧，因此 LookLeft / LookRight 始终对应屏幕方向。
	 */
	void AddLookInput(float WorldYCm, float WorldZCm);

	/** 复活 / 传送后调用：清零观察偏移，立即按新位置解析区域，镜头参数直接到位不插值。 */
	void SnapToCurrentLocation();

	/** 当前所在区域 ID，没有区域时为 None。 */
	UFUNCTION(BlueprintPure, Category = "Camera|Room View")
	FName GetActiveRoomId() const { return ActiveRoomId; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 黑场后处理：在 Stencil 3 且没有其他物体覆盖的像素画暗色轮廓。初始化时挂到相机的后处理上。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Room View")
	TSoftObjectPtr<UMaterialInterface> BlackoutPostProcessMaterial;

	/** PIE 时在细节面板查看，方便调参。 */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Camera|Room View")
	FName ActiveRoomId;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Camera|Room View")
	FTARoomCameraSettings CurrentSettings;

	/** 当前观察偏移（cm）：X 为屏幕左右，Y 为屏幕上下。 */
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Camera|Room View")
	FVector2D LookOffset = FVector2D::ZeroVector;

private:
	void ResolveRegion(bool bSnap);
	void UpdateRoomVisibility();
	FVector GetProbeLocation() const;
	void ClampLookOffsetToCurrentLimits();
	float GetWorldYScreenSign() const;
	void ApplyToBoom();

	TWeakObjectPtr<USpringArmComponent> Boom;
	TWeakObjectPtr<UCameraComponent> Camera;
	TWeakObjectPtr<ATARoomRegion> ActiveRegion;
	TArray<TWeakObjectPtr<ATARoomPortal>> ActivePortals;

	FTARoomCameraSettings FallbackSettings;
	FTARoomCameraSettings BlendFrom;
	FTARoomCameraSettings BlendTo;
	float BlendElapsed = 0.f;
	float BlendDuration = 0.f;
	bool bInitialized = false;
	bool bSnapOnNextTick = false;
};
