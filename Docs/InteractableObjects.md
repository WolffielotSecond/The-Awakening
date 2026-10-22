# 交互物体与 QTE：Blueprint 操作指南

适用于当前 `ATAInteractableActor`、`UTAInteractionBehaviorComponent` 和 `UTAQTESequenceComponent`。基础交互检测、输入与提示继续使用项目已有系统。架构见 [WorldInteractionSystem.md](WorldInteractionSystem.md)，QTE 参数与验证见 [QTEDebugTester.md](QTEDebugTester.md)。

## 1. 创建基础物体

1. 内容浏览器创建 Blueprint Class，在 All Classes 搜索 `TAInteractableActor` 作为父类。
2. 打开 Blueprint，在继承的 `MeshComponent` 中指定模型。碰撞需要参与 Query，并能被 `Visibility` 通道查询到；现有调试物体使用 Query Only、Visibility Block。
3. 选择 `InteractPromptComponent`，在视口移动提示锚点。默认相对位置 Z=100。普通物体保留 `PromptType=Interact`，Widget Class 可以留空，使用项目默认提示控件。
4. Class Defaults → Interaction 填写 `InteractTextId`；旧 `InteractText` 建议留空。
5. 在 Event Graph 添加继承的 `OnInteract` 事件，先连接 Print String 验证，再接正式开门或机关逻辑。
6. Compile、Save、放入关卡，用现有玩家走近并按当前交互键测试。

基类自带模型、提示锚点和 `InteractionBehavior`，没有自动开门、拾取或 QTE 行为。物体不需要 Enable Input、硬编码 E 键或自己做 Tick Trace。当前角色 C++ 默认交互距离为 200，Blueprint 可以覆盖；范围由玩家管理，不在每个交互 Actor 上配置。附近拾取列表优先接收普通交互。

`OnInteract(Interactor)` 中的 Interactor 是发起交互的玩家。需要玩家引用时使用这个参数；物体的 Self 不是玩家。

## 2. 提示文字和类型

正常文字的解析顺序：

`InteractPromptComponent.TextId → Actor.InteractTextId → 旧 InteractText → 项目默认 ID`

旧字段只在 Actor 的 ID 为空时回退使用。实际显示经 `TALocalizeSubsystem` 查询当前语言；ID 不存在时显示 ID 本身。

例如新建 `Interact_Pipe_Pull`，需要自己在 `Content/Localization/zh-CN.json`、`zh-TW.json`、`en.json` 等语言文件中补充同名条目，再把 ID 填入物体。`Interact_Default` 已有“交互 / 互動 / Interact”翻译。语言文件修改后重新进入 PIE 验证加载。

锚点的 TextId 会覆盖 Actor 的 ID。当前接口 `GetInteractText()` 优先返回旧字段，与统一 Prompt 的 ID 优先级不同，因此新内容统一填写 InteractTextId。单独覆写 GetInteractText 不一定改变当前统一提示。

`PromptType` 的 Interact / ParkourJump / ParkourDrop 是提示分类，不是门、钢管、阅读或 QTE 的玩法类型。普通物体保留 Interact。图标通过实际 Input Action 映射获取，不在物体上指定键盘图片。提示显隐交给玩家 `TAPromptComponent`。

## 3. 条件与事件

选择继承的 `InteractionBehavior`：

| 配置 | 作用 |
|---|---|
| Conditions.RequiredTags | 当前剧情状态必须包含所有指定 Tag |
| Conditions.BlockedTags | 存在任意指定 Tag 时拒绝 |
| Conditions.Conditions | 额外条件数组，复用 Flag / HasItem / Money / Attribute / HasDocument / ReadDocument |
| Allow Denied Attempt | 默认开启；不满足条件仍可选中并尝试。关闭则筛除目标 |
| Denied Prompt Text Id | 不满足条件时覆盖普通交互提示；不自动播放对话或语音 |
| Execute Configured Behavior | 默认关闭；开启后直接执行组件配置，跳过子类 OnInteract |
| Event Id | 请求关卡 Event Director 启动指定剧情 |
| Special Action | 启动玩家特殊动作；配置此项时优先于 Event Id |

`OnAllowed(Interactor)` 表示此次配置交互被接受，不能当作动作或剧情完成。`OnDenied(Interactor)` 可接拒绝反馈；条件失败或剧情事件启动失败会触发，特殊动作启动失败当前只返回失败。

