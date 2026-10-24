#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "UI/TAPromptTypes.h"
#include "TAInteractionPromptAnchor.generated.h"

class UTexture2D;
class UTASpecialActionComponent;

/**
 * 世界交互提示锚点：挂在任何可交互元素上（跑酷标记、复活点、剧情触发器、纯蓝图的 ITAInteractable Actor），
 * 玩家身上的 UTAPromptComponent 只通过它显示 / 隐藏 / 刷新提示，不再关心元素的具体类型。
 * 摆放位置就是本组件的位置；控件类不填时用项目设置里的默认提示控件。
 * 显隐只由提示系统控制，元素自己不要调用 SetVisibility。
 * 掉落物品不使用本组件，它的提示由拾取列表 UI 负责。
 */
UCLASS(ClassGroup = (TheAwakening), meta = (BlueprintSpawnableComponent))
class THE_AWAKENING_API UTAInteractionPromptAnchor : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UTAInteractionPromptAnchor();

	/** 决定使用哪个按键（交互 / 跨越 / 跳下）。跑酷标记由 MarkerType 自动设置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	ETAPromptType PromptType = ETAPromptType::Interact;

	/** 本地化 Key。留空时依次使用元素的 InteractTextId（ATAInteractableActor）、项目设置里该类型的默认 Key。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prompt")
	FString TextId;

	/** 实际使用的本地化 Key（按上面的回退顺序）。 */
	FString ResolveTextId() const;

	/** 显示并填入图标与文字。控件类为空时先套用默认提示控件。 */
	void ShowPrompt(UTexture2D* KeyIcon, const FText& PromptText);
	void ShowPromptChord(const TArray<UTexture2D*>& Icons, const TArray<FText>& KeyLabels, const FText& PromptText,
		const FText& FirstIconSuffix = FText::GetEmpty());
	void ShowQTE(UTASpecialActionComponent* Action);
	void HidePrompt();
	bool IsPromptShown() const { return bPromptShown; }

	/** 找到 Actor 上指定类型的提示锚点。 */
	static UTAInteractionPromptAnchor* FindAnchor(const AActor* Actor, ETAPromptType Type);

private:
	UPROPERTY(Transient) TObjectPtr<class UImage> ChordOriginalImage;
	UPROPERTY(Transient) TObjectPtr<class USizeBox> ChordSizeBox;
	void RestoreChord();
	bool EnsurePromptWidget();
	bool bPromptShown = false;
	bool bQTEWidget = false;
	UPROPERTY(Transient) TSubclassOf<UUserWidget> PreviousWidgetClass;
	void RestoreNormalWidget();
};
