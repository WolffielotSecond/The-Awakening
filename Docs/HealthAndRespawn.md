# 死亡与复活

伤害、保护、回滚见 [DamageSystem.md](DamageSystem.md)；时停与游戏时间见 [TimeSystem.md](TimeSystem.md)。本文只写死亡和复活。

## 流程

```
存活 → 死亡中（DeathDelay）→ 黑屏（FadeOutTime）→ 复位（黑屏中同一帧）→ 淡入（FadeInTime）→ 存活
```

| 阶段 | 发生的事 |
|---|---|
| 死亡（进入死亡中的那一帧） | 挂 `State.Dead`；申请优先级 900 的空输入请求（暂停也打不开）；回滚并关闭对话 / 小游戏；关闭通用弹窗、设置、暂停、背包、扫描；停住移动、中止跑酷；广播 `OnDeathStarted` |
| 死亡中 | 播放死亡动画（动画蓝图读 `IsDead()`），空中死亡照常落地 |
| 黑屏 | 镜头淡出到 `FadeColor`。以后商人拖回的演出插在这一段 |
| 复位 | 传送到复活点（同步 Controller 朝向，镜头偏移归零、跳过一次 Camera Lag）；Health = MaxHealth × `RespawnHealthRatio`；清临时伤害修改器和带 `Effect.ClearOnRespawn` 的持续 GE；广播 `OnWorldReset` |
| 淡入结束 | 移除 `State.Dead`；恢复扫描（死亡前的开关值）；广播 `OnRespawned`；加 `RespawnInvulnerableTime` 秒复活无敌（强制伤害不受影响）；最后释放输入请求 |

Pawn 不销毁、不走 `RestartPlayer`，背包和组件状态都保留。所有阶段按游戏时间计时。死亡期间一直按着的方向键，复活后会继续生效（与关闭菜单后的行为一致）。

## 配置

项目设置 → Game → **The Awakening Respawn**（`UTARespawnSettings`，存 `DefaultGame.ini`）：`DeathDelay` 1.5、`FadeOutTime` 0.5、`FadeInTime` 0.5、`RespawnInvulnerableTime` 2、`RespawnHealthRatio` 1、`FadeColor` 黑。

黑屏用 PlayerCameraManager 的镜头淡入淡出，不需要 WBP。若以后要换成带文字或动画的转场界面，在 `UTARespawnSubsystem::StartCameraFade` 处替换。

## 复活点 `ATACheckpoint`

在关卡里放置（或基于它建蓝图做外观）：

| 属性 / 组件 | 说明 |
|---|---|
| `CheckpointId` | 唯一 ID；存档、`TACheckpoint <Id>` 用 |
| `SpawnPoint`（箭头） | 复活位置和朝向，可与交互范围分开摆 |
| `InteractionRange`（Box） | 交互范围。玩家在范围内才出现交互提示、按交互键才能登记 |
| `bActivateOnOverlap` | 进入范围即登记（剧情节点） |
| `bIsLevelStart` | 关卡开始的默认复活点，**每关放一个** |
| `InteractTextId` | 默认 `Interact_Checkpoint`（设为复活点 / 設為復活點 / Set respawn point） |
| 蓝图事件 `OnActivated` / `OnDeactivated` | 成为 / 不再是当前复活点时，做激活特效 |

- 最后一次交互的复活点生效，回头交互旧复活点也会生效；只路过不交互不改变。
- 没有任何复活点时退回关卡的 PlayerStart 并打 Warning；连 PlayerStart 都没有则原地复活。
- 复活点只在本局有效，不存档。

## 杀死体积 `ATAKillVolume`

拉伸 `Volume`（Box）覆盖深渊、地图外。进入的、带生命组件的角色受到一次强制伤害（来源默认 `Damage.Source.Environment`），无敌和不会死都挡不住。

## 接入 OnWorldReset（敌人、可重置机关）

`UTARespawnSubsystem::Get(this)->OnWorldReset` 在复位时广播一次：敌人回到初始位置和状态、可重置机关复位。已开的门、已拾的物品、剧情旗标**不**重置（设计上没有死亡惩罚）。复活期间 `IsRespawning()` 为 true，敌人 AI 用它停止攻击。

## 调试命令

`TAKill`、`TACheckpoint <Id>`，其余见 DamageSystem.md。

## 与原实现方案的差异

- 复活无敌用生命组件的统一保护入口（无敌，按游戏时间到期），不是持续 GE。
- `State.Dead` 在淡入**结束**时移除（淡入期间仍是死亡状态，伤害无效、输入锁住），与交还控制同时发生。
- 扫描恢复为死亡前的开关值；扫描结束原因为 UIInterrupted。
- 复活点额外提供 `OnDeactivated` 蓝图事件。
- `The_Awakening.Build.cs` 增加了私有依赖 `DeveloperSettings`（`UTARespawnSettings` 需要）。

## 验证

自动化测试：`TheAwakening.Respawn.Flow`、`TheAwakening.Respawn.Checkpoints`、`TheAwakening.Respawn.KillVolume`，以及 `TheAwakening.Health.*` 中的死亡测试。

PIE 待验证（需要先在测试关卡放一个 `bIsLevelStart` 复活点和至少一个普通复活点）：

- `TAKill` 后：约 1.5 秒后黑屏，黑屏中传送到复活点，淡入后能动；HUD 回满。
- 复活后 2 秒内 `TADamage` 无效，之后正常。
- 交互 A、再交互 B、回头再交互 A 后死亡，在 A 复活；只路过不交互不改变复活点；交互提示只在 InteractionRange 内出现。
- 行走、疾跑、跑酷空中、落地惯性中分别 `TAKill`：角色停住，空中死亡会落地。
- 按住扫描时死亡：特效淡出，复活后松开再按才能扫描。
- 掉进杀死体积直接死亡，`TAGod` 打开时也会死。
- 死亡期间按 Esc、Tab、E、右键都没有反应；键鼠、手柄都测一遍；复活时 Alt+Tab 切出再回来没有卡住的输入锁。
