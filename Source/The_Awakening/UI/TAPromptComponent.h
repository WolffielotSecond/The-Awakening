#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/TAPromptTypes.h"
#include "TAPromptComponent.generated.h"

class UInputAction;

/**
 * 世界交互提示的唯一驱动者（挂在玩家角色上）。每帧为每种提示类型选出一个目标，通过目标身上的
 * UTAInteractionPromptAnchor 显示，其余隐藏。只在提示出现、切换语言 / 设备 / 键位时刷新内容。
 * - 交互：与执行共用 FTAInteractionQuery；附近有可拾取物品时按键交给拾取列表，交互提示也让给拾取列表。
 * - 跨越 / 跳下：分别处理，各自显示离角色最近且可跑酷的标记。
 * - 角色完全冻结或没有交互 / 跑酷权限时不显示。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAPromptComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTAPromptComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 主更新：选出各类型的目标并切换显隐。 */
	UFUNCTION(BlueprintCallable, Category = "Prompt")
	void UpdatePrompts();

	/** 语言 / 设备 / 键位变化后刷新当前显示中的提示。 */
	UFUNCTION(BlueprintCallable, Category = "Prompt")
	void RefreshVisiblePrompts();

	/** 当前某类型显示中的目标（测试与调试用）。 */
	AActor* GetShownSource(ETAPromptType Type) const;

	/** 与交互检测共用距离（角色 BeginPlay 时设为 InteractionDistance）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	float PromptRadius = 250.f;

	UPROPERTY(EditAnywhere, Category = "Prompt|Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, Category = "Prompt|Input")
	TObjectPtr<UInputAction> ParkourJumpAction;

	UPROPERTY(EditAnywhere, Category = "Prompt|Input")
	TObjectPtr<UInputAction> ParkourDropAction;

protected:
	void CollectInteractCandidate(TMap<ETAPromptType, FTAPromptCandidate>& Best) const;
	void CollectParkourCandidates(TMap<ETAPromptType, FTAPromptCandidate>& Best) const;
	void ApplyWinners(const TMap<ETAPromptType, FTAPromptCandidate>& Best);
	void ShowCandidate(const FTAPromptCandidate& Candidate);
	void HideAll();

	/** 当前显示中的提示（每种类型最多一个）。 */
	UPROPERTY()
	TMap<ETAPromptType, FTAPromptCandidate> ActivePrompts;

	TWeakObjectPtr<APawn> OwnerPawn;
};
