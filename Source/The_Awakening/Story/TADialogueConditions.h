// Source/The_Awakening/Story/TADialogueConditions.h
// 条件求值上下文 + 可扩展求值器接口 + 内置求值器
#pragma once

#include "CoreMinimal.h"
#include "Story/TADialogueTypes.h"

class UWorld;
class ATAPlayerState;
class UTAInventoryComponent;
class UTADialogueSubsystem;

/**
 * 条件求值上下文。
 * 运行时：填入真实 World/PlayerState/Inventory/Dialogue；
 * 编辑器预览：额外用 bUseOverrides + Override* 模拟游戏状态。
 */
struct THE_AWAKENING_API FTADialogueConditionContext
{
	UWorld* World = nullptr;
	ATAPlayerState* PlayerState = nullptr;          // Money / Attribute
	UTAInventoryComponent* Inventory = nullptr;     // HasItem
	const UTADialogueSubsystem* Dialogue = nullptr; // Flag

	// ---- 编辑器预览模拟数据（bUseOverrides=true 时优先使用）----
	bool bUseOverrides = false;
	int32 OverrideMoney = 0;
	/** 资产名列表（每个视为持有 1 个） */
	TArray<FString> OverrideItems;

	bool HasOverrideItem(const FString& ItemName) const
	{
		return OverrideItems.ContainsByPredicate(
			[&](const FString& S) { return S.Equals(ItemName, ESearchCase::IgnoreCase); });
	}
};

/**
 * 条件求值器接口。
 * 注册到 UTADialogueSubsystem::RegisterConditionEvaluator 即可扩展新条件类型，
 * 无需修改剧情系统本体代码。
 */
class THE_AWAKENING_API ITADialogueConditionEvaluator
{
public:
	virtual ~ITADialogueConditionEvaluator() {}

	/** 返回条件类型名（与 JSON 中 "type" 字段一致），如 "Flag" */
	virtual FString GetTypeName() const = 0;

	virtual bool Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const = 0;
};

// ---------------- 内置求值器（声明在此，子系统注册；实现见 .cpp） ----------------

/** Flag：剧情旗标比较（"true"/"false" 按布尔，其余按数值） */
class FTADialogueFlagEvaluator : public ITADialogueConditionEvaluator
{
public:
	virtual FString GetTypeName() const override { return TEXT("Flag"); }
	virtual bool Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const override;
};

/** HasItem：背包持有 ≥ Count 个指定物品（物品用资产名引用） */
class FTADialogueHasItemEvaluator : public ITADialogueConditionEvaluator
{
public:
	virtual FString GetTypeName() const override { return TEXT("HasItem"); }
	virtual bool Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const override;
};

/** Money：玩家金钱比较 */
class FTADialogueMoneyEvaluator : public ITADialogueConditionEvaluator
{
public:
	virtual FString GetTypeName() const override { return TEXT("Money"); }
	virtual bool Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const override;
};

/** Attribute：GAS 属性比较（属性名如 Health） */
class FTADialogueAttributeEvaluator : public ITADialogueConditionEvaluator
{
public:
	virtual FString GetTypeName() const override { return TEXT("Attribute"); }
	virtual bool Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const override;
};
