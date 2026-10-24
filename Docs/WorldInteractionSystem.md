# 通用交互、世界剧情事件与特殊动作

实现日期：2026-10-23。设计依据是现有 C++ / Blueprint 架构及《The Awakening Game Overview (14)》中文版第 18、22、23 页。文档中的剧情文字是设计参考；本次没有制作整章演出、怪物 AI 或战斗资源。

## 1. 现有架构检查与整合决定

| 已有系统 | 检查结果 / 本次处理 |
|---|---|
| `ITAInteractable` | 已有 `CanInteract`、`GetInteractText`、`OnInteract`，继续作为对象能力接口，没有另造 Interface。 |
| `ATAInteractableActor` | 已有模型、统一提示锚点、文本 ID；增加可选配置组件，默认不接管旧交互行为。已有派生 BP 可继续工作。 |
| Character 交互 | `IA_Interact.Started → TryPlayerInteract → TryInteract`，先检查权限和冻结，再优先拾取列表；保留此顺序。 |
| `FTAInteractionQuery` | 已经统一执行与 Prompt 的最近目标规则（Visibility 球形查询、接口、CanInteract、排除 WorldItem）。增加配置组件条件过滤。 |
| `UTAPromptComponent` | 继续负责 Interact / ParkourJump / ParkourDrop 分组及胜出目标。增加同帧查询结果共享和特殊动作候选，没有新增检测组件或独立交互 UI。 |
| `UTAInteractionPromptAnchor` / 世界 Prompt WBP | 原布局、字体、图标资源、位置全部复用；拒绝提示及 QTE 用同一 Interact 分组。 |
| `UTAInputIconSubsystem` | 原来已经通过 LocalPlayer 的 Enhanced Input 查询实际 IA 映射，并监听设备、语言、ControlMappingsRebuilt；直接复用，不写死键盘图标。 |
| Enhanced Input / 输入仲裁 | 原 IA/IMC、`FTAInputRouter`、`TAInputOwnershipAdapter`、PC 对外部焦点的判定全部保留。扩展 `SpecialAction` / `Pause` 能力词汇；没有新增 PC 剧情分支。 |
| 扫描 | `UTAScanningComponent` 自己维护扫描候选、屏幕距离 Hover、信息窗口及冻结。扫描输入请求不允许普通 Interact。扫描信息 / 点击属于原扫描 / UI 入口，未改成普通交互或增加重复 Trace。 |
| 跑酷 | Marker + ParkourComponent 独立判定可行性，原 Prompt 分组和物理释放规则保持不变。特殊动作期间没有 Parkour 权限。 |
| 物品 / 日记本 / 文本 | WorldItem、NearbyPickup、Inventory 的剧情槽、WorldDocument、DocumentSubsystem 已实现拾取与正式收集。仅增加物品成功入包后授予剧情 Tags 的数据配置；没有重做文本收集。 |
| 剧情 | `UTADialogueSubsystem` 已是 GameInstanceSubsystem，具有旗标、可扩展条件求值器、事件总线、对话进度、完成记录及存档。直接扩展其世界状态与事件生命周期，没有第二个 StoryStateSubsystem。 |
| 动作 / PaperZD | `UTACharacterActionComponent` 已提供 Tag 动作请求、动画认领、关键帧、完成、打断、无动画回退与输入限制。特殊动作的 Animation 阶段复用它。 |
| 破解器 / 小游戏 | PathPuzzleSession / Widget、奖励和输入接收者已有自己的业务及权限。世界交互可请求 Director 打开已有终端 / 对话 / 小游戏；没有另造终端流程。 |
| 存档 / 回滚 | 扩展现有 `FTAStorySaveData`，继续由 `UTASaveSubsystem` 收集。新世界 Tags / 事件完成记录 / 分支结果支持现有回滚作用域。 |
| Level Blueprint | 检查源码与现有 Blueprint 资产入口，保留已有 StoryTrigger / Level 事件用法；本次没有批量迁移或修改关卡 Level Blueprint。新事件可逐步迁到 Director。 |

## 2. 新增类和配置

