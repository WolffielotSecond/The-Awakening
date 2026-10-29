#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "TARoomRegion.generated.h"

class UBoxComponent;
class UMeshComponent;
class UMaterialInterface;
class UTARoomCameraProfile;

/** 黑场时被替换材质的组件及其原材质；强引用原材质，避免替换期间被 GC。 */
USTRUCT()
struct FTARoomSavedMaterials
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> Component;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;
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
	 * 玩家不在本房间（也不在通向本房间的门口）时，这些 Actor 上的所有网格换成黑场材质。
	 * 用吸管选家具、室内装饰等；与当前走廊共用的墙、地板、门框不要放进来。只换材质，碰撞、交互、Tick 不变。
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room|Blackout")
	TArray<TObjectPtr<AActor>> DetailActors;

	/** 黑场材质，BP_RoomRegion 默认是 MI_RoomBlackout。为空时不做黑场。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room|Blackout")
	TObjectPtr<UMaterialInterface> BlackoutMaterial;

	bool ContainsPoint(const FVector& WorldPoint) const;

	/** 切换黑场；状态不变时不做任何事。 */
	void SetBlackedOut(bool bInBlackedOut);

	UFUNCTION(BlueprintPure, Category = "Room|Blackout")
	bool IsBlackedOut() const { return bBlackedOut; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Room")
	TObjectPtr<UBoxComponent> RegionBox;

private:
	void RestoreMaterials();

	UPROPERTY(Transient)
	TArray<FTARoomSavedMaterials> SavedMaterials;

	bool bBlackedOut = false;
};

/**
 * 门口预览区域。玩家脚下位置进入这个 Box，且其中一侧是玩家当前所在房间时，两侧房间都正常显示。
 * 室内门常开，所以不判断门的开关。离开时用放大 ExitMarginCm 的范围判断，避免在门槛来回闪。
 * 关卡里请放 BP_RoomPortal。
 */
UCLASS()
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

	bool ContainsPoint(const FVector& WorldPoint, bool bUseExitMargin) const;
	bool Connects(FName RoomId) const { return RoomId != NAME_None && (RoomA == RoomId || RoomB == RoomId); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Portal")
	TObjectPtr<UBoxComponent> PortalBox;
};

/** 已加载区域与门口的轻量登记表；在 BeginPlay / EndPlay 成对注册注销，子关卡卸载后自动消失。 */
UCLASS()
class THE_AWAKENING_API UTARoomRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTARoomRegistrySubsystem* Get(const UObject* WorldContext);

	void RegisterRegion(ATARoomRegion* Region);
	void UnregisterRegion(ATARoomRegion* Region);
	void RegisterPortal(ATARoomPortal* Portal);
	void UnregisterPortal(ATARoomPortal* Portal);

	/**
	 * 返回包含该点的最高优先级区域。同优先级时保留 Previous（防止在重叠处来回切换），
	 * 否则按 RoomId 字典序稳定选择。没有区域包含时返回 nullptr。
	 */
	ATARoomRegion* ResolveRegion(const FVector& WorldPoint, const ATARoomRegion* Previous) const;

	/** 收集包含该点、且连接 ActiveRoomId 的门口；InOutActivePortals 为上一帧结果，用于离开时的放大范围。 */
	void ResolvePortals(const FVector& WorldPoint, FName ActiveRoomId, TArray<TWeakObjectPtr<ATARoomPortal>>& InOutActivePortals) const;

	/** bShowAll 为 true 时所有房间正常显示；否则不在 VisibleRooms 里的房间黑场。 */
	void ApplyRoomVisibility(const TSet<FName>& VisibleRooms, bool bShowAll) const;

private:
	TArray<TWeakObjectPtr<ATARoomRegion>> Regions;
	TArray<TWeakObjectPtr<ATARoomPortal>> Portals;
};
