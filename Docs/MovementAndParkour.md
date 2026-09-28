# 移动与持续跑酷输入

## 操作规则

- WASD 或左摇杆保持输入时，背包／对话／小游戏关闭后，无需松开重按即可继续移动。菜单内松开按键或摇杆回中后，关闭菜单不会继续走。
- 左摇杆超过 `StickDeadZone` 后方向归一化，幅度不影响速度。非疾跑使用 `WalkSpeed`（默认 500）；按住 Sprint 使用 `SprintSpeed`（默认 750）。取消了超过 0.5 自动疾跑。现有 Input Action 的 Dead Zone Modifier 也会先处理输入。
- `IMC_Default` 中 IA_Sprint 对应键盘 Left Shift 和手柄左摇杆按键 L3／LS（Gamepad_LeftThumbstick），均为按住疾跑，松开恢复走路。
- 跑酷：Space／手柄右侧面键持续请求跨台跳，Left Ctrl／手柄下方面键持续请求下落。提前按住，进入有效 Marker 后立即尝试；菜单、完全冻结或移动被禁止时不启动，恢复后仍按住则继续尝试。
- 同一个 Marker 成功执行一次后，持续按住不会原地循环。松开对应动作或离开后重新进入可再次执行；不同 Marker 可连续触发。一次跑酷期间暂时离开原框不会使落回同一框后立刻重复。
- 同时按住跳跃和下落时先尝试跳跃；没有可执行跳跃才尝试下落。仍保留现有 Marker 类型、落点、抛物线和边缘安全检测。

## 跑酷朝向与落点预览

跑酷组件的 `Parkour | Facing` 提供：

- `Allow Stationary Back Facing`（bAllowStationaryBackFacing）：默认开启，静止时允许背对落点启动；关闭后静止也要求面向允许的半圆。代码旁保留了中文设计备注。
- `Stationary Speed Threshold`：默认 5 cm/s，水平速度不超过此值按静止处理。使用实际水平速度，不是按键是否按住。

移动中，角色水平前向与“当前位置到落点”的水平向量点积必须 ≥ 0（含微小浮点容差），即以跑酷方向为中心的前方 180°，两条边界均包含。纯竖直路线不限制水平朝向。开始时固定本次落点和方向并立即转向，飞行期间保持该朝向；自动转向设置在结束后恢复，动画使用固定起跳方向，避免到达落点时翻面。

`CanParkourToMarker` 是蓝图可读的统一资格查询，包含触发框、有效 Marker、UI、冻结、移动禁用、正在跑酷、同框重复触发及朝向条件。执行和提示都使用它；落点 Widget 随资格显隐，LandingTargetComponent 及其 Child Actor 始终在游戏中隐藏，只负责定位，不向附着的 Widget 传播隐藏状态。触发框本身保持可用。Marker 在 PostPhysics 更新落点显示，角色先更新当帧移动朝向，再尝试持续跑酷。

## 实现位置

### 跑酷落地惯性

起跳前记录实际水平速度向量，跑酷结束后恢复；途中松键、松开 Sprint 或改变方向不会抹掉记录。落地后由 CharacterMovement 的制动、加速和转向摩擦接续当前输入：松键短暂滑步后停下，750 疾跑可减到 500 步行，反向输入先减速再反向。竖直速度不继承。

角色 Character Movement 组件的 `Parkour | Landing` 可调：`Landing Response Duration`（默认 0.6 秒）、`Landing Braking Deceleration`（2500）、`Landing Acceleration`（4000）、`Landing Friction`（2）。参数只在落地过渡期间生效，之后恢复普通移动处理。期间朝向跟随实际水平速度，接近静止时可转向输入方向；惯性方向经过已有边缘安全检测，不安全时停止水平滑动。菜单会取消过渡并停止，重新跑酷也会结束上一段过渡。扫描冻结沿用角色运动组件的冻结机制。

原生角色构造函数使用 `UTAMovementComponent` 替换默认移动组件；已有编辑器会话需重启，确认角色蓝图继承该组件。自动化测试额外验证实际蓝图的组件类型，以及松键制动、750→500、反向输入和过渡结束。

`The_AwakeningPlayerController` 的 Slate 输入预处理器在 UI 消费事件前记录按键和摇杆状态。菜单仍可以消费输入来操作虚拟鼠标，同时持续移动意图不会丢失。失焦会清理缓存；手柄断开会清理手柄输入。

控制器默认使用 `UTAPlayerInput`（EnhancedPlayerInput 子类），公开玩家已经解析的实际映射。`ReadHeldAction` 按映射及 Modifier 读取当前值，Modifier 使用独立实例，避免重复推进原 Enhanced Input 的内部状态。持有输入故意不等待 Pressed／Hold 等触发器；此逻辑仅用于移动、Sprint 和跑酷，交互、扫描、背包等仍用原事件绑定。

主角 Tick 中 `UpdateHeldGameplayInput` 重新读取当前意图，`UpdateMovementInput` 再判断能否移动。UI 期间清理角色输出不再丢失控制器保存的物理输入；对话恢复映射后也能读取仍按住的键。冻结组件停止角色 Tick 时，控制器的输入记录仍继续，解冻后重新读取。

`TAParkourComponent::UpdateHeldRequests` 处理持续请求和重复触发保护，`StartParkour` 统一检查 UI／冻结／禁用移动状态，直接从蓝图调用 TryParkourJump／TryParkourDrop 也不能绕过这些限制。

此实现面向当前本地键鼠／手柄玩法。UI 关闭时短暂没有视口焦点，只抑制动作、不清空按住缓存；松键事件仍更新缓存。真正切出应用时清理旧输入，回到应用后需要新的设备事件；以后更换 PlayerInput 类时应继承 TAPlayerInput。触屏或其他输入注入方式需要另行对接持续意图。

## 验证

自动化测试前缀 `TheAwakening.Input`：使用项目角色蓝图及 IMC_Default 验证菜单恢复、菜单内松键、映射移除／恢复、失焦清理、摇杆两档速度、L3；跑酷测试验证提前按住、同框防重复、松键／重新进入后恢复和冻结限制。朝向测试覆盖前半圆边界、移动中背向限制、静止例外开关、纯竖直路线、执行期间固定朝向、自动转向恢复及预览显隐。

实机再确认：摇杆持续偏转时用手柄打开／关闭背包；半推与满推的行走／疾跑速度；沿多个真实跑酷框持续按住；扫描冻结期间松键后解冻不会误触发。修改了控制器输入类及 IMC_Default，已有编辑器会话应重启后测试。
