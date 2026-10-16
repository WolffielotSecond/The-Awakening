# 统一时停与游戏时间

所有需要停时间的功能只调用 `UTATimeSubsystem`（`Core/TATimeSubsystem.h`，World Subsystem），按来源申请、按来源释放。不要再直接调用 `SetPause` / `UGameplayStatics::SetGamePaused`，也不要直接调用 `UTAFreezeSubsystem` 的 `RequestFreeze` / `ReleaseFreeze`。

## 两种时停

| 模式 | 接口 | 效果 | 现有调用方 |
|---|---|---|---|
| 冻结 | `RequestFreeze(Source, Strength)` / `ReleaseFreeze(Source)` | 只冻结挂了 `TAFreezeComponent` 的实体，世界时间、特效、UI 照常运行。Strength 0～1，多个来源取最大值。细节见 [ScanFreeze.md](ScanFreeze.md) | 扫描（渐变）、小游戏（完全冻结） |
| 全局暂停 | `RequestPause(Source)` / `ReleasePause(Source)` | 引擎暂停。任意来源持有即暂停，最后一个来源释放才恢复；释放自己的请求不会解除别人的暂停 | 暂停菜单（来源为暂停页实例） |

来源对象被销毁后自动视为释放（暂停期间子系统仍会 Tick 清理）。

## 游戏时间

- `GetGameplayTime()`：本 World 累计游戏时间（秒），每帧累加 世界 DeltaTime × (1 − 冻结强度)，全局暂停时不走。规则与冻结组件对实体的减速一致。
- `GetGameplayTimeScale()`：当前倍率 = 世界时间倍率 × (1 − 冻结强度)，暂停时为 0。
- 计时器：`SetGameplayTimer(Delegate, Duration, bLoop)` / `ClearGameplayTimer(Handle)` / `IsGameplayTimerActive` / `GetGameplayTimerRemaining`。蓝图用 `Set Gameplay Timer by Event`。到期按先后顺序触发；回调里可以新增或清除计时器；绑定对象销毁后自动移除；循环计时器在一个长帧内最多补触发 16 次。

## 计时规则

**玩法计时一律用游戏时间**：伤害修改器持续时间、复活流程、复活保护，以及以后的 Buff、冷却、敌人行为等。

**例外（保持原有时间源，不改）**：

| 计时 | 时间源 | 原因 |
|---|---|---|
| 通用弹窗倒计时、设置 15 秒视频模式回退 | 真实时间 | 在暂停菜单里运行，用游戏时间会永远不到期 |
| 光标动画、高级数据 FPS 采样、显示器检测 | 真实时间 | 界面/系统层 |
| 扫描淡入淡出、扫描信息框延迟 | 组件 Tick / 世界 Timer | 扫描自己发起冻结 |
| 小游戏倒计时、结算后 1 秒关闭 | 真实时间 / 世界 Timer | 小游戏自己发起完全冻结，用游戏时间会卡死 |

新增功能如果自己发起时停，它自身的计时同样属于例外，需要在代码注释里写明。

## 高级数据

设置 → 显示高级数据 → 完整：末尾追加 `-平台-游戏时间`。

- 平台读取 `Config/DefaultEngine.ini` 的 `[OnlineSubsystem] DefaultPlatformService`（当前为 `Steam`），显示名用本地化 Key `Settings.Platform.<名称>`，缺失时显示原名。读配置而不是运行时在线子系统，因为编辑器里没开 Steam 客户端时在线子系统会退回 NULL。
- 游戏时间格式由本地化 Key `Settings.AdvancedData.GameTime` 决定：简体 / 繁体 `{Time}(倍率{Scale})`，英文 `{Time}(x{Scale})`。`{Time}` 为 `HH:MM:SS`（小时超过 99 继续累加），`{Scale}` 最多两位小数。

## 验证

自动化测试：`TheAwakening.Time.PauseSources`、`TheAwakening.Time.GameplayClock`、`TheAwakening.Time.GameplayTimers`、`TheAwakening.Time.AdvancedDataFormat`；原有 `TheAwakening.Freeze.*` 保持通过。

PIE 待验证：

- 打开 / 关闭暂停菜单，世界正常暂停与恢复；从设置页返回再关闭暂停也正常恢复。
- 高级数据（完整）显示 `Steam` 和游戏时间；扫描时倍率逐渐降到 0、松开后回到 1；小游戏期间为 0；暂停时为 0 且时间不走。
- 切换简 / 繁 / 英，格式随语言变化。
