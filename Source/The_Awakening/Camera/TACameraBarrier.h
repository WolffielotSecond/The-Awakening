#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TACameraBarrier.generated.h"

class UBoxComponent;
class UArrowComponent;

/**
 * 镜头挡墙：只挡摄像机、不挡玩家的双面碰撞体，放在两个场景之间的隔断墙上。
 * 摄像机被挡在玩家所在的一侧，碰到挡墙就停住；隔断墙本身挡住墙后的场景，所以画面看不到下一个场景。
 * 玩家从挡墙一侧完全走到另一侧后，摄像机在 CrossSeconds 内平滑穿过挡墙，然后被挡在新的一侧。
 * 摄像机碰不到挡墙（在盒子的宽度 / 高度范围外）时不起作用。
 * 关卡里请放 BP_CameraBarrier，不要直接放本 C++ 类。摆放方法见 Docs/CameraBarrierGuide.md。
 */
UCLASS(meta = (DisplayName = "Camera Barrier"))
class THE_AWAKENING_API ATACameraBarrier : public AActor
{
	GENERATED_BODY()

public:
	ATACameraBarrier();

	/** 摄像机离挡墙表面至少保持的距离（cm），相当于摄像机的碰撞半径。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Barrier", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CameraRadiusCm = 10.f;

	/** 玩家走到另一侧后，摄像机穿过挡墙用的秒数。0 为立即到位。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Barrier", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CrossSeconds = 0.5f;

	/** 关闭后本挡墙不参与计算（调试用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Barrier")
	bool bBarrierEnabled = true;

	/** 挡墙厚度方向（盒子 X 轴，世界坐标单位向量）。+1 侧在箭头方向，-1 侧在箭头背面。 */
	FVector GetBarrierNormal() const;

	/** 点在挡墙厚度方向上的本地坐标（cm）：0 为挡墙中心，±半厚度为两个表面。 */
	float GetLocalThickness(const FVector& WorldPoint) const;

	/** 盒子半厚度（cm，已含缩放）。 */
	float GetHalfThickness() const;

	/**
	 * 玩家位于哪一侧：+1 箭头方向，-1 箭头背面。
	 * 只有玩家完全离开盒子、到达另一面之外才换边；在盒子厚度内保持 PreviousSide，因此站在门里不会来回切换。
	 * PreviousSide 为 0 时（首次）按中心面判断。
	 */
	int32 ResolvePlayerSide(const FVector& PlayerLocation, int32 PreviousSide) const;

	/**
	 * 把摄像机挡在 Side 一侧需要沿 Side × Normal 推出的距离（cm，>= 0）。
	 * 摄像机不在盒子的宽度 / 高度范围内（碰不到挡墙）时返回 0。
	 */
	float ComputePush(const FVector& CameraLocation, int32 Side) const;

	/**
	 * 本挡墙对摄像机的总位移（世界坐标）。Side 为当前一侧；CrossAlpha 为从上一侧过渡到当前一侧的进度（0–1，已平滑）。
	 * 过渡期间在“挡在旧一侧”和“挡在新一侧”两种位置之间插值，摄像机平滑穿过挡墙。
	 */
	FVector ComputeDisplacement(const FVector& CameraLocation, int32 Side, float CrossAlpha) const;

	UBoxComponent* GetBarrierBox() const { return BarrierBox; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** X 为厚度方向（对齐墙的厚度）；Y / Z 是挡墙的宽度和高度，要盖到摄像机可能经过的位置。不参与碰撞。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Barrier")
	TObjectPtr<UBoxComponent> BarrierBox;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Camera Barrier")
	TObjectPtr<UArrowComponent> DirectionArrow;
#endif
};
