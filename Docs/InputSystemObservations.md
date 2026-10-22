# 输入系统调查：IMC、Slate 与改键位交接记录

调查日期：2026-10-23。用途：为后续负责改键位或统一输入路径的 agent 提供代码入口、已确认事实、风险和验证范围。

本次只有静态代码调查和文档整理，没有修改运行时代码、Blueprint、IMC、IA、配置或关卡，没有运行新的编译、测试或 PIE。文中“建议”是待评估工作，不代表已实现，也不是要求接手者立即重构全部输入。

## 1. 结论与范围

当前项目是混合输入架构：业务键位大量使用 IA / IMC，普通交互和 QTE 按键判定使用 Enhanced Input 事件；部分菜单通过 Slate 收键、查询当前 IMC 映射后直接执行命令；移动、跑酷等通过原始事件观察缓存计算持续动作值。不能把“使用 IMC 定义键位”与“由 Enhanced Input 完整求值后执行”视为同一件事。

调查检索覆盖 `Source/`：249 份运行时 C++ 头文件/源文件、40 份编辑器 C++ 文件、45 份测试 C++ 文件，另检查相关输入配置。数量是本次快照，后续新增文件会改变。第三方插件和引擎内部不属于本结论范围。

没有逐个打开 Blueprint 二进制资产检查事件图，因此不能保证所有现有 BP 都没有 OnKeyDown、物理按键事件或旧输入节点。尤其需要复查背包 WBP 和关卡脚本。源码中部分文件不是 UTF-8，复查时不要为搜索方便批量改写编码。

配置使用 Enhanced Input：`Config/DefaultInput.ini` 默认 PlayerInput / InputComponent 为 Enhanced Input 类型；玩家控制器进一步使用继承 `UEnhancedPlayerInput` 的 `UTAPlayerInput`。

## 2. 输入路径分类

| 路径 | 当前使用者 | 是否查询 IA/IMC | 是否经过 IA Trigger 事件求值 |
|---|---|---|---|
| UEnhancedInputComponent.BindAction | 普通交互、拾取切换、背包开关、扫描开始/结束、视角、暂停、QTE | 是 | 是 |
| Slate 收键后转交 PlayerController.InputKey，再进入 Enhanced Input | 日记本、文本阅读器的键盘/手柄命令 | 是 | 是，最终执行由 ExecutePlayerAction 接收 |
| Slate 收键后 ResolvePlayerInput / ExecutePlayerInput | 背包、物品菜单、数量框、暂停菜单、设置、对话/历史、公共确认框、小游戏部分命令 | 是，读取当前解析映射 | 不完整；主要按键与 IA 的映射关系直接派发 |
| ReadHeldAction + 原始观察缓存 | 移动、冲刺、跑酷、小游戏平移、物品检视旋转、设置页持续操作等 | 是，使用已解析映射与 Modifier | 不使用完整 Trigger 语义，表达持续按住意图 |
| Widget 指针事件 / UMG 按钮 | 点击、拖拽、滚轮、模型旋转、对话背景点击 | 部分查询 IA，部分使用指针语义 | 通常不是键盘 IA 触发链 |
| BindKey(G) | 非 Shipping 的打开测试小游戏入口 | 否 | 否 |

`TAItemInspectWidget` 的 ResolvePlayerInput 本身为空，但共享 Back 仍可通过控制器旧路由处理；不要仅凭存在这个接口就认定其全部功能都走直接键盘分发。

## 3. 已确认的 Slate 输入预处理器

主要文件：[The_AwakeningPlayerController.h](../Source/The_Awakening/The_AwakeningPlayerController.h)、[The_AwakeningPlayerController.cpp](../Source/The_Awakening/The_AwakeningPlayerController.cpp)。搜索 `FTAInputDeviceDetector`。

运行时发现一个自定义 `IInputProcessor` 实现：`FTAInputDeviceDetector`。名字虽然是设备检测器，职责实际包括：

