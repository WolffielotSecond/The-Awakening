// Source/The_Awakening/Story/TADialogueTypes.h
// 剧情系统：所有 JSON 数据结构、广播载荷、委托定义
#pragma once

#include "CoreMinimal.h"
#include "TADialogueTypes.generated.h"

class UTexture2D;

// ============================================================
// 立绘
// ============================================================

/** 立绘位置（归一化坐标） */
USTRUCT(BlueprintType)
struct FTAPortraitPosition
{
	GENERATED_BODY()

	/** 0~1：立绘中心的水平位置（0=屏幕左，1=屏幕右） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	float X = 0.25f;

	/** 0~1：立绘底部（脚底）的垂直位置（0=屏幕底，1=屏幕顶） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	float Y = 0.f;
};

/**
 * 一个角色的立绘描述。
 * 双重用途：
 *  - JSON 节点里：省略的字段表示「沿用当前状态」（表情差分靠只覆盖部分字段）
 *  - 运行时状态：控制器维护的当前立绘集合
 */
USTRUCT(BlueprintType)
struct FTAPortraitEntry
{
	GENERATED_BODY()

	/** 立绘身份键：同一 ID 视为同一张立绘（位置变化→平移；贴图变化→换表情） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait")
	FString CharacterId;

	/** 基础立绘贴图资产 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (EditCondition = "bImagesSpecified"))
	TSoftObjectPtr<UTexture2D> Base;

	/** 眼睛层：睁眼贴图 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (EditCondition = "bImagesSpecified"))
	TSoftObjectPtr<UTexture2D> EyesOpen;

	/** 眼睛层：闭眼贴图 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (EditCondition = "bImagesSpecified"))
	TSoftObjectPtr<UTexture2D> EyesClosed;

	/** 嘴巴层：张嘴贴图 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (EditCondition = "bImagesSpecified"))
	TSoftObjectPtr<UTexture2D> MouthOpen;

	/** 嘴巴层：闭嘴贴图 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (EditCondition = "bImagesSpecified"))
	TSoftObjectPtr<UTexture2D> MouthClosed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (DisplayName = "Apply Images"))
	bool bImagesSpecified = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (EditCondition = "bPositionSpecified"))
	FTAPortraitPosition Position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (DisplayName = "Apply Position"))
	bool bPositionSpecified = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (EditCondition = "bScaleSpecified"))
	float Scale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (DisplayName = "Apply Scale"))
	bool bScaleSpecified = false;

	/** false = 移除该角色立绘 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (EditCondition = "bVisibleSpecified"))
	bool bVisible = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portrait", meta = (DisplayName = "Apply Visibility"))
	bool bVisibleSpecified = false;
};

// ============================================================
// 事件
// ============================================================

/** 一个事件：广播名 + 字符串参数（JSON 中 params 可选） */
USTRUCT(BlueprintType)
struct FTAStoryEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	TMap<FString, FString> Params;
};

/** 事件广播载荷（蓝图中按 Payload.Name 分发，Payload.Params 取参数） */
USTRUCT(BlueprintType)
struct FTAStoryEventPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StoryEvent")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "StoryEvent")
	TMap<FString, FString> Params;

	FTAStoryEventPayload() {}
	FTAStoryEventPayload(const FString& InName, const TMap<FString, FString>& InParams)
		: Name(InName), Params(InParams) {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStoryEvent, const FTAStoryEventPayload&, Payload);

// ============================================================
// 条件
// ============================================================

/**
 * 分支选项的条件。数据驱动：Type 决定求值器（注册到 UTADialogueSubsystem），
 * 其余字段按类型使用；Value 统一存字符串（"true"/"100"/"0.5"），求值器自行转换。
 */
USTRUCT(BlueprintType)
struct FTAStoryCondition
{
	GENERATED_BODY()

	/** Flag / HasItem / Money / Attribute */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FString Type;

