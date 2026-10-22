# QTE 调试物体与参数

从零制作普通交互物体、配置文字与条件，以及两种 QTE 接入步骤见 [InteractableObjects.md](InteractableObjects.md)；底层架构见 [WorldInteractionSystem.md](WorldInteractionSystem.md)。

## 直接使用

内容浏览器打开项目现有调试目录 `Content/DeubgItem/`，把 **TA_Debug_QTE_Tester** 拖入关卡。进入 PIE 后靠近并使用原交互键交互。也可以在 PIE 控制台执行 `TASpawnQTE`，在玩家前方生成一个测试物体。

每次交互启动一次独立 QTE。成功后物体标签显示 `QTE SUCCESS`，可再次交互重复测试；取消后可重试。`bLastSucceeded`、`SuccessfulRuns` 和 `OnTestEnded` 可在 Blueprint 中查看或绑定。

## 在 QTESequence 组件中配置组合

选中关卡中的 `TA_Debug_QTE_Tester`，在组件列表选中 **QTESequence**，展开 QTE 分类的 **Steps** 数组。数组长度就是阶段数量，支持增删、排序；依次完成全部阶段才成功。

每个元素可以独立配置：

| 变量 | 用法 |
|---|---|
| Key Group | Interaction：从交互 IA 池随机选一个；Direction：使用方向 IA |
| Direction | Up / Down / Left / Right / Random；只在方向组中生效 |
| Press Mode | Single Press：按一次；Mash：连续点按 |
| Progress Per Click | 该连按阶段每次有效点击增加量，默认 0.15 |
| Decay Per Second | 该连按阶段每秒衰减量，默认 0.05 |
| Click Cooldown Seconds | 该阶段两个有效点击之间的最短间隔，默认 0.1 秒 |

单次点按阶段正确按一次即完成，不使用连按增量、衰减。不同阶段可使用不同的三个参数。例如可以配置“上方向单次 → 左方向连按（每次 +0.2）→ 随机交互键单次”。默认仍是“随机交互键连按 → 固定右方向单次”。

组件的 **Key Flip Duration** 默认 **0.2 秒**；**Exit Input Buffer Seconds** 默认 **0.2 秒**。QTE 成功关闭提示后，缓冲期间继续保留原输入权限句柄，暂时禁止移动、跑酷和背包，结束后释放句柄。已用于 QTE 且仍按住的输入源，经现有 Input Ownership / 持续输入观察机制消费至真实松开；即使持续按住 D 超过缓冲，也不会自动转为向右移动。松开并重新按下即可正常移动。该机制按实际 IA 映射解析，不硬编码 D。

**Action Template** 提供现有 IMC、交互 IA 候选池（首个 Press / Mash 阶段）、展示参数、对齐设置及 Completion Event。**Direction Actions** 顺序为上、下、左、右，复用项目现有方向 IA。同一方向的 WASD 与箭头由 IMC 映射到同一 IA；随机只在每次启动时抽取一次。组合配置覆盖模板的 Stages，模板原始资产不会被改写。运行中更改配置在下次启动生效。

全部时间按游戏时间计算。被冷却拒绝的点击不增加进度，也不会重置上一次有效点击时间；暂停保持进度、冷却、阶段与动画时钟。

## Blueprint 事件和只读进度

- `Start QTE(Interactor)` / `Cancel QTE`：其他交互 Actor 也可以添加这个组件，在原 `OnInteract` 中调用，无需添加检测、按键绑定或 UI。
- `On QTE Ended(bool bSucceeded)`：在组件上绑定事件；全部阶段完成返回 true，取消 / 死亡等返回 false；一次运行只通知一次。事件在提示结束时触发，结束输入缓冲随后继续。
- `Get Mash Progress`：当前连按阶段的 0～1 进度，只读，没有外部设置接口；非连按阶段或未运行时为 0。
- `On Mash Progress Changed(StepIndex, Progress)`：输入或衰减更新时通知，可用于外部展示；切换到单次阶段报告 0。
- `Get Current Step Index`：从 0 开始；未运行返回 -1。`Is QTE Active` 查询运行状态。
- 原玩家组件 `OnEnded(Target, bSucceeded)`、调试物体 `OnTestEnded` 保留；剧情事件仍由原动作 CompletionEvent 提交 Director，不因选中交互或完成中间阶段提前触发。