- 键鼠/手柄切换通知与图标刷新。
- 观察键盘和鼠标按下/松开、手柄轴，写入 HeldKeyValues。
- 手柄死区、虚拟方向键和虚拟光标处理。
- 为增强 UI 路径转发按下/松开/模拟轴到 PlayerController.InputKey。
- 调用 RoutePlayerInputKey，直接派发旧 UI 接收者的逻辑命令。
- 每帧刷新输入所有权、虚拟光标、死区和部分设置计时。

它通过 FSlateApplication.RegisterInputPreProcessor 注册，在控制器 EndPlay 注销，同时清理应用激活和设备连接委托。大多数操作检查玩家输入所有权；释放观察允许在焦点/输入模式变化后清理状态。它是应用内 Slate 监听，不是发现了操作系统全局键盘钩子。

**接手时不能直接删除整个处理器。** 设备切换、界面中松开按键、失焦、手柄断开和虚拟光标依赖它。应先区分“底层观察/转发”与“直接执行游戏命令”，再逐项决定迁移。

## 4. 旧 UI 路由：已确认的语义缺口

核心入口：

```text
FTAInputDeviceDetector.HandleKeyDownEvent / HandleMouseButtonDownEvent
  → PlayerController.RoutePlayerInputKey
  → 当前输入所有者 ITAPlayerInputReceiver
  → IsKeyMappedToAction / ResolvePlayerInput
  → 精确句柄权限检查
  → ExecutePlayerInput
```

接口见 [TAPlayerInputReceiver.h](../Source/The_Awakening/Core/TAPlayerInputReceiver.h)。`UsesEnhancedUIActions()` 默认 false。

`IsKeyMappedToAction` 遍历 `UTAPlayerInput.GetHeldActionMappings()`，比较 Mapping.Action 与 Mapping.Key。它跟随 Enhanced Input 当前解析的映射，不是简单读取写死的 E 或 D；但该比较不表示 IA 的 Hold / Tap / Chord / Triggered 条件已经通过。

因此，这是一个已确认的输入语义缺口：若需求是所有命令都尊重 IA Trigger 配置，旧 UI 路径尚未统一。不能仅凭这些控件具有 InputAction 属性，就宣称已经完成 Enhanced Input 执行迁移。

潜在影响需要后续复现，不应当作本次已发现的具体游戏故障：

- 给菜单 IA 增加长按、轻点、组合键后，仍可能按旧收键时机执行。
- 同一物理键关联多个命令时，由接收者的判断顺序决定执行哪个命令。
- 业务重复、菜单关闭后输入穿透及按住重触发，可能受 Slate 消费与 EI 状态同时影响。

优先阅读这些实现：

- `UI/Inventory/TAInventoryPanelWidget.cpp`：ResolvePlayerInput、ExecutePlayerInput、ConsumeInventoryTogglePress。
- `UI/Inventory/TAItemActionMenuWidget.cpp`、`TAQuantityDialogWidget.cpp`。
- `UI/Pause/TAPauseMenuWidget.cpp`。
- `Settings/TASettingsMenuWidget.cpp`：SettingsCommands、连续调节和重置长按。
- `Story/TADialogueWidget.cpp`、`TADialogueHistoryWidget.cpp`。
- `UI/TACommonDialog.cpp`。
- `Puzzle/TAPathPuzzleWidget.cpp`：确认、撤销及平移。

## 5. 已迁移的 UI 路径可以作为参考

[TADiaryWidget.h](../Source/The_Awakening/UI/Document/TADiaryWidget.h) 与 [TADocumentReaderWidget.h](../Source/The_Awakening/UI/Document/TADocumentReaderWidget.h) 覆写 `UsesEnhancedUIActions()=true`。

控制器使用 `RegisterEnhancedUIAction` 绑定 IA 的 Started / Triggered / Completed / Canceled。Slate 入口通过 `ShouldForwardEnhancedUIKey` 判断是否转交 `InputKey`，最终由 `DispatchEnhancedUIAction → ExecutePlayerAction` 执行业务。旧 RoutePlayerInputKey 会避开这类接收者。

