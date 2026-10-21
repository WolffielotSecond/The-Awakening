#pragma once
#include "Commandlets/Commandlet.h"
#include "TADocumentUICommandlet.generated.h"

/**
 * 生成文本系统 UI 的默认资源（已存在的不覆盖，之后在编辑器里自由调整）：
 * - /Game/Input/Actions/UI/IA_ReaderScroll（Axis1D，正值向下）并在 IMC_UI 中映射 ↓ / ↑ / 右摇杆 Y
 * - /Game/Input/Actions/UI/IA_DocumentTranslate（Boolean）、IA_DocumentRotate（Axis2D）：只建动作，按键在 IMC_UI 中自行配置
 * - /Game/UI/Document/WBP_DocumentReader（父类 UTADocumentReaderWidget）：变暗变糊背景 + 居中 3D 模型 + 居中无底框转译 + 底部操作栏
 * - /Game/Input/Actions/UI/IA_ItemActions、IA_DiaryPreviousPage、IA_DiaryNextPage（Boolean）：只建动作，按键在 IMC_UI 中自行配置
 * - /Game/UI/Inventory/WBP_ItemActionMenu、WBP_ItemActionOption：物品交互菜单
 * - /Game/UI/Document/WBP_Diary：日记本
 * - /Game/UI/PickUp/WBP_PickupBanner：拾取提示（-RebuildBanner 重建）
 * - /Game/UI/Inventory/WBP_ItemInspect：物品检视（-RebuildInspect 重建）
 * - /Game/Input/Actions/UI/IA_QuantityDecrease、IA_QuantityIncrease（Boolean，按键自行配置）与 /Game/UI/Inventory/WBP_QuantityDialog：数量小窗口（-RebuildQuantity 重建）
 * 用法：UnrealEditor-Cmd The_Awakening.uproject -run=TADocumentUI [-Rebuild] [-RebuildMenu] [-RebuildDiary]
 *   -Rebuild 重建 WBP_DocumentReader，-RebuildMenu 只重建 WBP_ItemActionMenu，-RebuildDiary 重建菜单与日记本；
 *   重建前都先备份到 Saved/LayoutBackups
 */
UCLASS()
class UTADocumentUICommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTADocumentUICommandlet();
	virtual int32 Main(const FString& Params) override;
};