- `FTAInteractionConditions`：RequiredTags（全部满足）、BlockedTags（任一存在即阻止）、已有 `FTAStoryCondition` 数组（全部满足）。复用 Flag / HasItem / Money / Attribute / HasDocument / ReadDocument 求值器。
- `UTAInteractionBehaviorComponent`：挂在已有可交互 Actor 上，配置条件、拒绝尝试、事件 ID、可选特殊动作，以及 OnAllowed / OnDenied。
- `UTAWorldEventDefinition`：事件 ID、一次性、互斥组、前置条件、完成时增加 / 移除 Tags、旗标增量（例如 Morality）。
- `ATAWorldEventDirector`：关卡内执行者，配置 Events 资产列表，通过 ExecuteEvent / EventEnded / ApplyPersistentState Blueprint 事件连接具体演出。
- `UTASpecialActionDefinition` / `FTAQTEStage`：可选专用 IMC、映射优先级、对齐 Transform、最大距离、阶段数组、完成后请求的剧情事件 ID。
- `UTASpecialActionComponent`：玩家角色默认组件，负责对齐、阶段、QTE 输入、生命周期与清理；没有独立 Trace 或输入 UI。
- `UTAQTESequenceComponent` / `FTAQTESequenceStep`：交互物体上的简化阶段配置；StartQTE 使用 Details 数组，GenerateQTE 使用调用传入数组，MakeQTEStep 提供逐段有效输入和完成回调。继续使用玩家特殊动作执行器。
- `ATADebugQTETester` / `UTAQTEWidget`：可放置的重复测试物体和原 Prompt 锚点上的圆环、呼吸缩放、0.2 秒透视翻转展示；由 `TAQTESetup` 更新专用资产。
- `UTAInteractionSamplesCommandlet`：生成独立示例资产。重跑不覆盖已有示例资产；原日记本只补 `GrantedStoryTags`，不改其类型、剧情槽、图标或模型。

原 `ITAInteractable` 没有更改函数签名。默认 `bExecuteConfiguredBehavior=false`，空条件不会更改旧 Actor 行为。已有拾取 / 对话对象只需要条件时保留此值；设为 true 才明确由组件接管行为。配置了 SpecialAction 时先执行动作，动作定义的 CompletionEvent 决定成功后事件；同一组件的 EventId 不会同时提前派发。

## 3. 输入到行为的调用链

```text
Enhanced Input: IA_Interact / Started
  → Character.TryPlayerInteract
  → PC.AllowsInput(Interact) + Character.TryInteract 的冻结检查
  → NearbyPickup.TryPickupSelected（已有优先级）
  → UpdateInteractTarget
  → PromptComponent.ResolveInteractionTarget(refresh=true)
  → FTAInteractionQuery.FindBestInteractable
       → 接口 + 配置可选择性 + CanInteract
  → 执行前再次 CanInteract / 配置条件检查
  → 配置行为：Behavior.TryExecute
       ├─ 拒绝 → OnDenied（不执行目标行为）
       ├─ 普通配置 → OnAllowed
       ├─ EventId → 关卡 Director.TryStart
       └─ SpecialAction → 玩家 SpecialActionComponent.Start
  → 未接管的旧行为：ITAInteractable.OnInteract
```

Character 保留弱目标引用。每帧角色更新结果供 Prompt 复用；没有角色更新的工具 / 测试场景仍可直接查询。真正按下交互时强制刷新并复核，不执行上一帧失效或超距目标。缓存按帧、玩家位置、半径约束，已销毁目标不能复用。拾取列表仍有最高普通交互优先级。

`ATAInteractableActor` 的默认接口实现也会执行其已启用的配置行为，供直接脚本调用；自定义 Blueprint 覆写接口时应遵守同样条件或调用父实现。公开业务 API 不代替真实玩家的输入权限入口。

## 4. 剧情事件与存档

事件生命周期：`TryStart → Started → ExecuteEvent → Finish(Completed / Cancelled / Failed)`。