组件事件可以在 BeginPlay 使用 Bind Event / Assign 绑定一次，不要每次交互重复绑定。仅需条件约束自己的 OnInteract 时，保持 Execute Configured Behavior 关闭；条件不满足仍会先走拒绝路径。若开启配置行为并只绑定 OnAllowed 开门，也可以不覆写 OnInteract。

例如日记本锁门：RequiredTags 选择 `Story.Prologue.Diary`，Allow Denied Attempt 开启，Denied Prompt Text Id 填对应拒绝文字 ID。未取得日记时走 OnDenied；取得后进入正常行为。

需要自定义可选中状态时覆写 `CanInteract`，将父类结果与自己的条件相与。例如 `Parent CanInteract AND NOT bOpened`。希望玩家能尝试被锁的门时，不要又在这里把条件不满足的情况全部排除。

## 4. QTE 组件挂在哪里

**在交互物体上添加 `TAQTESequenceComponent`，命名 QTESequence。** 基类没有自带它，现有 `TA_Debug_QTE_Tester` 已添加。

玩家已有 `TASpecialActionComponent` 负责执行与输入。无需给每个物体复制玩家执行器或新建 UI。保留物体原有提示锚点，使用项目已有的玩家角色测试。

组件基础设置：

- Action Template 初次保留 `/Game/DeubgItem/QTE/DA_Debug_QTE_Pipe`；提供 IMC、交互 IA 候选池、对齐、距离及可选 CompletionEvent。候选池来自模板首个 Press / Mash 阶段。
- Direction Actions 保留四项顺序：上 `IA_MoveForward`、下 `IA_MoveBackward`、左 `IA_MoveLeft`、右 `IA_MoveRight`。
- Key Flip Duration 默认 0.2 秒；Exit Input Buffer Seconds 默认 0.2 秒。
- 在自定义 OnInteract 中启动 QTE 时，InteractionBehavior 的 Execute Configured Behavior 保持关闭。

组件有两种启动方式，使用同一个执行器。

## 5. 方法 A：Details 配置 + Start QTE

1. 在 QTESequence 的 Steps 数组增删、排序阶段，数组长度决定步骤数。
2. 每项设置 Key Group、Direction、Press Mode 和进度参数。
3. `Event OnInteract → Start QTE`，Target 接 QTESequence，Interactor 接事件传入的玩家。
4. 预先绑定组件 `On QTE Ended(bSucceeded)`。结果为 true 才执行奖励；false 处理取消。

| 每段参数 | 可选值 / 默认值 |
|---|---|
| Key Group | Interaction：随机交互 IA；Direction：方向 IA |
| Direction | Up / Down / Left / Right / Random，仅对方向组生效 |
| Press Mode | Single Press：正确按一次；Mash：连续点击满进度 |
| Progress Per Click | 连按每次有效点击 +0.15，即 15% |
| Decay Per Second | 连按每秒 -0.05，即 5%；0 为不衰减 |
| Click Cooldown Seconds | 连按有效点击间隔至少 0.1 秒；0 为关闭限频 |

Single Press 固定一次完成，不使用连按增长和衰减。数值按 0～1 填写，不要把 15% 填成 15。简化组件生成的阶段没有自动超时；模板原 Stages 的 Timeout 不会传入重新构建的 Steps。

Start QTE 的返回值只表示启动是否成功，不能从 true 直接发放最终奖励。Details 中的步骤事件不可直接绑定，需要每段回调时使用下一种方法。

## 6. 方法 B：Generate QTE + 每段事件

Blueprint 数据连线：

```text
Make QTE Step（第 0 段） ──→ Make Array [0]
Make QTE Step（第 1 段） ──→ Make Array [1]
                                      └──→ Generate QTE.In Steps
Event OnInteract ──执行线───────────────→ Generate QTE
                └─Interactor──────────→ Generate QTE.Interactor
QTESequence 引用 ──────────────────────→ Generate QTE.Target
```

Make QTE Step 是纯数据节点，没有白色执行引脚；展开高级引脚可设增长、衰减和冷却。每个返回值按顺序组成阶段数组。Generate QTE 使用传入的 In Steps，独立复制运行，不修改 Details 的 Steps；模板和展示时长仍来自组件。

Make QTE Step 的两个委托引脚分别连接 Create Event：

| 引脚 | 用途 |
|---|---|
| On Accepted Input | 每次有效按键更新进度后回调；错误键、冷却拒绝、暂停和翻转阻塞不回调 |
| On Completed | 本段成功时一次回调，然后才进入下一段 |

两种回调签名均为 `StepIndex: Integer, Action: Input Action 对象引用, Progress: Float`，无返回值。Action 是实际选中的 IA，不是物理键，也不是 Input Action Value。

