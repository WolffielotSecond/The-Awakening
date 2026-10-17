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

## 血量 HUD

C++ 只提供数据和事件，界面全部在 WBP 里拼装。

1. 在 `Content/UI/HUD/` 新建 Widget Blueprint `WBP_HealthHUD`，父类 `TAHealthHUDWidget`。
2. 可选控件（按名字绑定，都可以不放）：

   | 控件名 | 类型 | C++ 做的事 |
   |---|---|---|
   | `ProgressBar_Health` | ProgressBar | 设置百分比 Health / MaxHealth |
   | `Text_Health` | TextBlock | 按本地化 Key `HUD_Health_Format`（`{Current} / {Max}`）显示，数值向上取整；切换语言时刷新 |

3. 心电图、受伤闪红等表现：覆写蓝图事件 `OnHealthPresentationChanged(Current, Max, Delta)`。它在每次数值变化后调用（包括绑定时的一次初始同步，Delta 为 0）；Delta < 0 表示受伤。
4. 创建：PlayerController 在 BeginPlay 按 `HealthHUDWidgetClass`（默认 `/Game/UI/HUD/WBP_HealthHUD`，`UI|Health` 分类）创建，层级 `HealthHUDZOrder`（默认 10）。HUD 设为 HitTestInvisible，不申请输入、不影响光标和焦点。没配或资源不存在时不创建，只打 Warning。
5. HUD 自己找到本玩家角色的 `UTAHealthComponent` 并订阅，换 Pawn 时自动跟随。

需要更多数据时直接用生命组件（蓝图可用）：

| 名称 | 说明 |
|---|---|
| `IsDead` / `GetHealth` / `GetMaxHealth` / `GetHealthNormalized` | 查询 |
| `OnHealthChanged(Current, Max, Delta)` | Health 或 MaxHealth 变化 |
| `OnDamageTaken(Amount, Type, Source)` | 实际扣血的伤害（被保护完全挡住的不广播），可按来源做不同受击反馈 |
| `OnDeathStarted` / `OnRespawned` | 进入死亡 / 复活完成 |
| `GetLastDamageSource` / `GetLastDamageType` | 最近一次伤害的来源与类型（例如死亡原因） |
| `HasProtection` | 是否处于无敌 / 不会死 |

## 调试命令

| 命令 | 作用 |
|---|---|
| `TAKill` | 强制伤害，扣光 Health（任何保护都挡不住） |
| `TAHeal [数值]` | 回血；不填数值则回满。死亡状态下无效，不能用来复活 |
| `TACheckpoint <Id>` | 直接设为当前复活点 |

伤害相关的 `TADamage`、`TATrueDamage`、`TAGod` 见 DamageSystem.md。只在 PIE / 开发版可用，Shipping 包没有。

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