- Started 时只登记执行者、参与者并预留互斥组，不提交善恶值或最终分支。
- 同一事件运行时不可重入。一次性事件完成后不可再开始；取消 / 失败允许重试。
- 互斥组在运行时阻止另一分支并发；只有 Completed 才写入永久分支选择、完成 Tags、旗标增量。
- Finish 必须提供启动时相同的执行者，不能由另一个 Director 结束它。
- Director 的参与者死亡 / 销毁时取消事件；Director EndPlay 取消自己持有的事件。
- `OnWorldEventChanged` 提供带状态的生命周期通知；结束结果也带 Result 参数进入已有 `OnStoryEvent` 总线。既有 `BroadcastStoryEvent` 仍是普通通知，不会隐式启动有状态事件。
- `OnWorldStateChanged` 通知持久世界状态变化；Director BeginPlay 和状态恢复时调用 `ApplyPersistentState`。在这里根据 `GetWorldStateTags`、`HasCompletedWorldEvent`、`GetWorldBranch` 恢复机关、怪物和场景外观。
- 新状态存入原存档 Story 区块。活动中的 Sequence / QTE / Actor 引用不存档，读档先取消活动动作、关闭对话并取消活动事件，再恢复结果。跨关卡保留 GameInstance 持久结果，关卡执行者不跨关卡保留。
- 新字段为可接受的空默认值，旧档可直接加载。Inventory 恢复已持有物品时补回其 acquisition Tags，所以旧档已有日记本也能满足新门条件。
- `GrantedStoryTags` 表示“曾成功获得”，不是当前物品数量。当前仍需持有的条件请用 HasItem，不要用历史 Tag。
- 已有回滚作用域内的 Tags、分支完成状态和原旗标增量一起记录撤销；没有新增大型任务系统或 GAS 能力。

自定义低层 C++ / Blueprint 执行者直接使用 StartWorldEvent 时，必须配对 Finish / CancelWorldEventsFor。普通场景开发使用 Director，其 EndPlay 清理已封装。

## 5. 特殊动作、Enhanced Input 与图标

特殊动作请求输入优先级 75，高于普通玩法和表现动作（50），低于扫描（100）和界面。声明 SpecialAction、Look、Pause；普通移动、交互、跑酷、扫描、背包切换不获授权。暂停及其设置页通过 bSuspendSpecialAction 挂起并恢复原阶段、进度和时钟，暂停时移除自己的 QTE IMC，恢复时重建映射并忽略仍按住的键；其他不兼容输入抢占取消动作。每个回调必须通过 `AllowsInputFor(精确句柄, this, SpecialAction)` ；未缩放 QTE 不因实体冻结取消或拒绝输入，旧动作保留冻结检查。

- 输入全部绑定 `UEnhancedInputComponent::BindAction`，无新增 Slate / Widget 键盘游戏行为，无 Tick 读键，无 `IsInputKeyDown`。
- 普通交互继续使用原 IA_Interact。QTE 阶段引用 IA 和 ETriggerEvent。
- Press / Mash 默认 Started；Mash 优先使用 ProgressPerPress 增长，未配置时回退为 1/Required。DecayPerSecond 按动作时钟秒衰减（QTE 统一读取 TimeSubsystem 未缩放真实步长，暂停停止），ClickCooldownSeconds 限制有效点击间隔（默认 0.1 秒）；持住不会按帧累计。
- Hold 必须 Triggered，Required 表示连续触发秒数，用 EI 的 GetTriggeredTime 累积，Completed / Canceled 清零。使用持续 Triggered 的 IA；已有一次性 Hold Trigger 可以配置为 Press + Triggered，由该 Trigger 自己决定长按门槛，不应再套一层持续 Hold。
- 底层 Direction 类型支持 Axis2D + Triggered，通过方向点积和阈值判断，复用已有 IA_Move；简化组件的方向组使用已有四个 Boolean 方向 IA 构造 Press / Mash，各方向的字母、箭头与手柄方向键在 IMC 中互通。没有新建平行方向 IA。
- 切换阶段时仅查询一次 Enhanced Input 的 action value，已处于按下状态的动作必须先经过 Completed / Canceled 释放再接受；不靠物理键轮询推断释放。
- 专用 IMC 可选。组件仅添加 / 移除自己持有的 IMC；若同一个专用 IMC 已被别的系统启用，则拒绝开始，避免错误移除别人的上下文。已有 IA 已有有效映射时可以不配置 MappingContext。
- 添加和移除时使用 IgnoreAllPressedKeysUntilRelease；结束移除自己的绑定、IMC 和委托。可视化 QTE 成功后默认保留原精确权限句柄 0.2 秒再释放，并通过现有持续输入观察消费仍按住的 QTE 映射源至真实松开，避免 D 完成后穿透为移动。没有覆盖默认上下文、重置路由器或恢复历史输入快照。
- Cancel、目标销毁、死亡、复活、控制器切换、失焦、被抢占、超距、超时和 EndPlay 都清理。Tick 只检查对象生命周期、距离和超时，不检测键盘状态。
- 对齐使用目标相对 Transform，位置移动带碰撞 Sweep；若失败，恢复开始前 Transform，不启动 QTE。动作开始显式停止当前移动。
- Animation 阶段调用原 CharacterActionComponent。PaperZD 可通过已有 OnActionRequested / AcceptAction / NotifyActionKeyframe / NotifyActionFinished 接管。目标物体动画可监听 SpecialAction.OnUpdated 的阶段索引和角色关键帧；取消时在 OnEnded 停止目标动画。