迁移其他菜单前，先理解这里的按下/松开配对、按键重复抑制、输入所有权和上下文生命周期；不要同时保留两条路径执行同一命令，也不要取消 UIOnly 等现有输入模式来绕过问题。

## 6. 持续输入缓存：已确认的架构边界

主要入口：控制器 `RecordHeldInput`、`ReadHeldAction`、`FindHeldKeyObservation`、`IsActionObservedReleased`；映射出口在 [TAPlayerInput.h](../Source/The_Awakening/Core/TAPlayerInput.h)。该类当前只有头文件，不存在必须查找的 TAPlayerInput.cpp。

```text
Slate 原始事件
  → RecordHeldInput：记录真实按下/松开
  → HeldKeyValues
  → ReadHeldAction：当前解析映射 + Modifier + 动作值累积
  → 消费者检查权限后更新移动或持续操作
```

ReadHeldAction 明确表达 held intent，不完整求值 IA Trigger。角色 `UpdateHeldGameplayInput` 每帧读取移动/冲刺；跑酷 `UpdatePlayerHeldRequests` 读取动作值，同时记录本次消费的真实输入源，等待这些源真正释放后重置。扫描 Tick 用 IsActionObservedReleased 修复界面中松开扫描键后不能重启的问题。

“没有 IsInputKeyDown”不等于“没有每帧读取按键状态”。当前没有每帧向操作系统查询按键，但有每帧读取事件缓存的实现。

后续改键位必须验证：

- 按住旧键时改绑，旧键不再驱动动作；真实释放仍能正确处理。
- 切换 IMC 不伪造 Released，不把暂时失去权限当作物理释放。
- 同一个 IA 的多个输入源与方向别名，释放其中一个不错误清空其他源。
- 按住进入菜单、菜单中松开、退出菜单后不残留移动/冲刺/扫描。
- 跑酷每个重叠机会的消费/重新武装规则保留，不因重绑或进出 UI 重复触发。

若未来改为完全由 EI 事件维护持续意图，需要明确保留上述契约，不能只把 ReadHeldAction 替换成一次 GetActionValue 就视为迁移完成。

## 7. QTE：当前输入合规情况与改键位待核查点

主要文件：[TASpecialActionComponent.cpp](../Source/The_Awakening/Interaction/TASpecialActionComponent.cpp)、[TAQTESequenceComponent.cpp](../Source/The_Awakening/Interaction/TAQTESequenceComponent.cpp)、[TAQTEWidget.cpp](../Source/The_Awakening/UI/TAQTEWidget.cpp)。

已确认：

- QTE 判定来自 UEnhancedInputComponent.BindAction；选择当前阶段 IA 后只接受相应事件。
- 冷却内点击、错误 IA、翻转阻塞和暂停时不计入有效输入。
- Tick 处理衰减、时钟、距离、生命周期和展示，不轮询物理键来增加进度。
- 进入阶段时一次性读取 EI GetActionValue，处理已按住输入的释放要求。
- QTE 结束消费当前仍按住的映射源，复用既有持续输入观察；这部分仍依赖底层原始事件缓存。
- GetPromptKeysForAction 查询 LocalPlayer 的实际有效 IA 映射；图标复用 Keys 资源。
- 暂停移除本动作上下文、恢复时重新加入，保留同一次随机选择及进度；只移除本动作拥有的上下文。

### QTE 专用 IMC 与改键服务的接入差异：需要验证

补充只读检查确认：控制器普通玩法、UI、设置和公共对话上下文会调用 `TAKeyBindingService.PrepareContext`；SpecialActionComponent 当前直接 AddMappingContext(Definition.MappingContext)，没有调用 PrepareContext。

因此，“QTE 查询实际映射显示图标”已实现，不代表“QTE 专用 IMC 必然已经被当前改键菜单枚举并持久化”。这是明确的接入差异；用户改键后 QTE 的具体表现，本次未运行验证，不能直接下结论说所有 QTE 重绑定都失败。

后续先确认产品语义：

