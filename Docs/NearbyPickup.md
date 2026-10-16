# 多物品拾取接入

版本：0.2.3-alpha.1。C++ 管理检测、选择和拾取，WBP 由你拼装。

## 创建两个 WBP

### 条目 WBP

建立 `WBP_PickupRow`，父类选 `TAPickupRowWidget`。
所有控件均为可选绑定；需要自动更新的控件请启用 Is Variable 并使用下列名称。

| 名称 | 类型 | 行为 |
| --- | --- | --- |
| Image_Item | Image | 物品图标；没有图标时隐藏 |
| Text_Name | TextBlock | 按物品 DisplayName 的文本 ID 查询多语言 |
| Text_Count | TextBlock | 剩余数量；数量为 1 时隐藏 |
| Border_Selection | Border | SelectedColor / NormalColor 控制高亮 |
| Image_InteractKey | Image | 当前 InteractAction 实际绑定的设备图标，仅选中行显示 |
| VerticalBox_Keys | VerticalBox | 上下选择提示容器；选中时显示，未选中时 Hidden，保留布局空间 |
| Image_PreviousKey | Image | 放在 VerticalBox_Keys 中，显示 PreviousAction 实际绑定的设备图标 |
| Image_NextKey | Image | 放在 VerticalBox_Keys 中，显示 NextAction 实际绑定的设备图标 |
| Text_Interact | TextBlock | 物品 PromptTextId 的多语言文本，仅选中行显示 |

可以把 Image_InteractKey 放在行左侧，复现示意图的提示位置。SelectedColor、NormalColor 在类默认值中设置。
如果使用其他高亮设计，覆写 `OnPresentationChanged`，读取 `bIsSelected` 和 `WorldItem`。
VerticalBox_Keys 和内部两个 Image 都勾选 Is Variable；选择改变、设备切换和按键映射重建时自动刷新。面板中的同名上下项提示仍为可选，可移除以避免重复显示。
不要在条目中接收 E、滚轮或键盘事件。

### 列表 WBP

建立 `WBP_PickupPanel`，父类选 `TAPickupPanelWidget`。
类默认值的 `RowWidgetClass` 指定为 `WBP_PickupRow`。
`Pickup | Layout` 中的 `RowVerticalPadding` 控制每个运行时条目的 ScrollBox Slot 上下 Padding，默认均为 -15，左右为 0。可以在面板类默认值修改，也可通过蓝图 `SetRowVerticalPadding` 即时更新已生成的条目。

| 名称 | 类型 | 行为 |
| --- | --- | --- |
| Scroll_Items | ScrollBox | 自动生成竖排条目、选中项自动滚入可见范围 |
| Image_PreviousKey | Image | 上一项动作的实际绑定图标，可选 |
| Image_NextKey | Image | 下一项动作的实际绑定图标，可选 |
| Text_Previous | TextBlock | 本地化“上一件物品”，可选 |
| Text_Next | TextBlock | 本地化“下一件物品”，可选 |
| Text_Feedback | TextBlock | 拾取失败反馈，可选 |

建议结构：Canvas/Overlay → 背景 → SizeBox（限制高度）→ Scroll_Items。
ScrollBox 设置 Vertical、ScrollBarVisibility 为 Visible（保持 AlwaysShowScrollbar=false）。
不要直接往 Scroll_Items 放静态子控件；运行时条目由父类管理。
面板为 HitTestInvisible、不获取焦点；滚轮通过 Enhanced Input 切换选中项，父类负责滚动。
本阶段没有鼠标拖动滚动条的交互：滚动条显示位置，玩家继续控制移动和视角。
若需要拖动滚动条，后续须定义独立的光标与输入所有权交互。

## 角色配置

在玩家角色 BP 的 NearbyPickupComponent 中，将 `PickupPanelClass` 设为 `WBP_PickupPanel`。
拾取半径在 BeginPlay 跟随角色 `InteractionDistance`；之后可通过组件的 PickupRadius 调整。
`bRequireLineOfSight` 默认为 false，保持原有范围拾取方式；打开后检测到物品原点的 Visibility 遮挡。
掉落物品没有头顶世界提示，交互提示只由本列表负责（不加入统一交互提示系统，见 [InteractionPrompts.md](InteractionPrompts.md)）。没有配置 WBP 时列表逻辑仍有效，但不会显示任何物品提示。

