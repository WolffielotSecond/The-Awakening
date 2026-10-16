#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UI/TAPromptTypes.h"
#include "TAPromptSettings.generated.h"

class UUserWidget;

/** 世界交互提示的集中配置。项目设置 → Game → The Awakening Prompts，保存在 DefaultGame.ini。 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "The Awakening Prompts"))
class THE_AWAKENING_API UTAPromptSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 提示锚点没有单独指定 Widget Class 时使用。推荐父类为 UTAWorldPromptWidget；普通 UserWidget 也可，按控件名 Image_Key / Text_Prompt / SizeBox_Key 填充。 */
	UPROPERTY(Config, EditAnywhere, Category = "Widget")
	TSoftClassPtr<UUserWidget> DefaultPromptWidgetClass{FSoftObjectPath(TEXT("/Game/UI/WBP_InteractPrompt.WBP_InteractPrompt_C"))};

	/** 按键图标高度（保持贴图宽高比）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Widget", meta = (ClampMin = "8"))
	float IconHeight = 56.f;

	/** 锚点和元素都没有指定文本 Key 时，按提示类型使用的默认本地化 Key。 */
	UPROPERTY(Config, EditAnywhere, Category = "Text")
	FString InteractTextId = TEXT("Interact_Default");
	UPROPERTY(Config, EditAnywhere, Category = "Text")
	FString ParkourJumpTextId = TEXT("Parkour_Jump");
	UPROPERTY(Config, EditAnywhere, Category = "Text")
	FString ParkourDropTextId = TEXT("Parkour_Drop");

	FString GetDefaultTextId(ETAPromptType Type) const
	{
		switch (Type)
		{
		case ETAPromptType::ParkourJump: return ParkourJumpTextId;
		case ETAPromptType::ParkourDrop: return ParkourDropTextId;
		default: return InteractTextId;
		}
	}

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
