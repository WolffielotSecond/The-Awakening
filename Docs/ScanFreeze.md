# 扫描冻结组件

主角已在 C++ 构造函数中挂载 `TAFreezeComponent`。其他实体在蓝图的 Add Component 中搜索 `TAFreeze`，添加一个组件即可参与扫描冻结。不需要添加到 ScanActor、HighlightPPActor 或 PlayerController。

扫描成功开始时冻结，松开、取消、打开 UI 或扫描组件结束运行时解除。扫描淡出继续正常播放。世界时间、材质 Time、扫描 Niagara 和扫描计时器没有被暂停或缩放。

组件自动向当前 World 的 `TAFreezeSubsystem` 注册。冻结期间加入的实体也会冻结；多个来源同时请求时，最后一个来源释放才恢复。请求者必须在 EndPlay 中释放自己的请求。

## 默认范围

- 暂停实体 Actor Tick 及当时已有组件的 Tick，保留位置、运动速度、动画和跑酷进度；恢复各自原本的 Tick 启用状态。
- 要让某组件继续更新，在它的 Component Tags 中添加 `FreezeExempt`（不是 Actor Tags）。在冻结前配置。
- 主角冻结期间禁止移动执行、交互和新跑酷动作；镜头移动、扫描松键、菜单输入仍可接收。CameraBoom 和 FollowCamera 已添加 `FreezeExempt`，保持镜头偏移与 Camera Lag 更新；背包或对话打开时仍禁止镜头移动。
- 蓝图可读取 `IsFrozen` / `IsActorFrozen`，并监听 `OnFreezeChanged` 扩展特殊行为。

## 扫描权限与背包

扫描进行中不能打开背包，松开后即使特效还在淡出也可以打开。扫描组件提供 `IsScanEnabled()` 和 `SetScanEnabled(bool)`（蓝图可调用），默认允许扫描；可以在组件的 Scan / Permission 中配置初始权限。运行时关闭权限会取消扫描、恢复冻结和鼠标状态并保留淡出，结束原因为 `Canceled`。重新开启只恢复权限，按键仍按住时需要先松开再按，不会自动扫描。

## 后续敌人 AI 接入提醒

当前不暂停 AIController、BrainComponent、Behavior Tree 或 StateTree。它们可能在另一个 Actor 上更新；实现敌人时，需要通过冻结事件适配暂停和原状态恢复，不能只停怪物身上的 Tick。

世界 Timer、Delay、GAS 持续时间和外部伤害／交互调用仍可执行，应分别暂停或检查冻结状态。物理模拟、子 Actor、冻结期间动态添加的组件不在当前自动暂停范围内。实体在冻结期间也不应自行重新开启被暂停的 Tick。这是本地玩法实现，未提供多人网络同步。

## 验证

自动化测试：`TheAwakening.Freeze.Lifecycle`，覆盖 Tick 恢复、原本关闭的组件、特效排除标签、重复请求、多个来源和晚加入实体。

UE 中还需验证：移动和跑酷中按住扫描，主角停在当前帧但镜头可以移动；扫描特效和目标悬停正常；松开恢复原进度；淡出期间再次按下能重新冻结；扫描中按背包键不会打开或打断扫描，松开后可打开；扫描中关闭权限或切出窗口不会遗留冻结状态；重新开启权限后松开再按才开始扫描。
