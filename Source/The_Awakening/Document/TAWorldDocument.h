#pragma once

#include "CoreMinimal.h"
#include "Inventory/TAWorldItem.h"
#include "GameplayTagContainer.h"
#include "Health/TARollbackSubsystem.h"
#include "TAWorldDocument.generated.h"

class UTADocumentDefinition;
class UTACharacterActionComponent;
class UStaticMesh;
class USkeletalMeshComponent;

/**
 * 场景中的可收集文本。和普通物品一样出现在拾取列表里，可扫描。
 * 交互的那一帧检查玩家剧情槽里有没有日记本，之后两条路完全分开：
 * - 没有日记本：标记已读（不收集）→ 拿起动画（关键帧隐藏场景模型）→ 时停并打开阅读 → 关闭后放下动画（关键帧重新显示模型）。
 *   Actor 保留，之后还能再拿起；拿到日记本后回来可以正式收集。阅读期间死亡：已读旗标随回滚撤销，模型恢复。
 * - 有日记本：交互帧立即收集并销毁 Actor，角色播放塞入日记本动画（只是表现，被打断也不影响收集）。
 */
UCLASS()
class THE_AWAKENING_API ATAWorldDocument : public ATAWorldItem
{
	GENERATED_BODY()

public:
	ATAWorldDocument();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ITAInteractable
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;

	// ATAWorldItem
	virtual FString GetDisplayNameTextId() const override;
	virtual UTexture2D* GetDisplayIcon() const override;
	virtual bool ShouldReportPickupFailure() const override { return false; }
	/** 骨骼网格体模型时不需要静态网格的占位 */
	virtual void ApplyFallbackMesh() override;

	UFUNCTION(BlueprintPure, Category = "Document")
	UTADocumentDefinition* GetDefinition() const { return Definition; }

	/** 拿起、阅读、放下期间为 true，不能再次交互 */
	UFUNCTION(BlueprintPure, Category = "Document")
	bool IsBusy() const { return State != EState::Idle; }

	/** 阅读界面关闭后调用：播放放下动画。不在阅读中时无效果。 */
	UFUNCTION(BlueprintCallable, Category = "Document")
	void FinishReading();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	TObjectPtr<UTADocumentDefinition> Definition;

	/** 文本定义的模型是骨骼网格体时用它显示（保持参考姿势），静态网格组件隐藏 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	/** 文本定义里没有模型时显示的占位模型（薄片） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	TObjectPtr<UStaticMesh> PlaceholderMesh;

private:
	enum class EState : uint8 { Idle, PickingUp, Reading, PuttingDown };

	void BeginReadPath(AActor* Interactor);
	void CollectPath(AActor* Interactor);
	void OpenReader();
	/** 立即回到空闲：强制关闭阅读界面、显示模型。死亡回滚和动作被打断时调用。 */
	void AbortReading();
	void EndPutDown();
	void SetWorldMeshVisible(bool bVisible);
	void BindAction(UTACharacterActionComponent* InAction);
	void UnbindAction();
	bool HasDiary(AActor* Interactor) const;
	FText Localize(const FString& TextId) const;

	UFUNCTION()
	void HandleActionKeyframe(FGameplayTag ActionTag, FName KeyName);

	/** 当前用骨骼网格组件显示模型 */
	bool bUsesSkeletalMesh = false;
	EState State = EState::Idle;
	/** 每次开始 / 中止都递增，丢弃过期的动作回调 */
	uint32 FlowSerial = 0;
	TWeakObjectPtr<UTACharacterActionComponent> ActionComponent;
	FTARollbackScopeHandle ReadingScope;
	TWeakObjectPtr<class UTADocumentReaderWidget> Reader;
	TWeakObjectPtr<AActor> ReaderInstigator;
};