Prompt 仍由 UTAPromptComponent 驱动。动作活动时同一 Interact 分组改用当前阶段 IA / 文本 ID / 原目标锚点。阶段 IA 或文本变化会刷新；设备、语言或映射重建沿用原 `OnInputPromptsChanged → Character.OnPromptRelatedSettingsChanged → RefreshVisiblePrompts`。最终图标仍由 `GetIconForActionForPlayer → QueryKeysMappedToAction` 获取，按设备筛选，沿用键鼠 / Xbox / PS 资源和回退。配置不引用图标纹理。

阶段进度由 `GetProgress`、`GetStageIndex`、`OnUpdated` 提供，简化组件提供只读 GetMashProgress 及进度通知。可视化 QTE 已使用原锚点的 UMG 展示绿色圆环、呼吸缩放和透视翻转；无需另建输入或检测系统。MakeQTEStep 的 OnAcceptedInput 在有效输入更新后回调，OnCompleted 在本段完成、推进下一段前回调；GenerateQTE 的 OnFinished 通知整次结果。键盘方向图标优先选择单字符绑定，继续允许箭头别名执行。详情及 Create Event 签名见 [QTEDebugTester.md](QTEDebugTester.md)。

## 6. Blueprint 配置

### 普通交互 / 条件门

1. 派生 ATAInteractableActor（或已有实现 ITAInteractable 的 Actor 并添加 Behavior 和 PromptAnchor）。模型碰撞需能被 Visibility 查询命中，锚点使用 Interact 类型。
2. 仅限条件控制旧行为时，保持 bExecuteConfiguredBehavior=false。新对象选择配置执行模式时设为 true，绑定 OnAllowed 实现开门等业务。
3. 家门 RequiredTags 加 `Story.Prologue.Diary`；保持 bAllowDeniedAttempt=true，DeniedPromptTextId 填本地化 ID，OnDenied 显示“先去妹妹房间”的提示。未拿日记仍然能尝试。
4. 日记本继续使用现有 `/Game/Document/BP_WorldItem_Diary` / DA_Item_Diary。后者已经配置 GrantedStoryTags。原 WorldDocument 会根据背包是否持有日记本允许正式收集，无需改写。

### 水池与一次性追逐

1. 放置 BP_PoolEntrance，将关卡 Director 的 Events 加入 DA_PoolEvent。
2. 派生 ATAWorldEventDirector，在 ExecuteEvent 的 Prologue.Pool 分支播放查看血迹演出、激活敌人并启动追逐。这个函数可以开始异步流程；不要立即 Finish。
3. 回到单元楼的原触发器 / Box Overlap 调用这个 Director 的 `Finish(Prologue.Pool, Completed)`。完成后记录 ChaseComplete Tag，一次性事件不再触发。
4. EventEnded 在 Cancelled / Failed 时关闭本次 AI / Sequence / 计时器；ApplyPersistentState 根据完成状态恢复医院场景变化。水池对象不持有怪物引用。

### 钢管与最终分支

