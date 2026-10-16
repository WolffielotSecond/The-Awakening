#pragma once

#include "CoreMinimal.h"
#include "Interaction/TAInteractableActor.h"
#include "TACheckpoint.generated.h"

class UArrowComponent;
class UBoxComponent;

/**
 * 复活点。玩家在交互范围内按交互键登记为当前复活点（最后一次交互的生效，回头交互旧复活点也生效）。
 * 交互提示复用 ATAInteractableActor 的提示组件；外观（Mesh、特效）在蓝图里做，激活表现写在 OnActivated / OnDeactivated。
 */
UCLASS()
class THE_AWAKENING_API ATACheckpoint : public ATAInteractableActor
{
	GENERATED_BODY()

public:
	ATACheckpoint();

	/** 唯一 ID，以后存档记录这个；调试命令 TACheckpoint <Id> 也用它。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	FName CheckpointId;

	/** 打开后玩家进入交互范围即自动登记，用于不需要交互的剧情节点。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	bool bActivateOnOverlap = false;

	/** 关卡开始时的默认复活点，每关一个。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	bool bIsLevelStart = false;

	/** 复活位置与朝向（箭头方向）。 */
	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	FTransform GetSpawnTransform() const;

	UFUNCTION(BlueprintPure, Category = "Checkpoint")
	bool IsActiveCheckpoint() const { return bActive; }

	/** 由复活子系统调用，切换激活表现。 */
	void SetActiveCheckpoint(bool bInActive);

	// ITAInteractable
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 成为当前复活点时（美术做激活特效）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint")
	void OnActivated();
	/** 被其他复活点取代时。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint")
	void OnDeactivated();

	UFUNCTION()
	void HandleRangeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 复活位置和朝向，可以和交互范围分开摆。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> SpawnPoint;

	/** 交互范围。阻挡 Visibility 供角色的交互查询发现，重叠 Pawn 供自动登记。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionRange;

private:
	bool bActive = false;
};