## Blueprint 函数直接生成 QTE

Actor 添加 **QTESequence** 组件后，可直接调用 **Generate QTE**，传入 `Interactor`（玩家角色）、`In Steps`（阶段数组）和可选 `On Finished` 事件。数组长度就是步骤数，无需另外维护 Count；返回 bool 表示是否启动。函数使用独立运行副本，不修改组件 Details 中的 Steps。

Blueprint 连线：

1. 每个阶段放一个 **Make QTE Step** 节点。设置 `Key Group`、`Direction`、`Press Mode`。展开高级引脚可设置增长、衰减、有效点击冷却。
2. 把各个节点返回值按顺序连到 **Make Array**。
3. 把数组连到 **Generate QTE → In Steps**，Target 连 QTESequence，Interactor 连玩家角色。
4. 每个 **Make QTE Step** 的 `On Accepted Input` 和 `On Completed` 引脚使用 **Create Event** 绑定自己的 Blueprint 事件 / 函数。两者回调签名均为 `(StepIndex: Integer, Action: InputAction, Progress: Float)`。
5. **Generate QTE → On Finished** 可选绑定 `(bSucceeded: Boolean)` 事件。组件原 `On QTE Ended` 仍保留。

事件时机：

| 引脚 | 触发时机 |
|---|---|
| On Accepted Input | 当前阶段一次有效 EI 输入被接受、进度已更新后。连按每次有效点击都会触发；错误键、限频拒绝、翻转阻塞或暂停时不触发 |
| On Completed | 本段成功时只触发一次。连按满条 / 单次正确点按后触发，然后才切换下一段；取消不会把未完成阶段记为完成 |
| On Finished | 全部阶段成功或已运行 QTE 被取消时，通知整次结果；成功时发生在最后一段 On Completed 之后 |

例如钢管：第 0 段 `Interaction + Mash`，`On Accepted Input` 绑定拉扯反馈，`On Completed` 绑定准备扯下；第 1 段 `Direction + Right + Single Press`，`On Completed` 绑定钢管脱离；整次 `On Finished` 检查成功结果后发放武器。随机键在运行开始时选择，回调里的 Action 是实际选中的 IA。物理按键、手柄映射与图标仍由现有 IMC / Prompt 管理。

每次运行只保留自己的回调，结束或取消后清理；事件内部调用 Cancel QTE 可以安全中断后续阶段，不会继续发送整次成功。未绑定的事件可留空。Details 配置流程继续使用原 **Start QTE**。

## 默认钢管式流程

1. 从 E、鼠标左键、鼠标右键、F 的 IA 配置池随机选定一个。手柄映射使用 Xbox 布局 X / Y。整个连按阶段只接受选中的 IA；其他候选键不计数。切换设备只换同一 IA 对应图标，不重抽键、不重置进度。
2. 默认每次有效点击 +15%，每秒 -5%，有效点击间隔至少 0.1 秒。
3. 图标按 4 Hz 在 0.8～1.2 倍之间缩放；周围绿色圆环从六点钟开始顺时针增长。
4. 满条只切换到下一阶段，不判定成功。图标用 UI 材质绕竖轴做透视 3D 翻转（0.2 秒），正面旧键、背面新键；翻转期间不会接受尚未显示的下一阶段完成输入。
5. 默认下一阶段固定为右：D / 右箭头均可，手柄方向键右。按其他方向无效，正确点按一次才成功。

## 输入和显示资产