1. 放置 BP_PipeEntrance。经 TAQTESetup 更新的 DA_PipeAction 保留四阶段：GrabPipe 动画、随机交互 IA 连按（每次 +15%、每秒 -5%、有效间隔 0.1 秒）、现有 IA_MoveRight 单次点按、DetachPipe 动画。也可在自己的交互 Actor 添加 QTESequence，使用 StartQTE 或 GenerateQTE 配置简化 Press / Mash 组合。
2. 更新后的示例使用 /Game/DeubgItem/QTE/IMC_QTE：交互候选为 E / 左键 / 右键 / F / Q 的独立 IA 与手柄 X / Y；固定右方向为 D / 右箭头 / 手柄方向键右。物理键只出现在 IMC 资产配置，运行时只接受当前阶段选中的 IA。
3. 根据场景调整 RelativeAlignment（示例目标局部 X=-80）、模型碰撞和距离。动画未配置时使用现有表现动作回退，可先验证输入。
4. Director.Events 加入 DA_PipeAcquiredEvent。在 `ExecuteEvent(Chapter1.PipeAcquired)` 中通过原 Inventory.TryAddItem 发放临时武器，检查实际加入数量，成功才 Finish(Completed)，失败 Finish(Failed) 并恢复目标表现。这里不提交攻击分支。
5. 另一个真正攻击 / 命中怪物的业务触发 DA_AttackEvent；吊机机关触发 DA_CraneEvent。二者共用 `Chapter1.MonsterOutcome` 互斥组；实际制伏完成后各自 Finish(Completed)，再提交 Morality 与最终结果。
6. 其他对象可用 PipeAcquired / Attacked / Crane 标签配置前置与阻止条件。想在完成后不再显示钢管提示，可设置 BlockedTags=PipeAcquired 且 bAllowDeniedAttempt=false。

示例 Door 的 OnAllowed / OnDenied、Director 的演出与奖励处理有意留给场景 Blueprint；这些属于具体场景行为，不是重复的检测、输入、图标或状态管理代码。事件 ID 在同一关卡应只由一个 Director 配置，避免重复职责。

## 7. 示例资产与验证

`Content/InteractionSamples/` 包含 3 个入口 Blueprint、4 个事件 Data Asset、钢管动作 Data Asset、1 个 Primary IA 和 1 个专用 IMC。最初的 Primary IA / IMC 保留为生成器配套资产；当前钢管经 TAQTESetup 更新为共享 QTE IMC，并引用现有 IA_MoveRight。首次生成可用 `UnrealEditor-Cmd The_Awakening.uproject -run=TAInteractionSamples -unattended -NullRHI` 生成；已经存在的示例不覆盖。随后运行 `-run=TAQTESetup` 更新 QTE 资产及钢管示例的输入阶段。两种生成器都保留，作为可维护的资源作者工具。

自动化新增 `TheAwakening.Interaction`：

- WorldEventLifecycle：前置、运行中互斥、非执行者拒绝结束、取消重试、一次性、完成时善恶值、保存 / 恢复、读档取消运行事件。
- ConditionsAcquisitionRollback：锁门仍可尝试、隐藏条件、成功拾取更新条件、失败拾取不授予状态、原剧情槽和世界状态共同回滚。
- SpecialActionEnhancedInput：真实 EI 输入栈驱动连按、持住不重复计数、旧键不能执行重绑定后的动作、键鼠 / 手柄实际映射图标、阶段顺序、权限抢占、目标销毁、动画回退、超时、失焦、长按释放重置、方向判断、死亡清理。
- SampleAssets：实际 BP 组件模板序列化、动作资产的 IA_Move 引用、原日记本新增配置、旧背包存档恢复 acquisition Tag。

下述初版记录及文末最新专项验证分别标明范围。无界面自动化不能证明手柄实机切换、动画观感或场景碰撞布置；正式接入关卡后需要 PIE 验证对齐、视角、交互文字、QTE 进度 UI、目标动画与角色动作同步。

## 8. 后续资源工作与边界

需要场景开发配置 Director 的具体执行图、门与追逐区域、敌人激活 / 结束逻辑、Sequence、音效、临时钢管武器定义及获得后的表现、GrabPipe / DetachPipe 动画，以及本地化提示和可选进度 UI。本次没有制作这些内容，也没有改写现有地图。

当前对齐是带碰撞的即时对齐，不含 Motion Warping 或路径寻路；位置不满足时拒绝，设计者需留出站位。Animation 阶段继续遵守已有组件“未认领则回退、超长则保底完成”的契约；不能把这一表现完成回调当作战斗命中证据。需要真正的命中 / 机关成功时，由业务系统明确调用 Director.Finish。

## 9. 初版验证记录（QTE 扩展前）