	/** Flag 类型：旗标名 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FString Flag;

	/** HasItem 类型：物品资产名（如 Item_Herb） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FString Item;

	/** Attribute 类型：属性名（如 Health） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FString Attribute;

	/** == / != / >= / <= / > / < */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FString Op = TEXT("==");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	FString Value = TEXT("0");

	/** HasItem 类型：至少持有数量 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	int32 Count = 1;
};

// ============================================================
// 节点 / 剧情
// ============================================================

/** 分支选项 */
USTRUCT(BlueprintType)
struct FTAStoryChoice
{
	GENERATED_BODY()

	/** 选项文本本地化 Key */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	FString TextId;

	/** 全部满足才显示（AND） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	TArray<FTAStoryCondition> Conditions;

	/** 选中时广播 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Choice")
	TArray<FTAStoryEvent> Events;

	/** 目标节点 ID（空 = 结束对话） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Choice")
	FString Target;
};

/** 剧情节点 */
USTRUCT(BlueprintType)
struct FTAStoryNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FString Id;

	/** dialogue / choice / end */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Node")
	FString Type = TEXT("dialogue");

	/** 说话角色的 CharacterId（空 = 旁白，无人动嘴） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FString SpeakerId;

	/** 角色名本地化 Key（空 = 隐藏名字框） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FString SpeakerNameId;

	/** 对话内容本地化 Key */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FString TextId;

	/** 本节点要更新/添加/移除的立绘（空数组 = 立绘不变） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	TArray<FTAPortraitEntry> Portraits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	TArray<FTAStoryEvent> EventsOnEnter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	TArray<FTAStoryEvent> EventsOnExit;

	/** type == choice 时使用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	TArray<FTAStoryChoice> Choices;

	/** dialogue 的下一个节点；空 = 结束 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Node")
	FString Next;
};

/** 整个剧情文件 */
USTRUCT(BlueprintType)
struct FTAStoryData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString StoryId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	FString Entry;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story")
	TArray<FTAStoryNode> Nodes;

	/** 解析后构建：Id → Nodes 下标 */
	TMap<FString, int32> IndexById;

	int32 FindNodeIndex(const FString& NodeId) const
	{
		const int32* Idx = IndexById.Find(NodeId);
		return Idx ? *Idx : INDEX_NONE;
	}

	/** 编辑后重建索引（忽略重复 ID） */
	void RebuildIndex()
	{
		IndexById.Reset();
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			if (!Nodes[i].Id.IsEmpty() && !IndexById.Contains(Nodes[i].Id))
			{
				IndexById.Add(Nodes[i].Id, i);
			}
		}
	}
};

// ============================================================
// 历史 / 存档
// ============================================================

/** 一条历史记录（存本地化 Key，切换语言时重新解析） */
USTRUCT(BlueprintType)
struct FTAStoryHistoryEntry
{
	GENERATED_BODY()

	/** 说话者名字 Key（空 = 旁白） */
	UPROPERTY(BlueprintReadOnly)
	FString SpeakerNameId;

	/** 文本 Key */
	UPROPERTY(BlueprintReadOnly)
	FString TextId;

	/** 是否为玩家选过的选项 */
	UPROPERTY(BlueprintReadOnly)
	bool bIsChoice = false;
};

/** 存档接口数据结构：未来的存档系统直接聚合此结构序列化 */
USTRUCT(BlueprintType)
struct FTAStorySaveData
{
	GENERATED_BODY()

	/** 剧情旗标 */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TMap<FString, int32> Flags;

	/** storyId → 最后到达的节点 ID */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TMap<FString, FString> StoryProgress;

	/** 已完成的剧情 */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	TArray<FString> CompletedStories;
};

// ============================================================
// 控制器 → UI 委托（无参：UI 侧从控制器拉取状态）
// ============================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStoryLineShown);        // 新的一句开始展示
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStoryChoicesChanged);   // 选项集合/高亮变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStoryPortraitsChanged); // 立绘集合变化（增删/换图/位置）
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStorySpeakerChanged);   // 说话者变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStoryHistoryChanged);   // 历史记录变化
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStoryFinished);         // 对话结束
