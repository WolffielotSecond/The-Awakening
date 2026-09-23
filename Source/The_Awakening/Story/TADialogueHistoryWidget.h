// Source/The_Awakening/Story/TADialogueHistoryWidget.h
// 只读历史面板（WBP_DialogueHistory 的 C++ 基类）
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Story/TADialogueTypes.h"
#include "TADialogueHistoryWidget.generated.h"

class UScrollBox;
class UVerticalBox;
class UTextBlock;

/**
 * 控件命名约定（WBP_DialogueHistory）：
 *   ScrollBox_History（UScrollBox）
 *   Box_Entries（UVerticalBox，放在 ScrollBox 内）
 * 只回看（不可点击跳转），自动滚到底部；切换语言后整体重建。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTADialogueHistoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 重建历史列表（控制器 HistoryChanged / 语言切换时调用） */
	void SetHistory(const TArray<FTAStoryHistoryEntry>& InHistory);

protected:
	void Rebuild();
	FText ResolveText(const FString& TextId) const;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> ScrollBox_History;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Entries;

	TArray<FTAStoryHistoryEntry> History;
};
