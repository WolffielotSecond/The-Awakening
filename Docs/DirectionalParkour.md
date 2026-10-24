# 方向式双落点跑酷（2026-10-24）

本次范围为输入、Marker、候选选择与提示。运动继续使用起跳时固定 Start/End、原有 Flying、固定时长 Alpha、Lerp + `4 * ArcHeight * Alpha * (1 - Alpha)`、无 Sweep 的 SetActorLocation。Finish、Abort、落地惯性、扫描冻结、边缘检测、动画和镜头未重写。

## 输入与目标

`IA_ParkourJump` 是统一执行 Action，默认 Space / Xbox A。`IA_ParkourDrop` 资源和旧反射属性保留供兼容，不再映射或参与 Held 执行。Ctrl 不再执行跑酷，未添加潜行。

左右由世界 Y（当前固定水平游戏轴）上的玩家到动态落点差值决定，与 Marker 的局部深度轴独立；默认 5 cm 内拒绝，双落点跨左右两侧也拒绝。无需关卡作者配置 Left/Right。

键盘从已有四个移动 Action 读取，实际键位跟随 IMC：左右必须有且只能有一个；单落点忽略上下，双落点无上下默认 Upper、上为 Upper、下为 Lower；双落点上/下冲突拒绝。键盘移动 Action 有输入时优先于摇杆，与普通移动一致。

摇杆复用现有 Move Action、Modifier 和物理观测；额外的跑酷幅度死区默认 0.2、每轴方向阈值默认 0.35，可在 ParkourComponent 调整。水平分量达阈值才能执行，垂直分量低于阈值为默认 Upper，不要求精确 45°。已有控制器过滤与 Action Modifier 在此之前生效。

无输入缓冲，动作判断时满足组合就启动，支持任意按键顺序、提前按住进入和继续按住进入下一个 Marker。输入所有权、Parkour / Move capability、冻结、业务资格和实际 TriggerBox overlap 均须有效。

提示与执行共用 `SelectDirectionalMarker`：有效配置 → 注册且真实重叠 → 未消费 → 权限/冻结/原朝向资格 → 输入水平侧 → Marker 原点距离 → 对象路径打破完全相等距离。无水平意图只允许预览最近有效 Marker。获胜 Marker 再选择唯一落点或 Upper/Lower；双落点上下冲突不会偷偷改选其他 Marker。

## Marker 编辑与旧数据

仍是一个 ATAParkourMarker、一个 TriggerBox、两个独立原生 Spline / ChildActor 落点 / Prompt 锚点。`Enable Upper`、`Enable Lower` 控制逻辑落点；`Upper Arc Height / Jump Duration`、`Lower Arc Height / Jump Duration` 独立可编辑并支持蓝图覆盖。

**原组件名必须保留。** 为避免丢失原生组件上的蓝图及关卡覆写，未删除、重命名或复制旧 Spline 控制点。组件树仍显示 `LandingSpline`、`LandingTargetComponent`、`PromptWidget`，新增第二套命名为 `AdditionalLandingSpline`、`AdditionalLandingTargetComponent`、`AdditionalPromptWidget`。

| Marker 来源 | 原组件对应逻辑落点 | Additional 对应落点 |
|---|---|---|
| 新 Marker / 旧 JumpToPoint | Upper | Lower |
| 旧 DropDown | Lower | Upper |

Details → Parkour → Legacy 的只读 `Legacy Spline Is Lower` 明确显示原组件属于哪个落点。蓝图可通过 `GetLandingSpline(bLower)`、`GetLandingTargetComponent(bLower)`、`GetLandingPrompt(bLower)`、`GetLandingLocationForSlot` 访问逻辑 Upper/Lower，无需按原组件名推断。