- UE 5.6 `The_AwakeningEditor Win64 Development` 编译与 UHT 通过。
- 示例生成命令成功，10 个独立资产已落盘；自动化实际加载并验证示例 Blueprint 组件模板、动作数据及原日记本配置。
- 完整 `TheAwakening` 自动化共 103 项：87 项无警告通过、13 项带警告通过、3 项失败、0 项未运行。报告：`Saved/Automation/InteractionRegression/index.json`。
- 新增 `TheAwakening.Interaction` 4/4 通过，包括真实 Enhanced Input 输入栈中的连按、长按、方向、重绑定、设备图标、动画回退、失焦、死亡与目标销毁清理。现有 Input / Prompt / 跑酷 / 背包 / 角色动作相关回归通过。
- 全量的三个失败已有仓库文档记录：Reader.TranslationPresentation 的水平居中断言（[Diary3DWrapUp.md](Diary3DWrapUp.md)）、Puzzle.Scope 的测试夹具映射初始化（[PathPuzzle.md](PathPuzzle.md)）、Settings.EnglishLayout 的旧夹具按钮绑定（[SettingsSystem.md](SettingsSystem.md)）。本次没有修改这些测试断言、布局资产或小游戏输入行为，也不宣称全量通过。
- 三项失败在独立进程中复跑仍然复现（报告 `Saved/Automation/InteractionIsolated/index.json`），并非新增交互测试先执行才出现。
- `git diff --check` 通过；新增运行时文件未发现 OnKeyDown / OnPreviewKeyDown / IsInputKeyDown / 硬编码 EKeys 输入处理。尚未进行交互式 PIE 或实机手柄 / 动画验收。

## QTE 可视化调试扩展

见 [QTEDebugTester.md](QTEDebugTester.md)。现已提供可放置的 TA_Debug_QTE_Tester、带衰减及可调点击冷却的连按、单次点按、随机 IA 池、圆环、呼吸缩放和透视翻转；暂停菜单现在挂起并恢复 QTE，活动 QTE 拒绝存档。钢管示例方向阶段已改为复用 IA_MoveRight，支持 D / 右箭头 / 手柄方向键右；最终剧情事件时机保持不变。

打包注意：原生 QTESequence 的默认模板为运行时软路径，`/Game/DeubgItem/QTE` 必须参与 Cook。
2026-10-24 已补入 `DirectoriesToAlwaysCook` 并验证模板、IMC 和 IA 实际进入 Windows 产物；缺失资源不再静默返回，详情及验证记录见上述调试文档。


## 10. 最新收尾状态

- 新建交互物体的父类、提示 ID、条件、两种 QTE 启动方式及 Blueprint 逐段事件连线见 [InteractableObjects.md](InteractableObjects.md)。QTESequence 挂交互物体，SpecialActionComponent 留在玩家。
- 最新编译 / UHT 通过；真实 RHI 离屏 Interaction / Input / Prompt **31/31 通过**（22 项无警告、9 项带既有警告），报告 `Saved/Automation/QTEBlueprintFunctions/index.json`。其中新增 K2 图实际编译 MakeQTEStep → MakeArray → GenerateQTE 及 CreateEvent。
- 覆盖逐段参数、输入过滤、冷却、回调取消、只读进度、暂停恢复、死亡、禁存档、设备输入与结束缓冲。渲染结果保留在 `Saved/QTE/`。
- 最近全量历史记录为 QTE 扩展阶段的 104 项：101 通过、3 个已知失败，见 `Saved/Automation/QTERegression/index.json`。新增 BlueprintFunctions 后未重跑全量，不能将专项 31/31 解释为全项目全绿。
- 本轮 wrap-up 仅同步文档并清理临时生成文件，没有重新执行编译或 PIE，也没有修改当前关卡、角色 Blueprint、存档或正式资产。正式动画、场景配置与手柄实机验收仍待进行。

本轮清理仅针对 Saved 中的交互 / QTE 一次性脚本、参考 PDF 截图、旧日志和 4 个中间报告目录，共删除 71 个文件（5.58 MiB）；保留最终专项报告、全量 / 独立失败历史报告、QTE 渲染帧和 TXT 操作手册。正式调试 Actor、生成器、测试、资产及场景配置保留。

后续 Q 键与统一未缩放时钟更新：QTESequence 自动使用 TimeSubsystem.GetUnpausedRealDeltaSeconds，Freeze/时缓不影响 QTE，Pause 保持；呼吸幅度 0.1。当前专项扩展到 Interaction / Input / Prompt / Time / Freeze，38/38 通过，见 `Saved/Automation/QTERealTime/index.json`。此前 wrap-up 与 31 项结果是历史记录。
