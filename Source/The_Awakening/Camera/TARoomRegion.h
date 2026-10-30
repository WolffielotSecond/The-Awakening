#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "TARoomRegion.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UTARoomCameraProfile;

/** 黑场前组件的渲染设置，退出黑场时原样还原。 */
USTRUCT()
struct FTARoomSavedRenderState
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> Component;

	bool bRenderInMainPass = true;
	bool bRenderInDepthPass = true;
	bool bRenderCustomDepth = false;
	int32 CustomDepthStencilValue = 0;
};

/**
 * 房间 / 走廊 / 室外等空间区域。角色脚下位置落在本 Actor 的任意一个 Box 里就算在这个区域。
 * 不规则房间可以在实例上再添加 Box 组件拼接。Box 不参与碰撞，只用于点包含判定。
 * 关卡里请放 BP_RoomRegion，不要直接放本 C++ 类。
 */
UCLASS()
class THE_AWAKENING_API ATARoomRegion : public AActor
{
	GENERATED_BODY()

public:
	ATARoomRegion();

	/** 稳定的区域 ID，例如 Home.Hall、Home.SisterBedroom。不要用 Actor 名代替。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	FName RoomId;

	/** 多个区域重叠时数值大的优先，例如门口小区域高于走廊。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	int32 Priority = 0;

	/** 本区域使用的镜头参数。为空时沿用角色蓝图上的默认镜头参数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Camera")
	TObjectPtr<UTARoomCameraProfile> CameraProfile;

	/**
	 * 本房间包含的 Actor。仅登记归属，不会自动黑场。
	 * 当前区域的 BlackoutRoomIds 引用本 RoomId 时，这些 Actor 才进入黑场。
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room|Included Assets")
	TArray<TObjectPtr<AActor>> IncludedAssets;

	/** 玩家进入本区域时，黑场这些 RoomId 对应区域的 Included Assets；不递归执行目标区域的规则。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Blackout")
	TArray<FName> BlackoutRoomIds;

	/** 玩家进入本区域时额外黑场的 Actor，例如墙；不要求登记在任何房间中。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room|Blackout")
	TArray<TObjectPtr<AActor>> BlackoutActors;

	/** 黑场写入的 CustomDepth Stencil 值。0 / 1 / 2 被扫描特效占用。 */
	static constexpr int32 BlackoutStencilValue = 3;

	bool ContainsPoint(const FVector& WorldPoint) const;

	/** 本区域的 RoomId 是否被当前区域的 BlackoutRoomIds 指定。 */
	UFUNCTION(BlueprintPure, Category = "Room|Blackout")
	bool IsBlackedOut() const { return bBlackedOut; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UBoxComponent> RegionBox;

private:
	friend class UTARoomRegistrySubsystem;
	bool bBlackedOut = false;
};

/**
 * 旧 Portal 的兼容壳，保留以便已有 BP / 地图正常加载。已停用，不参与任何镜头或黑场判定。
 */
UCLASS(NotPlaceable, meta = (DisplayName = "Room Portal (Legacy, Inactive)"))
class THE_AWAKENING_API ATARoomPortal : public AActor
{
	GENERATED_BODY()

public:
	ATARoomPortal();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	FName RoomA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	FName RoomB;

	/** 已经在预览时，离开判断范围向外放大的距离（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ExitMarginCm = 30.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<UBoxComponent> PortalBox;
};

/** 已加载区域登记表，统一按组件去重管理黑场和原始渲染状态。 */
UCLASS()
class THE_AWAKENING_API UTARoomRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTARoomRegistrySubsystem* Get(const UObject* WorldContext);

	void RegisterRegion(ATARoomRegion* Region);
	void UnregisterRegion(ATARoomRegion* Region);

	/**
	 * 返回包含该点的最高优先级区域。同优先级时保留 Previous（防止在重叠处来回切换），
	 * 否则按 RoomId 字典序稳定选择。没有区域包含时返回 nullptr。
	 */
	ATARoomRegion* ResolveRegion(const FVector& WorldPoint, const ATARoomRegion* Previous) const;

	/** 当前区域指定的房间资产与额外 Actor 的并集进入黑场；nullptr 清除黑场。 */
	void ApplyRoomVisibility(ATARoomRegion* ActiveRegion);
	virtual void Deinitialize() override;

private:
	TArray<TWeakObjectPtr<ATARoomRegion>> Regions;
	TWeakObjectPtr<ATARoomRegion> CurrentRegion;

	UPROPERTY(Transient)
	TArray<FTARoomSavedRenderState> SavedRenderStates;
};