## 输入与提示

新增 `/Game/Input/IMC_Pickup`，控制器自动加载，并通过现有 TAKeyBindingService 创建每玩家运行时映射。

| 动作 | 默认键鼠 | 默认手柄 |
| --- | --- | --- |
| IA_PickupPrevious | 滚轮向上 | 十字键上 |
| IA_PickupNext | 滚轮向下 | 十字键下 |
| 角色已有 InteractAction | 现有 IMC 的绑定 | 现有 IMC 的绑定 |

角色的 PreviousPickupAction / NextPickupAction 默认指向新增动作，可在 BP 中替换。
控制器的 PickupInputMappingContext 也可替换；替换时确保上下项动作与角色引用一致。
修改已有 IMC 即可调整按键，没有运行时硬编码按键。上下项使用 Started，每次按键/滚轮脉冲移动一行，边界不循环。
绑定接入设置重映射服务，显示名称接入 Settings.Binding.IA_PickupPrevious/Next。
提示使用 TAInputIconSubsystem 的本玩家实际映射；设备切换、语言切换和映射重建自动刷新。

列表不申请输入所有权。拾取与切换都检查现有 Interact 权限和冻结状态；背包、对话、暂停、扫描等拒绝交互时隐藏列表。
移动与视角保持原权限。附近有物品时，列表选中项优先于普通门/对话交互；列表为空时走普通交互。

## 列表和拾取规则

- 只纳入 ATAWorldItem（含货币），按演员原点到玩家原点距离判断，检测结果按 Actor 去重。
- 第一次按距离排序；之后保留现有顺序，新物品按距离排序后追加。
- 选中项使用弱物品引用。移除选中项后，优先选择原列表下一件仍有效物品，再选择上一件。
- 每帧更新候选，数量与物品定义变化也刷新；执行拾取前再次核验范围、存在性与 CanInteract。
- 按键时原选中物品刚失效，本次不转移到其他物品或门；下一次按键按新选择执行。
- 按一次交互拾取当前物品的一组，沿用原 OnInteract、TryAddItem 和货币交易。只有背包实际接收后扣减数量；部分接收保留世界余量。
- 拾取失败保留选择与数量。Text_Feedback 默认显示多语言 Pickup_Failed；成功或列表清空后清除。
- 可覆写 `OnPickupFeedback(AddedCount, Message)` 做提示动画；可在动画结束调用 ClearFeedback。

若自行拼自定义 HUD，可用 SetPickupComponent 绑定面板，或者订阅组件 OnNearbyItemsChanged、OnPickupResult，读取 GetNearbyItems/GetSelectedItem/GetSelectedIndex。
不要在 WBP 直接调用世界物品 OnInteract 绕开验证。

## 验收

本次验证：UE 5.6 Development Editor 编译通过；TheAwakening.Pickup 三项、Input.Router 和 Input.IconResources 共五项自动化全部通过（无测试警告）；版本管理脚本 37 项检查通过。

自动化：`TheAwakening.Pickup`。实际 WBP 拼好后进行 PIE：

1. 单物品／多个物品／超过高度的列表，默认最近项、去重与自动滚动。
2. 滚轮上下、手柄十字键上下，边界不循环；按实际绑定拾取高亮项。
3. 选中中间项，移走前一项、当前项、最后一项；移动玩家时保持顺序与物品身份。
4. 背包满、部分堆叠、全部拾取、货币拾取。
5. 背包／对话／暂停／扫描期间不拾取、不切换；关闭后恢复列表。
6. 切换三种语言、键鼠／手柄、重绑定后提示刷新。
7. 无候选时隐藏；物品头顶没有提示；有物品时普通交互不会抢走拾取按键，其他元素的交互提示也会让出。

界面未创建，因此视觉布局、实际滚轮/手柄输入和 PIE 焦点效果需要上述验收。
