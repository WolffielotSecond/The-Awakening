// Source/The_Awakening/Story/ATAStoryTriggerActor.h
// 剧情触发器：玩家按交互键后播放指定剧情
#pragma once

#include "CoreMinimal.h"
#include "Interaction/TAInteractableActor.h"
#include "Story/TAStoryAsset.h"
#include "ATAStoryTriggerActor.generated.h"

/**
 * 指向一个已编译的 Unreal 剧情资产。
 * 交互提示自动使用 Interact_Talk（"对话"）。
 */
UCLASS()
class THE_AWAKENING_API ATAStoryTriggerActor : public ATAInteractableActor
{
	GENERATED_BODY()

public:
	ATAStoryTriggerActor();

	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	/** 提供给交互扫描的碰撞体；默认半径与附近交互提示检测距离匹配。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class USphereComponent> TriggerCollision;

	/** 要播放的 Unreal 剧情资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TObjectPtr<UTAStoryAsset> StoryAsset;

	/** 是否只触发一次 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	bool bTriggerOnce = true;

protected:
	bool bPlayed = false;
};
