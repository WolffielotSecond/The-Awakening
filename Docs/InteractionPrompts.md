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
| Upper | ParkourJump（Upper Action） | 与执行共享 `SelectParkourMarker`，显示获胜 Marker 的 Upper 锚点 |
| Lower | ParkourDrop（Lower Action） | 同一获胜 Marker 的 Lower 锚点；双落点分别通过朝向门槛后同时显示 |

普通提示不显示的情况：角色完全冻结（扫描完全冻结、小游戏）、没有对应输入权限（菜单、对话、死亡等）。活动 QTE 的当前目标使用专门候选保留原锚点，由特殊动作权限与挂起状态控制。

跑酷按 Marker 原点距离排序，距离完全相同时按对象路径稳定排序，不再按移动方向筛选。单落点只显示且只允许对应逻辑 Action：Upper Only 使用 Space/B，Lower Only 使用 Ctrl/A；双落点分别检查角色到各落点连线的水平朝向，严格在 ±80° 内才显示该提示，两边界和背向隐藏；两者均合格时同时显示。Upper 默认 Space/B，Lower 默认 Ctrl/A。两键同时按住的执行冲突不切换或隐藏提示。旧的“提示最近、执行最后注册”差异不再存在。

两个 Action 的图标均按本玩家当前 IMC 和输入设备解析。跑酷不再调用方向组合图标；通用 `ShowPromptChord` 能力保留，普通交互提示兼容。详细规则与旧数据迁移见 [DirectionalParkour.md](DirectionalParkour.md)。

## 给元素加提示

- **C++ 元素**：`ATAInteractableActor` 的子类（剧情触发器、复活点）自带锚点 `InteractPromptComponent`；`ATAParkourMarker` 自带两套落点 Prompt Anchor，按逻辑 Upper/Lower 设置类型；原组件与逻辑槽位的迁移关系见 DirectionalParkour.md。
- **纯蓝图 Actor**：实现 `TAInteractable` 接口，再 Add Component → `TAInteractionPromptAnchor`，把 `PromptType` 设为交互。
- **位置**：就是锚点组件的位置，在蓝图 / 关卡里拖动即可。默认值沿用此前实际显示的位置（可交互物 Z=100，跑酷落点 Z=−50）。
- **控件**：锚点的 Widget Class 留空时使用项目设置的 `DefaultPromptWidgetClass`（默认 `WBP_InteractPrompt`）；需要特殊样式时在锚点上单独指定。
- **文字**：锚点 `TextId` → 元素的 `InteractTextId`（`ATAInteractableActor`）→ 项目设置中该类型的默认 Key。复活点为 `Interact_Checkpoint`，剧情触发器为 `Interact_Talk`。

## 刷新

在提示出现、语言切换、输入设备切换、改键位及阶段 IA / 文本覆盖变化时刷新图标和文字（图标读本玩家的实际键位绑定）。同一个目标持续显示期间不重复查询。

## 提示控件父类

`WBP_InteractPrompt` 的父类已设为 `TAWorldPromptWidget`。控件名保持 `Image_Key`、`Text_Prompt`，可选加一个包住图标的 `SizeBox_Key`。新建的提示控件也建议用这个父类；父类不是它的旧控件仍可用，系统会按这几个控件名查找并填充。

## 验证

自动化测试：`TheAwakening.Prompt.InteractMatchesExecution`（最近目标、提示即执行目标、默认控件、冻结隐藏 / 恢复、拾取物品让出）、`TheAwakening.Prompt.Elements`（复活点文本、锚点覆盖、Upper/Lower 共用稳定 Marker winner、离开后切换）；`TheAwakening.Input.ParkourFacing` 检查严格 80° 门槛；`TheAwakening.Prompt.UpperLowerParkour` 检查逐落点显示、严格按键与实际终点一致性。

后续人工回归清单（本轮跑酷已由用户验收）：

- 复活点、剧情触发器、跑酷标记的提示位置与之前一致（复活点首次出现提示）。
- Upper/Lower 各自符合朝向资格时显示；背向/80° 边界隐藏，并与执行一致。
- 按住扫描完全冻结、打开小游戏时提示消失，结束后恢复。
- 站在掉落物品旁：只有拾取列表，物品头顶无提示，其他交互提示让出。
- 切换语言、键鼠 / 手柄、改键位后提示刷新。

## 配置交互与特殊动作扩展（2026-10-23）

见 [WorldInteractionSystem.md](WorldInteractionSystem.md)。世界剧情交互沿用上述 Interface、查询、分组与锚点；Character 更新的同帧目标由 Prompt 复用。特殊动作活动时，Interact 分组使用当前阶段的 Input Action 和本地化文本，继续查询现有输入图标子系统。阶段 IA / 文本变化会刷新，不新增一套交互 UI。


可视化 QTE 在这个锚点临时显示原生 UMG 圆环与翻转图标，结束后恢复原普通提示控件，不另加检测或输入 UI。操作配置见 [InteractableObjects.md](InteractableObjects.md)，逐段回调及验证见 [QTEDebugTester.md](QTEDebugTester.md)。
