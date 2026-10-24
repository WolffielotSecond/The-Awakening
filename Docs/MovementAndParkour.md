# 移动与持续跑酷输入

双落点 Marker 继续使用，当前输入为 Upper：Space／手柄 B，Lower：Left Ctrl／手柄 A。无移动方向要求，详细规则、迁移与验证见 [DirectionalParkour.md](DirectionalParkour.md)。

## 操作规则

- 背包开关键（Tab／Special Left）每次按住只切换一次，松开后才能再次切换；UI 拦截这两个键的重复事件。背包内 Xbox A 根据虚拟光标位置选择“背包／技能”页签，不再确认无关的默认焦点页签。

- WASD 或左摇杆保持输入时，背包／对话／小游戏关闭后，无需松开重按即可继续移动。菜单内松开按键或摇杆回中后，关闭菜单不会继续走。
- 左摇杆超过 `StickDeadZone` 后方向归一化，幅度不影响速度。非疾跑使用 `WalkSpeed`（默认 500）；疾跑状态使用 `SprintSpeed`（默认 750），状态由下述 Hold／Toggle 规则决定。取消了超过 0.5 自动疾跑。现有 Input Action 的 Dead Zone Modifier 也会先处理输入。
- `IMC_Default` 中 IA_Sprint 对应键盘 Left Shift 和手柄左摇杆按键 L3／LS（Gamepad_LeftThumbstick）。设置→游戏→常规的“点按切换疾跑”（`Game.ToggleSprint`）统一控制两种设备：默认关闭为 Hold，按住疾跑、松开恢复走路；开启为 Toggle，每次有效按下切换疾跑状态。
- 疾跑设置沿用草稿→应用：取消草稿不影响玩法。应用 Hold→Toggle 继承已接受的疾跑状态；Toggle→Hold 清除疾跑，并等待当前疾跑键真实松开后重新按住。即使菜单暂停世界，应用时也记录转换，不依赖角色 Tick。
- Toggle 疾跑松开有效移动输入后，在实际水平速度严格低于 `SprintSpeed` 时取消；等于或高于阈值时恢复移动保留疾跑。恢复输入的第一帧也检查此前松键期间的速度。摇杆回中或低于死区等同松开移动，竖直速度不参与判断。UI／冻结权限拒绝和跑酷本身不作为松键；失焦及显式停止移动会清除 Toggle 状态。菜单期间按下的疾跑键不能在关闭菜单后触发切换。
- 跑酷：IA_ParkourJump 默认 Space／手柄 B 请求 Upper；IA_ParkourDrop 默认 Ctrl／手柄 A 请求 Lower。单/双落点均严格绑定：Space/B 只能 Upper，Ctrl/A 只能 Lower，请求槽位未启用时拒绝，不回退。无需 WASD、LS 或鼠标方向，纯垂直路线可执行。提前按住进入有效 Marker 可触发。
- 同一次 Marker overlap 中成功执行后，持续按住不会 Tick 循环触发。参与该次消费的全部物理来源真正松开后可重新按下执行；真实退出后重新进入也是新机会，包括跑酷动作自身造成的退出。持续按住的 A→B→A→B 可以连续触发。重复 Register、Unknown、Lost 或映射遮蔽不能制造新机会。
- 两执行 Action 共用整个 Marker 的消费；所有配置均在两 Action 同时有效时拒绝；同一 Action 的键鼠/手柄消费来源必须全部真实松开才重新武装。提示与执行共用按 Marker 距离和对象路径排序的稳定候选，不按移动方向筛选。

## 跑酷朝向与落点预览

当前不要求移动方向、水平容差或两落点同侧，但起跳前必须满足该落点的严格朝向扇形：角色水平前向与起点到落点连线的水平投影夹角严格小于 80°，两边界排除；静止也不能背向起跳。蓝图保留的旧静止例外字段不能绕过新规则。每个落点的预览使用同一门槛，扇形外隐藏。起跳时按选中终点确定朝向；纯垂直路线保留原朝向；飞行期间保持固定朝向，结束后恢复原转向设置。Sprite 继续读取固定起跳方向。

`CanParkourToMarker` 检查真实重叠、配置、冻结、移动状态、正在跑酷和消费；玩家查询再检查输入所有权与 Parkour/Move capability。`SelectParkourMarker` 由执行和提示共用，按 Marker 原点距离及对象路径稳定排序。LandingTarget 只负责定位并在游戏中隐藏，两个逻辑落点提示独立显示。详见 [InteractionPrompts.md](InteractionPrompts.md)。

## 实现位置

### 跑酷落地惯性

起跳前记录实际水平速度向量，跑酷结束后恢复；途中松键、松开 Sprint 或改变方向不会抹掉记录。落地后由 CharacterMovement 的制动、加速和转向摩擦接续当前输入：松键短暂滑步后停下，750 疾跑可减到 500 步行，反向输入先减速再反向。竖直速度不继承。

