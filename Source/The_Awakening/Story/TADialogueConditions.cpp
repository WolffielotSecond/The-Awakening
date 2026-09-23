// Source/The_Awakening/Story/TADialogueConditions.cpp
#include "Story/TADialogueConditions.h"
#include "Story/TADialogueSubsystem.h"
#include "Core/TAPlayerState.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** 数值比较：L Op R */
	bool CompareValues(double L, const FString& Op, double R)
	{
		if (Op == TEXT("==")) return L == R;
		if (Op == TEXT("!=")) return L != R;
		if (Op == TEXT(">=")) return L >= R;
		if (Op == TEXT("<=")) return L <= R;
		if (Op == TEXT(">"))  return L > R;
		if (Op == TEXT("<"))  return L < R;
		return false;
	}

	/** 按资产名解析物品定义：常规路径直读（打包后同样有效）→ AssetRegistry 兜底 */
	UTAItemDefinition* ResolveItemByName(const FString& ItemName)
	{
		if (ItemName.IsEmpty())
		{
			return nullptr;
		}

		// 1) 常规路径：/Game/Inventory/{Name}.{Name}
		if (UTAItemDefinition* Def = LoadObject<UTAItemDefinition>(nullptr,
			*(FString::Printf(TEXT("/Game/Inventory/%s.%s"), *ItemName, *ItemName))))
		{
			return Def;
		}

		// 2) AssetRegistry 兜底：物品不在 /Game/Inventory 时按类 + 资产名搜索
		FAssetRegistryModule& ARMod = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FARFilter Filter;
		Filter.ClassPaths.Add(UTAItemDefinition::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		TArray<FAssetData> Assets;
		ARMod.Get().GetAssets(Filter, Assets);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetName.ToString().Equals(ItemName, ESearchCase::IgnoreCase))
			{
				return Cast<UTAItemDefinition>(Asset.GetAsset());
			}
		}

		return nullptr;
	}
}

// ---------------- Flag ----------------

bool FTADialogueFlagEvaluator::Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const
{
	if (!Context.Dialogue || Condition.Flag.IsEmpty())
	{
		return false;
	}

	const int32 FlagValue = Context.Dialogue->GetFlagInt(Condition.Flag);
	const FString& Wanted = Condition.Value;

	if (Wanted.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
		Wanted.Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		const bool bWanted = Wanted.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		const bool bActual = FlagValue != 0;
		if (Condition.Op == TEXT("==")) return bActual == bWanted;
		if (Condition.Op == TEXT("!=")) return bActual != bWanted;
		return CompareValues(bActual ? 1.0 : 0.0, Condition.Op, bWanted ? 1.0 : 0.0);
	}

	return CompareValues((double)FlagValue, Condition.Op, FCString::Atod(*Wanted));
}

// ---------------- HasItem ----------------

bool FTADialogueHasItemEvaluator::Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const
{
	if (Context.bUseOverrides)
	{
		return Context.HasOverrideItem(Condition.Item) ? 1 >= Condition.Count : false;
	}

	if (!Context.Inventory)
	{
		return false;
	}

	UTAItemDefinition* ItemDef = ResolveItemByName(Condition.Item);
	if (!ItemDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] HasItem: 找不到物品资产 %s"), *Condition.Item);
		return false;
	}

	return Context.Inventory->GetItemCount(ItemDef) >= Condition.Count;
}

// ---------------- Money ----------------

bool FTADialogueMoneyEvaluator::Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const
{
	if (Context.bUseOverrides)
	{
		return CompareValues((double)Context.OverrideMoney, Condition.Op, FCString::Atod(*Condition.Value));
	}

	if (!Context.PlayerState)
	{
		return false;
	}

	return CompareValues((double)Context.PlayerState->GetMoney(), Condition.Op, FCString::Atod(*Condition.Value));
}

// ---------------- Attribute ----------------

bool FTADialogueAttributeEvaluator::Evaluate(const FTAStoryCondition& Condition, const FTADialogueConditionContext& Context) const
{
	// 编辑器预览没有真实属性集，返回 false
	if (!Context.PlayerState)
	{
		return false;
	}

	UTAAttributeSet* AttrSet = Context.PlayerState->GetAttributeSet();
	if (!AttrSet)
	{
		return false;
	}

	TArray<FGameplayAttribute> Attributes;
	UAttributeSet::GetAttributesFromSetClass(UTAAttributeSet::StaticClass(), Attributes);
	for (const FGameplayAttribute& Attribute : Attributes)
	{
		if (Attribute.GetName().Equals(Condition.Attribute, ESearchCase::IgnoreCase))
		{
			return CompareValues((double)Attribute.GetNumericValue(AttrSet), Condition.Op, FCString::Atod(*Condition.Value));
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Dialogue] Attribute: 找不到属性 %s"), *Condition.Attribute);
	return false;
}
