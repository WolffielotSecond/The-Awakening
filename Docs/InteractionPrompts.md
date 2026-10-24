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
| Upper | ParkourJump（统一执行 Action） | 与执行共享 `SelectDirectionalMarker`，显示获胜 Marker 的 Upper 锚点 |
| Lower | ParkourJump（统一执行 Action） | 同一获胜 Marker 的 Lower 锚点；双落点时两个提示始终同时显示 |

普通提示不显示的情况：角色完全冻结（扫描完全冻结、小游戏）、没有对应输入权限（菜单、对话、死亡等）。活动 QTE 的当前目标使用专门候选保留原锚点，由特殊动作权限与挂起状态控制。

跑酷按输入水平侧筛选、Marker 原点距离排序，距离完全相同时用对象路径稳定排序。没有水平输入时预览最近有效 Marker；执行仍必须有水平输入。双落点 W/S 不改变 Marker 排序，也不切换提示。旧的“提示最近、执行最后注册”差异已消除。

组合图标由现有锚点在 `SizeBox_Key` 内生成横排，保留原 WBP 的文字、图标高度和样式；普通交互继续使用单图标。键盘方向与执行键来自当前 IMC，手柄方向来自现有 Move Action。详细规则与迁移见 [DirectionalParkour.md](DirectionalParkour.md)。

## 给元素加提示

- **C++ 元素**：`ATAInteractableActor` 的子类（剧情触发器、复活点）自带锚点 `InteractPromptComponent`；`ATAParkourMarker` 自带 `PromptWidget`（挂在落点上，类型随 `MarkerType` 自动设置）。
- **纯蓝图 Actor**：实现 `TAInteractable` 接口，再 Add Component → `TAInteractionPromptAnchor`，把 `PromptType` 设为交互。
- **位置**：就是锚点组件的位置，在蓝图 / 关卡里拖动即可。默认值沿用此前实际显示的位置（可交互物 Z=100，跑酷落点 Z=−50）。
- **控件**：锚点的 Widget Class 留空时使用项目设置的 `DefaultPromptWidgetClass`（默认 `WBP_InteractPrompt`）；需要特殊样式时在锚点上单独指定。
- **文字**：锚点 `TextId` → 元素的 `InteractTextId`（`ATAInteractableActor`）→ 项目设置中该类型的默认 Key。复活点为 `Interact_Checkpoint`，剧情触发器为 `Interact_Talk`。

## 刷新

在提示出现、语言切换、输入设备切换、改键位及阶段 IA / 文本覆盖变化时刷新图标和文字（图标读本玩家的实际键位绑定）。同一个目标持续显示期间不重复查询。

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

## 配置交互与特殊动作扩展（2026-10-23）

见 [WorldInteractionSystem.md](WorldInteractionSystem.md)。世界剧情交互沿用上述 Interface、查询、分组与锚点；Character 更新的同帧目标由 Prompt 复用。特殊动作活动时，Interact 分组使用当前阶段的 Input Action 和本地化文本，继续查询现有输入图标子系统。阶段 IA / 文本变化会刷新，不新增一套交互 UI。


可视化 QTE 在这个锚点临时显示原生 UMG 圆环与翻转图标，结束后恢复原普通提示控件，不另加检测或输入 UI。操作配置见 [InteractableObjects.md](InteractableObjects.md)，逐段回调及验证见 [QTEDebugTester.md](QTEDebugTester.md)。
