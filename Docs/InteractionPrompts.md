# 统一交互提示（世界内元素）

跑酷标记、复活点、剧情触发器以及其他可交互物的按键提示，统一由一套系统显示。菜单内的按键提示（`WBP_ActionPrompt`）不在此范围，见 [UIInputMapping.md](UIInputMapping.md)。**掉落物品不加入本系统**，它的提示只由拾取列表负责，见 [NearbyPickup.md](NearbyPickup.md)。

## 结构

| 部分 | 作用 |
|---|---|
| `UTAInteractionPromptAnchor`（提示锚点，`UI/`） | 挂在元素上的 WidgetComponent。决定提示显示在哪、用哪种按键、显示什么文字。显隐只由提示系统控制 |
| `UTAPromptComponent`（玩家角色上） | 每帧为每种提示类型选出一个目标并显示，其余隐藏 |
| `FTAInteractionQuery`（`Interaction/`） | 交互目标查询。角色执行交互和提示**共用**这一个函数 |
| `UTAPromptSettings`（项目设置 → Game → The Awakening Prompts） | 默认提示控件、图标高度、各类型默认文本 Key |
| `UTAWorldPromptWidget`（`UI/`） | 推荐的提示控件 C++ 父类，按名字绑定 `Image_Key` / `Text_Prompt` / `SizeBox_Key`，蓝图事件 `OnPromptChanged` 做动画 |

## 提示类型与选择规则

| 类型 | 按键 | 选择 |
|---|---|---|
| 交互 | Interact | 与执行相同：Visibility 球形查询内、`CanInteract` 通过、离角色最近的一个。附近有可拾取物品时交互键交给拾取列表，交互提示也让出 |
| 跨越 | ParkourJump | 进入范围、`CanPlayerParkourToMarker` 通过、离角色最近的跨台跳标记 |
| 跳下 | ParkourDrop | 同上，高台跳下标记；与跨越分别处理，可以同时显示 |

不显示提示的情况：角色完全冻结（扫描完全冻结、小游戏）、没有对应输入权限（菜单、对话、死亡等）。

**已知限制**：两个同类型跑酷标记同时重叠时，提示显示在最近的那个上，而执行仍按原规则选最后进入范围的那个，两者可能不一致（按设计保留原显示逻辑）。

## 给元素加提示

- **C++ 元素**：`ATAInteractableActor` 的子类（剧情触发器、复活点）自带锚点 `InteractPromptComponent`；`ATAParkourMarker` 自带 `PromptWidget`（挂在落点上，类型随 `MarkerType` 自动设置）。
- **纯蓝图 Actor**：实现 `TAInteractable` 接口，再 Add Component → `TAInteractionPromptAnchor`，把 `PromptType` 设为交互。
- **位置**：就是锚点组件的位置，在蓝图 / 关卡里拖动即可。默认值沿用此前实际显示的位置（可交互物 Z=100，跑酷落点 Z=−50）。
- **控件**：锚点的 Widget Class 留空时使用项目设置的 `DefaultPromptWidgetClass`（默认 `WBP_InteractPrompt`）；需要特殊样式时在锚点上单独指定。
- **文字**：锚点 `TextId` → 元素的 `InteractTextId`（`ATAInteractableActor`）→ 项目设置中该类型的默认 Key。复活点为 `Interact_Checkpoint`，剧情触发器为 `Interact_Talk`。

## 刷新

只在提示出现、语言切换、输入设备切换、改键位时刷新图标和文字（图标读本玩家的实际键位绑定）。同一个目标持续显示期间不重复查询。

## 提示控件父类

`WBP_InteractPrompt` 的父类已设为 `TAWorldPromptWidget`。控件名保持 `Image_Key`、`Text_Prompt`，可选加一个包住图标的 `SizeBox_Key`。新建的提示控件也建议用这个父类；父类不是它的旧控件仍可用，系统会按这几个控件名查找并填充。

## 与之前的差异

- 删除了 `ATAInteractableActor`、`ATAWorldItem`、`ATAParkourMarker` 各自的 `SetPromptVisible` / `RefreshPrompt` / `GetPromptWidgetComponent`（蓝图中无调用），改为 `GetPromptAnchor()`。
- 掉落物品的头顶提示组件已删除；物品蓝图里设置的提示 Widget Class 不再使用。
- 剧情触发器不再在构造函数里写死提示控件，改用项目设置的默认值；复活点因此也有了提示。
- 跑酷落点预览不再单独控制提示显隐。
- 不再按显示顺序把多个提示叠放（原叠放基于各自 Actor，实际并不对齐）；每个提示就在自己锚点的位置。
- 冻结时隐藏提示（原先提示组件停止 Tick，会停在冻结前的状态）。
- 繁体中文 `Parkour_Jump` 由“越过”改为“越過”。

## 验证

自动化测试：`TheAwakening.Prompt.InteractMatchesExecution`（最近目标、提示即执行目标、默认控件、冻结隐藏 / 恢复、拾取物品让出）、`TheAwakening.Prompt.Elements`（复活点文本、锚点覆盖、跨越 / 跳下分别最近、离开后切换）；`TheAwakening.Input.ParkourFacing` 改为检查提示资格。

PIE 待验证：

- 复活点、剧情触发器、跑酷标记的提示位置与之前一致（复活点首次出现提示）。
- 跨越和跳下同时可用时两个提示都出现。
- 按住扫描完全冻结、打开小游戏时提示消失，结束后恢复。
- 站在掉落物品旁：只有拾取列表，物品头顶无提示，其他交互提示让出。
- 切换语言、键鼠 / 手柄、改键位后提示刷新。