旧 Marker 在 PostLoad 按逐对象自定义版本迁移，旧 DropDown 的实例覆写优先于蓝图默认值。旧 ArcHeight / JumpDuration 自动复制到原落点所属的新参数；旧数值保留在 Legacy 下只读。已有曲线、组件变换、Prompt Widget Class、TriggerBox 和旧 LandingTarget 引用原样保留。无需重做旧点。验证后正常保存蓝图/关卡即可持久化新版本；工具不批量重存关卡。

开启另一个落点后，请在对应 Additional 或原组件上编辑曲线与锚点，并设置该逻辑落点的独立轨迹参数。两个落点必须在 TriggerBox 有效深度范围内始终属于同一水平侧。

禁用只改 Visibility / bDrawDebug，不删除控制点，不更新该 Target、不参与资格和提示。UE 原生 Spline visualizer 在直接编辑或选择组件时可能仍画控制点；未添加自定义 Spline 类或破坏曲线来强行隐藏。重新启用保留原曲线。

`bUseXAsDepthAxis`、Marker 局部坐标与 20 次二分搜索保留；Spline 相对变换先转换回 Marker 空间后比较深度，两个落点独立计算。编辑器 Map Check 报告双禁用、无效曲线、默认玩家位置下跨侧和采样非单调深度；实际每次执行还根据玩家深度拒绝跨侧。Map Check 的采样不能证明整个曲线绝对单调，仍需关卡验证。

## 提示与消费

单落点只显示左右 + 执行；双落点两个锚点始终同时显示上/下 + 左右 + 执行，不随 W/S 选择隐藏或高亮另一个。沿用 UTAPromptComponent 类型槽位 ParkourJump / ParkourDrop 作为 Upper / Lower 显示槽位，两者都使用统一执行 Action。

键盘组合图标通过当前 Action 映射解析；手柄显示现有 Move Action 对应的摇杆与方向，以及当前执行键。Xbox / PS 斜方向图标现在为复制的 LS 占位纹理，附明确方向箭头。正式图标 TODO 见 [InputIcons.md](InputIcons.md)。现有图标目录已列入 AlwaysCook。

消费仍属于整个 Marker：任意落点执行一次后阻止同框重复。只记录执行 Action 本次参与消费的物理来源，方向键变化/松开不重新武装。全部消费来源真实松开才清除；Unknown、Lost、映射遮蔽和菜单权限变化都不能当松开。真实离开 TriggerBox 清除该 Marker 消费，支持持续按住 A→B→A→B；一个碰撞组件离开但其他组件还重叠不算退出。Alt+Tab/F8 后继续按住可能仍需真实松开重按，沿用原有安全原则。

## 工具、验证与人工检查

2026-10-24 最终结果：UE 5.6.1 `The_AwakeningEditor Win64 Development` 编译通过；`TheAwakening.Input + TheAwakening.Prompt + TheAwakening.Freeze` 联合回归 **31/31 通过，0 失败、0 未运行**（21 项无警告，10 项带测试世界/无视口等 headless 环境警告）。`git diff --check` 通过。报告：`Saved/Automation/ParkourFinal/index.json`；日志：`Saved/Logs/ParkourFinalTests.log`。

`BP_ParkourMarker` 编译成功。World Partition 审计实际加载 `Lvl_ThirdPerson` 的 9 个旧实例：7 Upper、2 Lower；原组件名、各 2 个曲线控制点、75/120 的原弧高和 0.5 秒时长均保留，迁移后的逻辑落点参数与原值一致。其他两个关卡未发现 Marker。审计报告记录了每个实例的原控制点坐标：`Saved/ParkourMigrationAudit.txt`，工具退出为 0、0 错误/0 警告。审计没有重存蓝图、关卡或外部 Actor。

`-run=TAParkourSetup` 只更新默认跑酷映射、创建缺失占位图标并审计蓝图/关卡；重复运行不覆盖已有占位纹理。加 `AuditOnly` 仅审计。报告写到 `Saved/ParkourMigrationAudit.txt`；不重存蓝图、关卡或已有 Marker。

