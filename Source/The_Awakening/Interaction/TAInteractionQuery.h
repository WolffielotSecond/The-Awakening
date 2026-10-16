#pragma once

#include "CoreMinimal.h"

class APawn;
class AActor;

/** 交互目标查询。角色执行交互与世界提示共用这一个函数，保证“提示的就是会交互的”。 */
struct THE_AWAKENING_API FTAInteractionQuery
{
	/**
	 * Visibility 通道球形查询；实现 ITAInteractable、CanInteract 通过、不是掉落物品（掉落物品由拾取列表负责），
	 * 取离角色最近的一个。没有阻挡命中时返回空。
	 */
	static AActor* FindBestInteractable(const APawn* Pawn, float Radius);
};
