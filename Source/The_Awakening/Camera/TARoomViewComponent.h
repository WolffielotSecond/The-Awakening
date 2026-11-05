#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Camera/TARoomCameraProfile.h"
#include "TARoomViewComponent.generated.h"

class ATARoomRegion;
class ATACameraBarrier;
struct FMinimalViewInfo;
class USpringArmComponent;
class UCameraComponent;
class UMaterialInterface;

/**
 * 本地玩家的空间视图协调：根据角色脚下位置解析当前区域，执行该区域指定的黑场规则，混合镜头参数，
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

	/**
	 * 最终视图约束：镜头挡墙把实际相机位置挡在玩家所在的一侧，玩家换边时平滑穿过。
	 * 由 UTAAutoFocusCameraComponent::GetCameraView 在计算对焦之前调用，Lag、区域混合和附加偏移都已包含在 View 里。
	 */
	void ApplyCameraBarriers(FMinimalViewInfo& View);

	/** 最近一次最终视图约束施加的位移（世界坐标，cm）。 */
	UFUNCTION(BlueprintPure, Category = "Camera|Room View")
	FVector GetLastBarrierCorrection() const { return LastBarrierCorrection; }

	/** 玩家相对挡墙的一侧（+1 箭头方向，-1 背面），未登记时为 0。 */
	int32 GetBarrierSide(const ATACameraBarrier* Barrier) const;

	/** 换边过渡进度（0–1，已平滑），1 表示已经完全挡在当前一侧。 */
	float GetBarrierCrossAlpha(const ATACameraBarrier* Barrier) const;

	/** 当前所在区域 ID，没有区域时为 None。 */
	UFUNCTION(BlueprintPure, Category = "Camera|Room View")
	FName GetActiveRoomId() const { return ActiveRoomId; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 黑场后处理：在 Stencil 3 且没有其他物体覆盖的像素画暗色轮廓。初始化时挂到相机的后处理上。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera|Room View")
	TSoftObjectPtr<UMaterialInterface> BlackoutPostProcessMaterial;

	/** 关闭后不应用镜头挡墙（以后的过场镜头覆盖可临时关闭）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Room View")
	bool bCameraBarriersEnabled = true;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Camera|Room View")
	FVector LastBarrierCorrection = FVector::ZeroVector;

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
	void UpdateBarrierStates(float DeltaSeconds, bool bSnap);
	FVector ComputeBarrierDisplacement(const FVector& CameraLocation) const;
	/** 按不含 Lag 的目标相机位置计算挡墙推回量，把朝墙方向多余的观察偏移收回，避免往回推时出现死区。 */
	void AbsorbLookOffsetIntoBarriers();
	bool BuildUnlaggedCameraLocation(FVector& OutLocation) const;

	TWeakObjectPtr<USpringArmComponent> Boom;
	TWeakObjectPtr<UCameraComponent> Camera;
	TWeakObjectPtr<ATARoomRegion> ActiveRegion;

	/** 每面挡墙：玩家所在一侧，以及换边过渡的线性进度（0–1）。 */
	struct FBarrierState
	{
		int32 Side = 0;
		float Alpha = 1.f;
	};
	TMap<TWeakObjectPtr<ATACameraBarrier>, FBarrierState> BarrierStates;
	/** 不含 Lag 的目标相机位置需要的推回量，用于阻止继续朝墙推镜头。 */
	FVector TargetBarrierPush = FVector::ZeroVector;

	FTARoomCameraSettings FallbackSettings;
	FTARoomCameraSettings BlendFrom;
	FTARoomCameraSettings BlendTo;
	float BlendElapsed = 0.f;
	float BlendDuration = 0.f;
	bool bInitialized = false;
	bool bSnapOnNextTick = false;
};
