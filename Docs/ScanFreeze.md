# 扫描冻结组件

主角已在 C++ 构造函数中挂载 `TAFreezeComponent`。其他实体在蓝图的 Add Component 中搜索 `TAFreeze`，添加一个组件即可参与扫描冻结。不需要添加到 ScanActor、HighlightPPActor 或 PlayerController。

扫描按淡入进度逐渐减速，完全淡入时冻结；松开、取消或打开 UI 后，随淡出逐渐恢复。扫描组件 EndPlay 时立即释放自己的请求。世界时间、材质 Time、独立扫描 Actor 上的 Niagara 和扫描计时器没有被暂停或缩放。

组件自动向当前 World 的 `TAFreezeSubsystem` 注册。新加入实体立即应用当前强度。RequestFreeze(Source, Strength) 的 Strength 为 0～1，默认 1；多个来源取最大强度，小游戏仍使用立即完全冻结。释放扫描不会清除小游戏请求。请求者必须在 EndPlay 中释放自己的请求。

## 渐变曲线

扫描组件的 `Scan | Settings | Freeze` 中暴露 `TimeFreezeBlendCurve`，默认引用 `/Game/Materials/Scan/Curves/CRV_TimeFreezeBlend`。横轴是 ScanNormalizedTime（0 未扫描，1 完全淡入），纵轴是冻结强度（0 正常，1 停止）。有效速度为冻结前的 CustomTimeDilation × (1 − 强度)，完全恢复时还原原倍率，不强制写成 1。

两个 key (0,0)、(1,1) 为线性过渡；可增加中间 key 或调整切线。推荐保持单调、值域 0～1、两端不变。输出会限制到 0～1；扫描进度端点强制正常／完全冻结。曲线留空使用线性值。FadeInSpeed / FadeOutSpeed 控制过渡时长，BlendCurve 仍独立控制视觉混合。淡出中重新按键沿当前进度反向，不重置强度。

## 默认范围

- 过渡期间通过 Actor.CustomTimeDilation 缩放实体及组件 Tick 的 DeltaTime，包括 CharacterMovement、PaperZD 动画。完全冻结时再暂停 Actor Tick 及当时已有组件的 Tick，恢复时还原各自原本的 Tick 启用状态。
- `FreezeExempt` Component Tag 避免完全冻结时关闭 Tick，但普通组件仍继承拥有者的时间倍率。需要保持原速的效果应放到独立、未注册冻结的 Actor；当前扫描效果已经如此。特殊组件可单独适配时间来源。
- 主角冻结期间禁止移动执行、交互和新跑酷动作；镜头移动、扫描松键、菜单输入仍可接收。CameraBoom 和 FollowCamera 已添加 `FreezeExempt`，保持镜头偏移与 Camera Lag 更新；背包或对话打开时仍禁止镜头移动。
- 主角 CameraBoom 使用 TAFreezeExemptSpringArm，始终以世界 DeltaTime 更新 Camera Lag，包括拥有者倍率为 0 时；无需除以零做补偿。
- 蓝图可读取 `GetFreezeStrength`；`IsFrozen` / `IsActorFrozen` 与 `OnFreezeChanged` 表示是否达到完全冻结，不代表部分减速。

## 扫描权限与背包

扫描进行中不能打开背包，松开后即使特效还在淡出也可以打开。扫描组件提供 `IsScanEnabled()` 和 `SetScanEnabled(bool)`（蓝图可调用），默认允许扫描；可以在组件的 Scan / Permission 中配置初始权限。运行时关闭权限会取消扫描、恢复冻结和鼠标状态并保留淡出，结束原因为 `Canceled`。重新开启只恢复权限，按键仍按住时需要先松开再按，不会自动扫描。

## 后续敌人 AI 接入提醒

当前不暂停 AIController、BrainComponent、Behavior Tree 或 StateTree。它们可能在另一个 Actor 上更新；实现敌人时，需要通过冻结事件适配暂停和原状态恢复，不能只停怪物身上的 Tick。

世界 Timer、Delay、GAS 持续时间和外部伤害／交互调用仍可执行，应分别暂停或检查冻结状态。物理模拟、子 Actor、冻结期间动态添加的组件不在当前自动暂停范围内。实体在冻结期间也不应自行重新开启被暂停的 Tick。这是本地玩法实现，未提供多人网络同步。

## 验证

自动化测试：`TheAwakening.Freeze.Lifecycle` 与 `TheAwakening.Freeze.Blend`，覆盖 Tick 恢复、原本关闭的组件、排除标签、重复请求、多个来源、晚加入实体、渐变倍率、组件 DeltaTime 继承和原倍率恢复。

UE 中还需验证：移动和跑酷中按住扫描，主角停在当前帧但镜头可以移动；扫描特效和目标悬停正常；松开恢复原进度；淡出期间再次按下能重新冻结；扫描中按背包键不会打开或打断扫描，松开后可打开；扫描中关闭权限或切出窗口不会遗留冻结状态；重新开启权限后松开再按才开始扫描。
