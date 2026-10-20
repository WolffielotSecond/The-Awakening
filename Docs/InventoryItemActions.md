# 背包物品交互菜单

背包里光标停在物品上，按 `IA_ItemActions`（在 `IMC_UI` 里配置）弹出二级菜单，列出能对这个物品做的操作。文本、日记本见 [DocumentSystem.md](DocumentSystem.md)。

## 选项

| 物品 | 选项 |
|---|---|
| 普通物品，不可堆叠 | 检视、丢弃 |
| 普通物品，可堆叠 | 检视、部分丢弃、拆分堆叠（背包没有空格子时置灰） |
| 日记本 | 查看内容 |
| 其他剧情物品 | 检视 |

剧情物品不能丢弃、不能拆分。置灰的选项可以被选中，但不能执行。

## 菜单操作 `WBP_ItemActionMenu`

父类 `UTAItemActionMenuWidget`，操作沿用背包的逻辑（不是暂停菜单的）：

- 任何时候都有一个选中项，打开时固定选中第一项。
- 上下选择（`IA_ChoicePrevious` / `IA_ChoiceNext`）换选中项；光标（鼠标或手柄虚拟光标）移到某项上即选中它。
- 确定键（`IA_Inventory_Confirm`、`IA_ChoiceConfirm`）执行选中项；鼠标点击执行被点的那一项。
- 点击菜单以外的地方关闭菜单，**这次点击被菜单吃掉**，不会点到后面的背包。返回键关闭菜单。
- 菜单出现在光标旁（`CursorOffset`）。时停沿用背包的。

控件命名：`Panel_Menu`（菜单本体）、`VerticalBox_Options`（选项由 C++ 用 `OptionWidgetClass` 生成）、根 Canvas 上一层全屏透明的可点击控件接住空白处点击。选项行 `WBP_ItemActionOption`：`Button_Option`、`Text_Option`，可选 `Border_Highlight`。

输入优先级：背包 200 < 菜单 210 < 数量窗口 215 < 日记本 / 检视 220。

## 丢弃 / 部分丢弃

丢弃直接把整叠丢到地上；部分丢弃先弹数量窗口（1 ~ 当前数量）。

落点规则（策划文档）：先试脚下；脚下已有物品时，从角色正前方开始每 60° 试一个方向。被墙挡住、高低差超过 `DropMaxStepHeight`（30）、落点与角色的夹角超过 `DropMaxSlopeAngle`（45°）、附近已有物品的位置不能落。都不能落时背包提示「当前位置无法掉落物品」（`Inventory_DropBlocked`），物品留在原格子。

地上生成的物品用物品定义的 `WorldItemClass`（例如 `BP_WorldItem_Herb`）；没配时生成基础 `TAWorldItem`，模型用 `PreviewMesh`。

## 拆分堆叠

弹数量窗口（1 ~ 当前数量），拿出的数量放进第一个空格，**不和其他同类堆叠合并**。没有空格时选项置灰。背包不再自动整理，拆出来的堆叠会一直留着。

## 数量窗口 `WBP_QuantityDialog`

父类 `UTAQuantityDialogWidget`，策划文档里的「< 10 >」加确定 / 取消，打开时数量为最小值。

- 键鼠点按钮；手柄可以用虚拟光标点按钮，也可以直接按 `IA_QuantityDecrease` / `IA_QuantityIncrease` 调数量，确定键确认，返回键取消。
- 点在窗口以外会被吃掉（不穿透到背包），但不关闭窗口。

控件命名：`Panel_Dialog`、`Text_Title`、`Button_Decrease`、`Button_Increase`、`Text_Quantity`、`Button_Confirm`、`Button_Cancel`，可选 `Text_Confirm`、`Text_Cancel`、`HorizontalBox_Controls`（底部按键提示，可点击）。

## 检视 `WBP_ItemInspect`

父类 `UTAItemInspectWidget`。背包变暗，3D 模型在屏幕中央（预览台槽位 4），名称和介绍在模型下方。

- 旋转和文本预览相同：轴固定、可无限翻转；鼠标在模型上拖动，手柄用 `IA_DocumentRotate`。
- 底部「旋转 / 返回」，旋转只在手柄下显示，返回可以点击。
- 打开 / 关闭淡入淡出，动画没播完前返回无效。时停沿用背包的；手柄时隐藏光标。

控件命名：`Image_Model`、`Text_Name`、`Text_Description`、`HorizontalBox_Controls`。模型用物品定义的 `PreviewMesh` 和 `PreviewTransform`。

## 生成

`-run=TADocumentUI` 生成 `IA_ItemActions`、`IA_QuantityDecrease`、`IA_QuantityIncrease` 和上面的 WBP，已有的不覆盖；`-RebuildMenu`、`-RebuildQuantity`、`-RebuildInspect` 重建对应 WBP，旧的备份到 `Saved/LayoutBackups`。