- 普通交互键改绑是否应影响 QTE 的随机交互候选？当前它们可使用独立 IA。
- 改绑移动右方向时，QTE 右方向是否跟随？当前 QTE 使用自己的 IMC 映射，同一 IA 在不同 IMC 中可以有不同键。
- WASD 与箭头是两个别名槽，允许分别改绑还是只改主键？
- Xbox X/Y 为多个候选 IA 的共享映射；冲突规则不能简单照搬“全局所有重复键都不允许”。

若接入 PrepareContext，必须保存、暂停移除、恢复和结束释放同一个实际运行 IMC，并相应调整上下文冲突检查。不要添加运行副本却仍移除源资产，也不要修改共享 Data Asset。

## 8. 现有改键位服务：复用入口及待评估问题

文件：[TAKeyBindingService.cpp](../Source/The_Awakening/Settings/TAKeyBindingService.cpp)、[TAKeyBindingsMenuWidget.cpp](../Source/The_Awakening/Settings/TAKeyBindingsMenuWidget.cpp)。改键功能已经存在，应先复用和验证，避免另建并行注册表。

已确认机制：

- LocalPlayerSubsystem，按玩家复制运行 IMC，保留 IA、Modifier、Trigger，不直接改源资产。
- 向 EnhancedInputUserSettings 注册可改键映射。
- BindingId 组成包含设备、源 IMC 名、IA 名和原始默认键名。
- Rebind 检查设备、映射和冲突组，再调用 MapPlayerKey。
- RebuildAndSave 调用 ApplySettings / SaveSettings，并立即重建映射、忽略仍按住的键，发送 OnBindingsChanged。
- 跳过模拟轴、无效键和名字含 Debug 的动作映射。
- 捕获新按键使用原始键值，这是配置新映射需要的能力；取消捕获按当前逻辑动作映射判断。

待评估，不代表已复现故障：

- BindingId 含原始键名/IMC 名，未来重命名或修改默认绑定时旧配置如何迁移。
- GetKey / Rebind 选择映射行中的首个可用项或槽位，未来多槽、多硬件设备是否符合预期。
- 冲突组基于上下文名称和动作名称分类，专用 QTE/临时上下文以及允许复用键的规则是否准确。
- 上下文重建与按住源记录是否在跨关卡、菜单打开和暂停恢复时保持一致。

## 9. 其他 Slate / 硬编码命中如何分类

| 命中 | 实际用途 | 后续处理边界 |
|---|---|---|
| Character.SetupPlayerInputComponent 的 BindKey(EKeys::G) | 非 Shipping 调试打开小游戏，仍检查 OpenDebugUI 权限 | 唯一检索到的旧式直接 BindKey；若要求调试也统一，可单独迁移 |
| SettingsMenuWidget.NativeOnPreviewKeyDown | 返回 Handled，阻止 Slate 默认按键导航/激活 | 不是在这里执行 QTE 或菜单业务 |
| TACommonDialog.NativeOnPreviewKeyDown | 返回 Handled，阻止默认行为 | 不要误认成直接判定游戏按键 |
| InventoryPanelWidget.NativeOnPreviewKeyDown | 无权限拦截，有权限则调用 Super | 需要进一步检查对应 BP 的键盘事件图 |
| KeyBindingsMenuWidget.CapturePlayerInput | 捕获用户选择的新物理键 | 与执行普通玩法命令分开管理 |
| Widget MouseDown/Up/Drag/Wheel、OnClicked 等 | UI 指针命中、拖拽、滚动、模型旋转，对话背景点击可能查询 AdvanceAction 映射 | 需要区分指针行为与键盘命令；不能按 EKeys 字样一律删除 |
| InputIconSubsystem 的 EKeys 对照表 | 把键名转换为图标资源名 | 属于显示，不是硬编码输入触发 |
| 编辑器 TALocalizationEditor.OnKeyDown | Ctrl+S、撤销、复制等编辑器快捷键 | 不属于运行时玩家输入整改范围 |
| 测试里的 InputKey / 模拟 Slate 事件 | 驱动输入入口、检查是否穿透 | 不属于生产业务监听 |

