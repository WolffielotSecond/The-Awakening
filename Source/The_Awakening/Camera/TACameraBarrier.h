#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TACameraBarrier.generated.h"

class UBoxComponent;
class UArrowComponent;
class APlayerController;
struct FMinimalViewInfo;

/** 透视视锥的四个侧面，由实际视图的位置、朝向、FOV 和画面宽高比得到。 */
struct THE_AWAKENING_API FTACameraFrustum
{
	FVector Origin = FVector::ZeroVector;
	/** 四个侧面的单位法线，指向视锥外侧：点 Q 在视锥内当且仅当 (Q - Origin) · Normal <= 0 对四个面都成立。 */
	FVector SideNormals[4];
	bool bValid = false;

	/**
	 * 按实际视图构建。PC 用来读视口尺寸和 LocalPlayer 的宽高比轴约束；为空时使用 View.AspectRatio、保持水平 FOV。
	 * 正交投影返回无效。
	 */
	static FTACameraFrustum FromView(const FMinimalViewInfo& View, const APlayerController* PC);

	/** 直接给出位置、朝向与水平 / 垂直半角的正切值（测试与内部使用）。 */
	static FTACameraFrustum FromAngles(const FVector& Location, const FRotator& Rotation, float TanHalfHorizontal, float TanHalfVertical);
};

/** 一面挡墙在本帧的求解输入：挡住的矩形（已加上安全距离）、允许方向和当前权重。 */
struct THE_AWAKENING_API FTACameraBarrierInput
{
	/** 单位向量，指向允许一侧（被保护的房间）。镜头只沿这个方向被推回。 */
	FVector Normal = FVector::ForwardVector;
	/** 画面不能看到的矩形四个角（世界坐标）。 */
	FVector Corners[4];
	/** 0–1，过门时平滑变化。 */
	float Weight = 1.f;
};

/**
 * 镜头挡墙：放在一个区域最左 / 最右的边缘墙上，玩家在所属区域内时，画面看不到墙后（下一个场景）的内容。
 * 只沿箭头方向推回最终相机位置；角色可以继续走向门口并暂时离开画面。
 * 关卡里请放 BP_CameraBarrier，不要直接放本 C++ 类。摆放方法见 Docs/CameraBarrierGuide.md。
 */
UCLASS(meta = (DisplayName = "Camera Barrier"))
class THE_AWAKENING_API ATACameraBarrier : public AActor
{
	GENERATED_BODY()

public:
	ATACameraBarrier();

	/**
	 * 玩家位于这些区域（RoomId）时本挡墙生效，通常填墙所在房间自己的 RoomId。
	 * 为空时一直生效，包括玩家不在任何区域时。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Barrier")
	TArray<FName> ActiveInRoomIds;

	/** 画面边缘离挡住的那一面至少留多少距离（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Barrier", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SafetyMarginCm = 10.f;

	/** 关闭后本挡墙不参与计算（调试用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Barrier")
	bool bBarrierEnabled = true;

	/** 玩家位于 RoomId 时本挡墙是否应生效（不含过渡权重）。 */
	UFUNCTION(BlueprintPure, Category = "Camera Barrier")
	bool IsActiveForRoom(FName RoomId) const;

	/** 按当前摆放生成求解输入：矩形取盒子背向箭头的那一面（-X 面），再向箭头方向外扩 SafetyMarginCm。 */
	FTACameraBarrierInput MakeInput(float Weight) const;

	/**
	 * 单面挡墙需要的推回距离（cm，>= 0，沿 Input.Normal）。
	 * 矩形完全在视锥某个侧面之外即视为看不到；无法通过沿法线移动解决时返回 0。
	 */
	static float ComputeRequiredPush(const FTACameraFrustum& Frustum, const FTACameraBarrierInput& Input);

	/**
	 * 多面挡墙合成的相机位移（世界坐标）。同一条轴上方向相反的两面都需要推回时（过门交接或房间比画面还窄），
	 * 按权重取加权平均：过门时等于两种构图之间的平滑过渡，两面都满权重时取两者中点，不来回抖动。
	 */
	static FVector ComputeDisplacement(const FTACameraFrustum& Frustum, TConstArrayView<FTACameraBarrierInput> Inputs);

	UBoxComponent* GetBarrierBox() const { return BarrierBox; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** X 轴（箭头）指向被保护的房间；Y / Z 范围要盖住房间的整个深度和高度。不参与碰撞。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Barrier")
	TObjectPtr<UBoxComponent> BarrierBox;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Camera Barrier")
	TObjectPtr<UArrowComponent> DirectionArrow;
#endif
};
