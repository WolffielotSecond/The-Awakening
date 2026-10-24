# 双执行键双落点跑酷

状态：2026-10-25 用户实机验收通过。本文记录最终规则；文件名沿用既有链接。

## 输入与落点

| 逻辑槽位 | Input Action | 键盘默认 | Xbox 默认 |
|---|---|---|---|
| Upper | IA_ParkourJump | Space | B（右侧面键） |
| Lower | IA_ParkourDrop | Left Ctrl | A（下方面键） |

输入与提示均通过现有 Enhanced Input / IMC 解析，允许改键和设备切换。不要求 WASD、左摇杆方向或鼠标指向，W/S 不选择落点。Upper/Lower 是作者指定的逻辑分类，不按 Z 高度自动排序。

| 配置 | Upper 请求 | Lower 请求 |
|---|---|---|
| Upper Only | Upper | 拒绝 |
| Lower Only | 拒绝 | Lower |
| Dual | Upper | Lower |
| 全部关闭 | 拒绝 | 拒绝 |

所有配置均拒绝同时按住两个 Action。请求槽位未启用或朝向不合格时拒绝，不替换为唯一落点，也不改选另一个 Marker。无组合输入缓冲，支持提前按住进入有效 TriggerBox。

## 资格、候选与朝向

`SelectParkourMarker()` 由执行和 Prompt 共用。候选需配置有效、已注册且 TriggerBox 真实重叠、未消费、角色/移动状态允许、未完全冻结，并具有输入所有权和 Parkour/Move Capability。至少一个落点通过朝向资格，Marker 才参与排序。

按玩家到 Marker 原点的三维距离平方选择最近候选；距离完全相等时按对象路径稳定排序。两个 Action 使用同一 Marker winner，再检查各自逻辑落点，不按按键、移动方向或注册顺序选择不同目标。

`CanParkourToLanding` 是执行与预览共用的逐落点资格入口。以起点到动态落点连线的水平投影为中心，角色水平前向夹角必须严格小于 80°：总开口 160°，两边界排除，正对中心允许。静止与移动使用同一规则，没有背向起跳例外。纯垂直路线没有水平朝向，沿用可执行及保持原朝向的规则。

双落点不要求同侧；各落点分别通过朝向门槛。请求被拒绝不消费 Marker，持续按住时转入合格扇形可启动。飞行开始后朝向固定，不随途中输入翻转。

角色 BeginPlay 通过 `ConfigureExecutionActions` 配置两个 Action，Tick 调用 `UpdatePlayerHeldRequests`。`TryParkourJump` / `TryParkourDrop` 分别提交各自已物理按住的请求，仍走相同权限、冲突、消费和目标检查。未新增 Slate 监听器或原始轮询。

## Marker 与旧数据

一个 ATAParkourMarker 保留一个 TriggerBox，两套独立 Spline / ChildActor 落点 / Prompt Anchor，以及独立 Upper/Lower ArcHeight、JumpDuration。两个落点都关闭，或任一启用曲线不足两个控制点，整个 Marker 无效。

原组件名必须保留：`LandingSpline`、`LandingTargetComponent`、`PromptWidget`；另一套为 `AdditionalLandingSpline`、`AdditionalLandingTargetComponent`、`AdditionalPromptWidget`。

| 来源 | 原组件 | Additional 组件 |
|---|---|---|
| 新 Marker / 旧 JumpToPoint | Upper | Lower |
| 旧 DropDown | Lower | Upper |

访问逻辑槽位使用 `GetLandingSpline(bLower)`、`GetLandingTargetComponent(bLower)`、`GetLandingPrompt(bLower)`、`GetLandingLocationForSlot`。不能假设原 LandingSpline 总是 Upper。

`bLegacySplineIsLower`、旧 MarkerType / ArcHeight / JumpDuration、逐对象自定义版本和 PostLoad 迁移均有实际用途，不能当垃圾删除。旧实例覆写优先于蓝图默认值；原曲线、组件变换、Prompt 类和 TriggerBox 数据保留。`bDirectionalLandingConfigured` 名称虽旧，仍是已序列化的迁移标志，保持名称以兼容现有资产。

`bAllowStationaryBackFacing` 由当前 BP_ThirdPersonCharacter 序列化，保留为兼容字段，执行逻辑从不读取它，不能绕过 80° 门槛。其他不再使用的方向阈值和旧速度门槛已经移除。

