#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAHealthHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UTAHealthComponent;

/**
 * 血量 HUD 父类。布局、样式全部在 WBP 里做；C++ 只找到自己玩家的生命组件并推送数值。
 * 控件均为可选命名绑定：ProgressBar_Health（百分比）、Text_Health（本地化 Key HUD_Health_Format）。
 * 心电图、受伤闪红等表现在蓝图里覆写 OnHealthPresentationChanged 实现。
 */
UCLASS(Abstract, Blueprintable)
class THE_AWAKENING_API UTAHealthHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTAHealthComponent* GetObservedHealthComponent() const { return ObservedHealth.Get(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 每次数值变化（含绑定时的初始同步，Delta 为 0）后调用，C++ 已更新可选控件。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthPresentationChanged(float Current, float Max, float Delta);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Health;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Health;

private:
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION()
	void HandleHealthChanged(float Current, float Max, float Delta);
	UFUNCTION()
	void HandleLanguageChanged();

	void ObservePawn(APawn* Pawn);
	void StopObserving();
	void ApplyPresentation(float Current, float Max, float Delta);

	TWeakObjectPtr<UTAHealthComponent> ObservedHealth;
};
