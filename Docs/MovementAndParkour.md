# 移动与持续跑酷输入

## 操作规则

- 背包开关键（Tab／Special Left）每次按住只切换一次，松开后才能再次切换；UI 拦截这两个键的重复事件。背包内 Xbox A 根据虚拟光标位置选择“背包／技能”页签，不再确认无关的默认焦点页签。

- WASD 或左摇杆保持输入时，背包／对话／小游戏关闭后，无需松开重按即可继续移动。菜单内松开按键或摇杆回中后，关闭菜单不会继续走。
- 左摇杆超过 `StickDeadZone` 后方向归一化，幅度不影响速度。非疾跑使用 `WalkSpeed`（默认 500）；按住 Sprint 使用 `SprintSpeed`（默认 750）。取消了超过 0.5 自动疾跑。现有 Input Action 的 Dead Zone Modifier 也会先处理输入。
- `IMC_Default` 中 IA_Sprint 对应键盘 Left Shift 和手柄左摇杆按键 L3／LS（Gamepad_LeftThumbstick），均为按住疾跑，松开恢复走路。
- 跑酷：Space／手柄右侧面键持续请求跨台跳，Left Ctrl／手柄下方面键持续请求下落。提前按住，进入有效 Marker 后立即尝试；菜单、完全冻结或移动被禁止时不启动，恢复后仍按住则继续尝试。
- 同一次 Marker overlap 中成功执行后，持续按住不会 Tick 循环触发。参与该次消费的全部物理来源真正松开后可重新按下执行；真实退出后重新进入也是新机会，包括跑酷动作自身造成的退出。持续按住的 A→B→A→B 可以连续触发。重复 Register、Unknown、Lost 或映射遮蔽不能制造新机会。
- 同时按住跳跃和下落时先尝试跳跃；没有可执行跳跃才尝试下落。仍保留现有 Marker 类型、落点、抛物线和边缘安全检测。

## 跑酷朝向与落点预览

跑酷组件的 `Parkour | Facing` 提供：

- `Allow Stationary Back Facing`（bAllowStationaryBackFacing）：默认开启，静止时允许背对落点启动；关闭后静止也要求面向允许的半圆。代码旁保留了中文设计备注。
- `Stationary Speed Threshold`：默认 5 cm/s，水平速度不超过此值按静止处理。使用实际水平速度，不是按键是否按住。

移动中，角色水平前向与“当前位置到落点”的水平向量点积必须 ≥ 0（含微小浮点容差），即以跑酷方向为中心的前方 180°，两条边界均包含。纯竖直路线不限制水平朝向。开始时固定本次落点和方向并立即转向，飞行期间保持该朝向；自动转向设置在结束后恢复，动画使用固定起跳方向，避免到达落点时翻面。

`CanParkourToMarker` 是蓝图可读的统一资格查询，包含触发框、有效 Marker、冻结、移动禁用、正在跑酷、同框重复触发及朝向条件；CanPlayerParkourToMarker 在其上检查玩家 Parkour capability。玩家执行和提示共用这一资格；落点 Widget 随资格显隐，LandingTargetComponent 及其 Child Actor 始终在游戏中隐藏，只负责定位，不向附着的 Widget 传播隐藏状态。触发框本身保持可用。Marker 在 PostPhysics 更新落点显示，角色先更新当帧移动朝向，再尝试持续跑酷。

## 实现位置

### 跑酷落地惯性

起跳前记录实际水平速度向量，跑酷结束后恢复；途中松键、松开 Sprint 或改变方向不会抹掉记录。落地后由 CharacterMovement 的制动、加速和转向摩擦接续当前输入：松键短暂滑步后停下，750 疾跑可减到 500 步行，反向输入先减速再反向。竖直速度不继承。

角色 Character Movement 组件的 `Parkour | Landing` 可调：`Landing Response Duration`（默认 0.6 秒）、`Landing Braking Deceleration`（2500）、`Landing Acceleration`（4000）、`Landing Friction`（2）。参数只在落地过渡期间生效，之后恢复普通移动处理。期间朝向跟随实际水平速度，接近静止时可转向输入方向；惯性方向经过已有边缘安全检测，不安全时停止水平滑动。权限拒绝本身不会取消过渡或清零速度；对话成功开始时显式 Stop，重新跑酷会结束上一段过渡。扫描冻结沿用角色运动组件的冻结机制。

原生角色构造函数使用 `UTAMovementComponent` 替换默认移动组件；已有编辑器会话需重启，确认角色蓝图继承该组件。自动化测试额外验证实际蓝图的组件类型，以及松键制动、750→500、反向输入和过渡结束。

`The_AwakeningPlayerController` 的 Slate 输入预处理器在 UI 消费事件前记录按键和摇杆状态。菜单仍可以消费输入来操作虚拟鼠标，同时持续移动意图不会丢失。失焦会清理缓存；手柄断开会清理手柄输入。

控制器默认使用 `UTAPlayerInput`（EnhancedPlayerInput 子类），公开玩家已经解析的实际映射。`ReadHeldAction` 按映射及 Modifier 读取当前值，Modifier 使用独立实例，避免重复推进原 Enhanced Input 的内部状态。持有输入故意不等待 Pressed／Hold 等触发器；此逻辑用于移动、Sprint 和跑酷；其他玩家行为走已授权的 Gameplay 入口或当前 winner 的 owner receiver。

主角 Tick 中 `UpdateHeldGameplayInput` 重新读取当前意图，`UpdateMovementInput` 再判断能否移动。UI 权限拒绝不清物理 Held，也不隐式清理已接受的移动命令。有效 Freeze request 期间按已验收规则继续提交 accepted locomotion command，随角色时间缩放推进；相关实现不在 Stage 4 改动。

`TAParkourComponent::UpdatePlayerHeldRequests` 从 PC 读取持续请求，按本次 consumption 的物理 Key 身份证明同框 Release。玩家入口通过 Parkour capability 授权，业务资格与运动判定保留在跑酷组件；不通过 UI mode bool 推导权限。最终输入与生命周期契约以 [InputRouting.md](InputRouting.md) 的 Stage four 章节为准。

已接受的低优先级限制：Alt+Tab/F8 造成 definitive ownership loss 后，Held observation 失效；恢复 ownership 不重建物理输入。一直按着跑酷键返回时，可能需要真实 Release/Re-Press 才能继续。不使用重获焦点快照、合成 Held 或宽限时间补偿。

此实现面向当前本地键鼠／手柄玩法。UI 关闭时短暂没有视口焦点，只抑制动作、不清空按住缓存；松键事件仍更新缓存。真正切出应用时清理旧输入，回到应用后需要新的设备事件；以后更换 PlayerInput 类时应继承 TAPlayerInput。触屏或其他输入注入方式需要另行对接持续意图。

## 验证

自动化测试前缀 `TheAwakening.Input`：使用项目角色蓝图及 IMC_Default 验证菜单恢复、菜单内松键、映射移除／恢复、失焦清理、摇杆归一化行走／疾跑速度、L3；跑酷测试验证提前按住、同框防重复、松键／重新进入后恢复和冻结限制。朝向测试覆盖前半圆边界、移动中背向限制、静止例外开关、纯竖直路线、执行期间固定朝向、自动转向恢复及预览显隐。

实机再确认：摇杆持续偏转时用手柄打开／关闭背包；半推与满推的行走／疾跑速度；沿多个真实跑酷框持续按住；扫描冻结期间松键后解冻不会误触发。修改了控制器输入类及 IMC_Default，已有编辑器会话应重启后测试。