运行时检索未发现 IsInputKeyDown、WasInputKeyJustPressed/Released、GetAsyncKeyState/GetKeyState、GetInputAnalogKeyState/GetInputVectorKeyState、ListenForInputAction、SetWindowsHookEx、RegisterHotKey 的调用。这里只是已检索源码范围的静态结果，不是对 Blueprint 或插件的保证。

## 10. 正常事件订阅不等于重复按键监听

项目使用 OnInputOwnerChanged、OnPlayerInputOwnershipLost、OnDeathStarted、OnRespawned、OnDestroyed、ControlMappingsRebuiltDelegate、OnInputPromptsChanged 等事件协调状态。

QTESequence 的 OnAcceptedInput / OnCompleted / OnFinished 是业务结果回调，不是重新监听物理键。已阅读的 QTE、图标和控制器路径存在结束/销毁解绑；本次没有穷尽检查所有委托是否泄漏，因此不能宣称全项目没有重复订阅问题。

## 11. 后续工作建议顺序

1. 先确认改键的范围：玩法、菜单、QTE、主/副键位、手柄设备、冲突规则和配置迁移。
2. 读取现有 InputRouting、SettingsSystem、InputIcons 文档及上述代码；先复用 TAKeyBindingService。
3. 运行/补充改绑与图标一致性的复现，确认 QTE 临时上下文是否进入配置注册。
4. 如需统一 Trigger 语义，按单个接收者迁移旧 UI 命令到 Enhanced UI Actions，确保旧链路不再执行同一命令。
5. 持续输入缓存单独评估；保留物理释放、重叠机会消费、焦点和权限契约。
6. 调试 G 键、Blueprint 事件图作为独立项目核查。

避免以“清理 Slate”为由直接移除设备检测、释放观察、指针命中和输入所有权适配。也不要为改键位在每个 Widget 新加物理键事件，或另造一套 Prompt 图标解析系统。

## 12. 回归范围与已有验证边界

优先使用现有测试：

- `TheAwakening.Input.UIPlayerIngress`、`InventoryRepeatRouting`：输入入口、重复与穿透。
- `TheAwakening.Input.HeldGameplay`、`HeldParkour`、`ParkourPhysicalRelease`：按住意图与真实释放。
- `TAHeldObservationLifecycleTests.cpp`、`TAScopedNavigationTests.cpp`、`TAVirtualCursorAxesTests.cpp`：观察生命周期、焦点/导航和虚拟光标。
- `TheAwakening.Settings.MenuInput`、`ResetHold`：菜单、改绑和持续操作。
- `TheAwakening.Document.Diary.EnhancedInput` 及日记本/阅读器测试：实际 IA 求值，阻止旧 Slate 键直接翻页。
- `TheAwakening.Interaction.SpecialActionEnhancedInput`、`QTE.DebugPipe`、`QTE.BlueprintFunctions` 与 Prompt 测试：QTE、回调、图标和清理。
- 小游戏 Scope、普通对话/公共对话、背包与物品操作测试。

改键后至少验证“执行键=显示键”、旧键不执行、重新加载保留设置、恢复默认、同 IA 多别名、临时 IMC 安装/移除、按住跨菜单/暂停/改绑、键鼠与手柄切换、失焦/断开/重连、死亡取消和 QTE 最终方向键不穿透移动。

之前真实 RHI 的 Interaction / Input / Prompt 专项为 31/31 通过，报告 `Saved/Automation/QTEBlueprintFunctions/index.json`；该历史结果不证明未来改键需求已全部覆盖。最近全量历史 `Saved/Automation/QTERegression/index.json` 有 3 个已知失败，详见架构文档，不应通过删断言掩盖。

本次调查未重跑测试，也未进行手柄实机或 Blueprint 全量图审计。

## 13. 相关文档

- [输入权限和路由历史](InputRouting.md)
- [设置与改键系统](SettingsSystem.md)
- [输入图标](InputIcons.md)
- [UI 输入映射](UIInputMapping.md)
- [QTE 参数、接口与回调](QTEDebugTester.md)
- [交互物体操作指南](InteractableObjects.md)
- [交互与世界剧情架构](WorldInteractionSystem.md)