新增自动化：`TheAwakening.Input.DirectionalParkour`（左右 × 三配置 × 上/中/下 × 两设备、独立参数与原公式、冲突、阈值、任意顺序、方向松开不重武装、IMC 改键、默认资源），`DirectionalParkourMarker`（双禁用、禁用重启保留控制点、不更新禁用锚点、跨侧拒绝、旋转深度），`TheAwakening.Prompt.DirectionalParkour`（双提示、独立位置、相同 winner、旧 WBP 组合容器）。原 Held、PhysicalRelease、Facing、Momentum、DialogueParticipation 回归适配统一输入并保留原生命周期断言。

实机/编辑器仍需验证：

1. 重启 UE，检查旧 Jump/Drop 蓝图与真实实例的原曲线、轨迹参数、Prompt 类和组件变换；关闭再打开项目，确认保存后的配置持久化。
2. 放置左右 Upper Only、Lower Only、Dual，在不同局部深度及 Marker 旋转下进入；双落点两个提示的位置、遮挡和屏幕排版。
3. 用真实键盘和手柄操作全部方向，测试死区/阈值手感、设备切换、IMC 改键后组合图标与两个 Prompt。
4. 提前按住、连续进入多个框；菜单、冻结/解冻、死亡中止、Alt+Tab、F8 与重新拥有输入；验证没有同框重复。
5. 跑酷期间保持旧轨迹/朝向，落地惯性、普通移动、扫描时间缩放、边缘安全。

无 Sweep 位移可能穿过途中障碍物，是原机制的已知限制；未在本次改碰撞策略。未进行真实设备、渲染效果或完整打包验证。

## 修改文件清单

运行时：

- `Source/The_Awakening/Movement/TAParkourMarker.h`、`.cpp`、`TAParkourMarkerPreview.cpp`
- `Source/The_Awakening/Movement/TAParkourComponent.h`、`.cpp`
- `Source/The_Awakening/The_AwakeningCharacter.h`、`.cpp`
- `Source/The_Awakening/Core/TAInputIconSubsystem.h`、`.cpp`
- `Source/The_Awakening/UI/TAInteractionPromptAnchor.h`、`.cpp`
- `Source/The_Awakening/UI/TAPromptComponent.h`、`.cpp`、`TAPromptTypes.h`

测试与作者工具：

- `Source/The_Awakening/Tests/TADirectionalParkourTests.cpp`
- `Source/The_Awakening/Tests/TAParkourInputTestFixture.h`
- `Source/The_Awakening/Tests/TAParkourPhysicalReleaseTests.cpp`
- `Source/The_Awakening/Tests/TAHeldGameplayTests.cpp`
- `Source/The_Awakening/Tests/TAPromptTests.cpp`
- `Source/The_Awakening/Tests/TADialogueParticipationTests.cpp`
- `Source/The_AwakeningEditor/TAParkourSetupCommandlet.h`、`.cpp`

资源与文档：

- `Content/Input/IMC_Default.uasset`
- `Content/Textures/Keys/Xbox/LS_UpLeft.uasset`、`LS_DownLeft.uasset`、`LS_UpRight.uasset`、`LS_DownRight.uasset`
- `Content/Textures/Keys/PS/LS_UpLeft.uasset`、`LS_DownLeft.uasset`、`LS_UpRight.uasset`、`LS_DownRight.uasset`
- `Content/Localization/en.json`、`zh-CN.json`、`zh-TW.json`（统一执行键在设置里名为 Parkour／跑酷）
- `Docs/DirectionalParkour.md`、`MovementAndParkour.md`、`InteractionPrompts.md`、`InputIcons.md`、`InputRouting.md`

旧 Marker 蓝图和 World Partition 外部 Actor 资源没有重存。PlayerController、TAPlayerInput、TAMovementComponent 沿用已有实现，无源码改动。