角色 Character Movement 组件的 `Parkour | Landing` 可调：`Landing Response Duration`（默认 0.6 秒）、`Landing Braking Deceleration`（2500）、`Landing Acceleration`（4000）、`Landing Friction`（2）。参数只在落地过渡期间生效，之后恢复普通移动处理。期间朝向跟随实际水平速度，接近静止时可转向输入方向；惯性方向经过已有边缘安全检测，不安全时停止水平滑动。权限拒绝本身不会取消过渡或清零速度；对话成功开始时显式 Stop，重新跑酷会结束上一段过渡。扫描冻结沿用角色运动组件的冻结机制。

原生角色构造函数使用 `UTAMovementComponent` 替换默认移动组件；已有编辑器会话需重启，确认角色蓝图继承该组件。自动化测试额外验证实际蓝图的组件类型，以及松键制动、750→500、反向输入和过渡结束。

`The_AwakeningPlayerController` 的 Slate 输入预处理器在 UI 消费事件前记录按键和摇杆状态。菜单仍可以消费输入来操作虚拟鼠标，同时持续移动意图不会丢失。失焦会清理缓存；手柄断开会清理手柄输入。

控制器默认使用 `UTAPlayerInput`（EnhancedPlayerInput 子类），公开玩家已经解析的实际映射。`ReadHeldAction` 按映射及 Modifier 读取当前值，Modifier 使用独立实例，避免重复推进原 Enhanced Input 的内部状态。持有输入故意不等待 Pressed／Hold 等触发器；此逻辑用于移动、Sprint 和跑酷；其他玩家行为走已授权的 Gameplay 入口或当前 winner 的 owner receiver。

Toggle Sprint 额外记录具有 Move 权限时的物理按下次数，按当前映射消费；两次角色 Tick 之间已经松开的快速点按也能生效。持续按住和重复事件不增加次数；失去 Move 权限、失焦或设备断开会丢弃对应的待处理按下。模式切换会消费旧按下并抑制仍按住的来源，直到真实松开。

`bSprintHeld` 目前保存已接受的疾跑意图：Hold 模式来自按住值，Toggle 模式来自 `bSprintToggled`，不能再将它简单理解为物理按键状态。`SetToggleSprintEnabled` 在应用设置时处理模式转换；角色每帧读取已应用值以同步新角色，草稿不参与玩法判断。松开移动后的取消阈值使用可配置的 `SprintSpeed`，不写死为 750；跑酷执行阶段不做这项取消判断。

主角 Tick 中 `UpdateHeldGameplayInput` 重新读取当前意图，`UpdateMovementInput` 再判断能否移动。UI 权限拒绝不清物理 Held，也不隐式清理已接受的移动命令。有效 Freeze request 期间按已验收规则继续提交 accepted locomotion command，随角色时间缩放推进；

`TAParkourComponent::UpdatePlayerHeldRequests` 从 PC 读取持续请求，按本次 consumption 的物理 Key 身份证明同框 Release。玩家入口通过 Parkour capability 授权，业务资格与运动判定保留在跑酷组件；不通过 UI mode bool 推导权限。输入与生命周期契约见 [InputRouting.md](InputRouting.md)，跑酷业务规则见 [DirectionalParkour.md](DirectionalParkour.md)。

已接受的低优先级限制：Alt+Tab/F8 造成 definitive ownership loss 后，Held observation 失效；恢复 ownership 不重建物理输入。一直按着跑酷键返回时，可能需要真实 Release/Re-Press 才能继续。不使用重获焦点快照、合成 Held 或宽限时间补偿。

此实现面向当前本地键鼠／手柄玩法。UI 关闭时短暂没有视口焦点，只抑制动作、不清空按住缓存；松键事件仍更新缓存。真正切出应用时清理旧输入，回到应用后需要新的设备事件；以后更换 PlayerInput 类时应继承 TAPlayerInput。触屏或其他输入注入方式需要另行对接持续意图。

## 验证

`TheAwakening.Input.SprintModes` 验证默认 Hold、草稿与应用、菜单暂停期间的双向模式切换、旧按住抑制、L3 Toggle、750 阈值两侧与恢复第一帧、竖直速度排除、菜单内按下、失焦清理、取消草稿、恢复默认和帧间快速点按。

自动化测试前缀 `TheAwakening.Input`：使用项目角色蓝图及 IMC_Default 验证菜单恢复、菜单内松键、映射移除／恢复、失焦清理、摇杆归一化行走／疾跑速度、L3；跑酷测试验证提前按住、同框防重复、松键／重新进入后恢复和冻结限制。朝向测试覆盖静止/移动下的 ±80° 严格边界、扇形内外与背向拒绝、旧静止例外不能绕过、纯竖直路线可执行，并继续覆盖飞行固定朝向、自动转向恢复及预览显隐。

后续改动回归：摇杆持续偏转时用手柄打开／关闭背包；半推与满推的行走／疾跑速度；沿多个真实跑酷框持续按住；扫描冻结期间松键后解冻不会误触发。本轮移动与跑酷规则已由用户验收；变更原生类或默认 IMC 后，重启编辑器再回归。