禁用落点不参与资格、Target 更新和提示，不删除控制点。重新启用恢复原曲线。Visibility / bDrawDebug 只控制辅助显示，UE 原生 Visualizer 在直接编辑组件时仍可能显示控制点。

## 动态落点与运动

LandingSpline 计算深度对应终点，不是飞行路径。`bUseXAsDepthAxis` 选择 Marker 局部 X 或 Y 深度；玩家位置与曲线点转换到 Marker 空间后，使用原 20 次二分搜索求解。两落点独立计算，要求深度轴单调。Map Check 保留无效配置与采样非单调警告。

起跳保存 Start、固定 End、实际起跳水平速度和选中落点参数；Flying、固定时长 Alpha、`Lerp + 4 * ArcHeight * Alpha * (1 - Alpha)`、无 Sweep 的 SetActorLocation、提前落地、Walking 恢复、LandingResponse、死亡 Abort、冻结/CustomTimeDilation 与边缘安全保持原实现。

无 Sweep 仍可能穿过途中障碍物。动画、Root Motion、Motion Warping、摄像机和碰撞改造不属于此阶段。

## Prompt

UTAPromptComponent 为获胜 Marker 的每个合格落点显示一个对应 Action 的单键提示。Upper 默认 Space/B，Lower 默认 Ctrl/A；两者都通过朝向资格时同时显示，扇形外的那个隐藏。各自跟随动态 LandingTarget；消费、冻结和权限拒绝会隐藏提示。

两个槽位继续使用 ETAPromptType::ParkourJump / ParkourDrop。图标按当前玩家 IMC、输入设备和语言刷新。普通交互与 QTE 仍使用现有统一锚点。

通用 `ShowPromptChord` 和 Xbox/PS 四个 LS 斜方向占位纹理按既有要求保留，但跑酷不使用它们。它们是有意保留的通用能力/备用资源，不是仍运行的旧方向系统。正式图标需求见 [InputIcons.md](InputIcons.md)。

## 消费与重新武装

`ConsumedHeldMarkers` 以整个 Marker 为单位。任意落点执行后，两 Action 都受同一消费记录阻止；按错键、改变方向或重复 Register 都不能绕过。

记录实际执行 Action 当时全部非零物理来源，如 Space+B 或 Ctrl+A。全部来源均有有效零观测才重新武装；Unknown、Lost、映射遮蔽、设备断开和菜单权限变化都不是 Release。后来按下的来源不加入已有记录。

真实离开 TriggerBox 立即清除该 Marker 注册和消费，包括飞行期间；最后一个重叠组件离开才算退出。支持持续按住跨 Marker A→B→A→B。Alt+Tab/F8 失去所有权后不合成 Held/Release，恢复时可能需要真实松开重按。

## 验证与工具

用户已验收严格槽位绑定与 80° 朝向限制。自动化覆盖输入/改键、三配置、纯垂直与跨侧、严格角度边界、逐落点预览、消费/所有权、原公式、落地惯性和冻结。入口文件为 `Source/The_Awakening/Tests/TAUpperLowerParkourTests.cpp`，并复用 Held、PhysicalRelease、Facing、Momentum 测试。

清理验证：Editor 编译通过，TheAwakening.Input + TheAwakening.Prompt + TheAwakening.Freeze 共 31 项通过（其中 10 项带无界面测试环境警告），失败及未运行均为 0。关键执行/轨迹/迁移函数及原角色和 Marker 蓝图与清理前快照一致，文档本地链接检查通过。报告：`Saved/Automation/ParkourFinal/index.json`；日志：`Saved/Logs/ParkourFinalTests.log`。自动化不等于完整打包验收。

`-run=TAParkourSetup` 可更新默认映射、创建缺失占位图标并审计旧 Marker；`AuditOnly` 仅审计。不会重存旧蓝图、关卡或外部 Actor。当前资产审计为 8 个 Marker（6 Upper、2 Lower），原控制点及 75/120 弧高、0.5 秒参数保留；报告为 `Saved/ParkourMigrationAudit.txt`。

后续改动需回归：真实设备与改键、80° 内外和边界、单/双落点按键严格匹配、连续跨框、菜单/Alt+Tab/F8/冻结/死亡，以及旧数据保存重载。不要恢复方向组合、单落点互通或静止背向例外。