- Blueprint：`/Game/DeubgItem/TA_Debug_QTE_Tester`
- 默认动作：`/Game/DeubgItem/QTE/DA_Debug_QTE_Pipe`
- 两阶段都随机：在组件 Steps 的第二项设 Direction = Random。底层动作资产示例仍有 `/Game/DeubgItem/QTE/DA_Debug_QTE_Random`。
- 专用映射：`/Game/DeubgItem/QTE/IMC_QTE`
- 交互按键池：4 个独立 Boolean IA，分别表示一个候选输入，避免选中左键却让右键 / E / F 同时生效。
- 方向复用已有 IA_MoveForward / Backward / Left / Right，每个方向的 WASD 与箭头别名映射到相同 IA。固定右方向使用已有 IA_MoveRight，没有另外实现物理按键监听。
- 图标通过原 InputIconSubsystem 查询当前 IA 的实际映射，继续使用 `Textures/Keys` 资源；键盘优先显示单字符按键，保留箭头别名的执行能力。
- QTE UI 是原 PromptAnchor 上的 UMG 展示，仍由 PromptComponent 控制显示。普通交互、跑酷提示的控件类会在 QTE 结束时恢复。圆环与翻转材质位于 `/Game/UI/QTE/`，该目录已在项目现有 UI cook 根内。

运行时没有新增 Slate 键盘事件、Widget 按键处理、IsInputKeyDown 或物理键轮询。物理键仅用于 IMC 资产作者配置。UMG 只显示状态与动画；所有进度输入来自 Enhanced Input 回调。

## 生命周期

QTE 禁止普通移动、跑酷、背包和扫描。ESC / 手柄暂停沿用原暂停 IA：暂停页及其设置子页通过输入请求声明“暂时挂起特殊动作”，关闭后回到原动作句柄与阶段。暂停时暂时移除本组件的 QTE IMC，让暂停菜单的确认键和方向键正常生效；恢复时重新安装原 IMC、重建 Enhanced Input 映射并忽略仍按住的键，避免菜单操作变成游戏输入。

其他输入抢占、失焦、目标销毁、角色死亡、复活、超距或组件结束仍取消动作。角色死亡不会保留 QTE 或发出成功。所有 SaveGame / AutoSave 在活动特殊动作期间拒绝生成存档快照，包含暂停中的 QTE，已有存档不被覆盖。

## 资源生成和验证

`UnrealEditor-Cmd The_Awakening.uproject -run=TAQTESetup -unattended -NullRHI` 生成 / 更新本功能资产。该命令更新自己的 QTE 目录及上一轮 DA_PipeAction 的输入阶段，不修改已有按键图标或通用映射。

自动化 `TheAwakening.Interaction.QTE.DebugPipe` 覆盖真实 EI 按键、随机排他输入、15% 增长、5% 衰减、0.1 秒限频、自定义参数、副本隔离、暂停恢复、禁存档、键鼠与 Xbox 输入、方向别名及死亡清理。启用真实 RHI 时，额外导出 `Saved/QTE/` 中的圆环与翻转阶段图像，并检查画面实际产生像素。

## 本次验证结果

- UE 5.6 Editor Win64 Development / UHT 编译通过，资源更新命令成功（0 错误、0 警告）。
- 最新真实 RHI 离屏回归 Interaction / Input / Prompt 共 **31/31 通过**（22 项无警告、9 项带项目既有警告），报告 `Saved/Automation/QTEBlueprintFunctions/index.json`。其中 BlueprintFunctions 实际创建并编译 Make QTE Step → Make Array → Generate QTE 及 Create Event 的节点图。
- QTE 测试新增实际三阶段组合（方向单次 → 另一方向连按 → 交互单次）、逐段参数、0.2 秒翻转、可绑定成功 / 取消事件、组件只读进度，以及真实 IA 映射下 D 按住跨越缓冲不参与移动、松开重按恢复的验证。保留随机键、冷却、衰减、暂停、禁存档、Xbox、死亡清理覆盖。
- Generate QTE 运行验证包括数组副本与配置隔离、逐步有效点击 / 完成回调、冷却内不派发事件、整次结束回调、在有效点击或本段完成回调中取消后的安全清理。
- 重新输出真实渲染图像 `Saved/QTE/`，已查看新翻转帧。`git diff --check` 通过。
- 本轮未手动进行正式关卡 PIE 验收。上一轮全量回归中的三个已有失败见 `Saved/Automation/QTERegression/index.json`。