从委托引脚创建 Create Event，Object 使用包含处理逻辑的对象（通常 Self）。选择已有匹配事件，或使用编辑器的匹配函数创建选项。手工创建函数时，按上述顺序设置输入并 Compile，再在 Create Event 列表中选择。每段可以绑定不同函数，也可共用函数按 StepIndex 分流；不需要的引脚留空。

Generate QTE 的 `On Finished` 可选绑定另一个签名：`bSucceeded: Boolean`。最后一步 On Completed 之后才发送整次成功。运行中取消报告 false。组件 On QTE Ended 也会通知；不要同时在两个接收点发奖励。

回调中可以安全调用 Cancel QTE；取消会阻止后续阶段和最终成功。正常完成一段时无需手动 Cancel 或手动切换阶段。

## 7. 钢管配置例子

1. 新建钢管子类，设置模型、碰撞、提示 ID，添加 QTESequence。
2. 第 0 段：Interaction + Mash，增长 0.15、衰减 0.05、冷却 0.1；On Accepted Input 绑定拉扯反馈，On Completed 绑定姿态切换。
3. 第 1 段：Direction + Right + Single Press；On Completed 绑定钢管脱离表现。
4. OnInteract 调用 Generate QTE，传两项数组与玩家。
5. On Finished 判断成功后，经现有物品或武器系统发放武器、更新剧情状态；取消不发奖励。

默认交互 IA 候选对应 E / 左键 / 右键 / F，手柄 X / Y，每段只接受本次选中的 IA。右方向复用 D / 右箭头 / 手柄方向键右。随机在启动时决定，设备切换不会重抽。改变物理键或限制候选应配置 IA 池与 IMC，不在物体中新增物理键监听。

动画、武器与奖励逻辑需要自己连接；QTE 不自动生成这些内容。取得钢管、攻击怪物、完成分支应分别在正确时机更新状态。完成后只能用一次的物体需要持久剧情状态 + BlockedTags 等条件，普通 Blueprint bool 不自动存档。

## 8. 生命周期与排查

- Get Mash Progress 只读返回当前连按 0～1 进度，非连按或未运行返回 0；Get Current Step Index 未运行为 -1。On Mash Progress Changed 可接外部展示，当前实现也可能在单次阶段更新时报告 0。
- QTE 禁止普通移动、跑酷、背包、扫描和存档。ESC 使用现有暂停系统挂起，保留阶段、进度与时钟，恢复后继续。
- 死亡、目标销毁、失焦、超距及其他不兼容输入抢占会取消。成功后默认 0.2 秒缓冲，仍按住的 QTE 输入源需真正松开才重新参与持续移动。
- 整次成功回调时提示已经结束，但缓冲可能仍在；不要在该回调里立即要求开始另一套 QTE，可合并为同一个数组。
- 无提示：查 Visibility 查询碰撞、距离、锚点类型、条件、CanInteract、拾取优先级及输入权限。
- 文字不变：查锚点 TextId 覆盖、DeniedPromptTextId、实例覆盖；显示 ID 则查语言 JSON。
- 不进 OnInteract：查 Execute Configured Behavior 和拒绝条件。
- QTE 不启动：查实际调用、Interactor 是否为玩家、模板、空数组、执行器、死亡/冻结/其他动作/缓冲/权限及最大距离。
- Create Event 没有匹配函数：查 Object、编译状态和精确参数签名；阶段回调与最终回调不同。
- 奖励重复：查 On Completed / On Finished / On QTE Ended / 模板 CompletionEvent 是否重复执行，或重复绑定。

## 9. 快速测试与源码

现有调试物体：`/Game/DeubgItem/TA_Debug_QTE_Tester`，可拖入关卡；PIE 控制台 `TASpawnQTE` 可在玩家前生成测试物体。原目录拼写为 DeubgItem。

C++ 子类继承 `ATAInteractableActor`，重写 `OnInteract_Implementation(AActor*)`；需要自定义可选中状态时重写 `CanInteract_Implementation` 并结合父类结果。QTE 调用与 Blueprint 使用同一个 `UTAQTESequenceComponent`。

相关源码位于 `Source/The_Awakening/Interaction/`，显示位于 `UI/`，调试 Actor 位于 `Debug/`。当前编译及 31 项交互/输入/Prompt 自动化结果见 [QTEDebugTester.md](QTEDebugTester.md)。正式关卡的碰撞、动画同步和手柄实机效果仍需 PIE 验收。
