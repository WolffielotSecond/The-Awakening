# 死亡与复活

伤害、保护、回滚见 [DamageSystem.md](DamageSystem.md)；时停与游戏时间见 [TimeSystem.md](TimeSystem.md)。本文只写死亡和复活。

## 流程

```
存活 → OnDeathStarted → 死亡演出接口 → 黑屏过渡 / 复活前演出接口 → 复位（黑屏中同一帧）→ 淡入（FadeInTime）→ 存活
```

| 阶段 | 发生的事 |
|---|---|
| 死亡（进入死亡中的那一帧） | 挂 `State.Dead`；申请优先级 900 的空输入请求（暂停也打不开）；回滚并关闭对话 / 小游戏；关闭通用弹窗、设置、暂停、背包、扫描；停住移动、中止跑酷；广播 `OnDeathStarted` |
| 死亡中 | 调用角色 Play Death Presentation；默认 Print String「死亡演出」，持续 DeathDelay 秒，并用 TimeSubsystem 等待同样时长；可覆写为等待动画完成。空中死亡照常落地 |
| 黑屏过渡 / 复活前演出 | 调用角色 Play Blackout Presentation；默认 Print String「黑屏过度」，持续 FadeOutTime 秒，同时镜头淡出到 FadeColor；覆写时可在复位前插入演出，完成后才继续 |
| 复位 | 传送到复活点（同步 Controller 朝向，镜头偏移归零、跳过一次 Camera Lag）；Health = MaxHealth × `RespawnHealthRatio`；清临时伤害修改器和带 `Effect.ClearOnRespawn` 的持续 GE；广播 `OnWorldReset` |
| 淡入结束 | 移除 `State.Dead`；恢复扫描（死亡前的开关值）；广播 `OnRespawned`；加 `RespawnInvulnerableTime` 秒复活无敌（强制伤害不受影响）；最后释放输入请求 |

Pawn 不销毁、不走 `RestartPlayer`，背包和组件状态都保留。原有计时和新增演出保底计时均使用 TimeSubsystem 游戏时间。死亡期间一直按着的方向键，复活后会继续生效（与关闭菜单后的行为一致）。屏幕占位文字使用开发版 Print String 的显示时长，调试文字自身不负责推进阶段，受屏幕调试消息开关和构建类型影响。

## 配置

项目设置 → Game → **The Awakening Respawn**（`UTARespawnSettings`，存 `DefaultGame.ini`）：`DeathDelay` 1.5、`FadeOutTime` 0.5、`FadeInTime` 0.5、`RespawnInvulnerableTime` 2、`RespawnHealthRatio` 1、`FadeColor` 黑。

新增 `PresentationMaxWait` 默认 60 秒，分别限制两个演出阶段的等待时间；有效保底时长取该值（至少 1 秒）与该阶段默认时长 + 1 秒的较大值，不会截短配置的默认演出。自定义演出超时会打警告并继续下一阶段，防止漏接完成回报后永久锁输入。

默认黑屏用 PlayerCameraManager 的镜头淡入淡出，不需要 WBP。要加入带文字或动画的转场界面，可覆写下述 Play Blackout Presentation，并在界面 / 演出完成后回报。

## 两个可覆写的死亡演出接口

接口位于 `AThe_AwakeningCharacter`，使用现有角色蓝图（例如 `BP_ThirdPersonCharacter`）即可覆写，无需添加组件或更换复活子系统。它们是带 C++ 默认实现的 BlueprintNativeEvent，不需要自己绑定 OnDeathStarted 来启动这两个阶段。原 OnDeathStarted 所有监听完成、旧动作打断和输入锁定后，系统才调用第一个接口。

| 角色蓝图 Override | 默认 C++ 行为 | 自定义完成后调用 |
|---|---|---|
| Play Death Presentation | Print String「死亡演出」+ 等待 DeathDelay | Respawn → Complete Death Presentation |
| Play Blackout Presentation | Print String「黑屏过度」+ FadeOutTime 镜头淡出，保持黑屏 | Respawn → Complete Blackout Presentation |

两个事件均收到 `Respawn`（当前复活子系统）、`RunId`（本次死亡编号）、`DamageSource`（最近伤害来源 GameplayTag）、`DamageType`（伤害计算类型）。两个完成节点的输入都是 `Character = Self` 和本次事件的 `RunId`，Target 使用事件给出的 `Respawn`。阶段不匹配、角色不匹配、重复完成、上一次死亡的迟到回报均忽略。

### 接死亡动画

1. 打开玩家角色蓝图，在 Override 中添加 Play Death Presentation。
2. 从事件取得并为本次播放保存 Respawn、RunId、DamageSource、DamageType。
3. 根据 DamageSource 选动画，播放 Montage、PaperZD 动画或 Sequence。
4. 在本次动画的完成通知中，调用 Respawn 的 Complete Death Presentation，传 Self 和本次 RunId。不要在播放节点后立刻接完成，除非该节点本身会等待播放完成。
5. 正常结束与中断出口都应有明确收尾；让所有出口只回报一次。异步回调保留该次播放的 RunId，不要在旧动画回调中读取已经被下一次死亡覆盖的公共变量。

覆写后系统不会并行继续默认 DeathDelay。未支持的死因分支可调用 Add Call to Parent Function，让它使用默认等待；也可显式调用 Respawn → Run Default Death Presentation（Self、RunId）。默认分支会自行回报完成，不要再另接完成节点。不要一边调用 Parent 启动默认计时，一边要求系统等待更长的自定义动画。

现有 CharacterActionComponent 也有动画请求和完成通知，但它监听 OnDeathStarted 来打断旧动作。若需要复用，应从本接口启动新动作，按本次动作 Tag 匹配 OnActionFinished，再回报；不能假设随便收到一个动作结束就代表本次死亡演出结束。

### 接黑屏过渡和复活前演出

覆写 Play Blackout Presentation 后，它负责从死亡画面过渡到最终黑屏：可以先调用 PlayerCameraManager 的 Start Camera Fade，等待完全黑屏，再接音效、字幕或商人救援等 Sequence。若演出需要可见场景，自行安排演出期间的镜头、淡入和再次淡出；最后恢复镜头并在全黑时回报 Complete Blackout Presentation。

完成回报前，玩家仍在死亡位置、未执行传送回血和 OnWorldReset；系统持续持有死亡输入锁。回报后系统强制保持 FadeColor 遮挡，执行原有复位，再按 FadeInTime 亮屏。不要自行清 State.Dead、交还输入或调用复位；这几件事仍由复活系统统一完成。

未覆写时不需要任何蓝图连线。若覆写后某个分支要回退原行为，调用 Parent 或 Run Default Blackout Presentation（Self、RunId），它会自动淡出并回报。这个回退接口不会为你额外等待自定义 Sequence；需要在黑屏中继续演出时，应由自定义逻辑掌握完成回报。

自定义计时用已有 TimeSubsystem 的 Set Gameplay Timer by Event；不新增 Tick 时间轮询。普通蓝图 Delay 不等同于 TimeSubsystem 的统一时停规则。默认镜头淡出沿用原 CameraManager 行为，本轮没有另写镜头时钟。角色销毁时取消等待并清理输入和镜头，关卡卸载时清理演出计时器。

### 不同死亡情况如何分流

事件传入的是进入复活时记录的伤害来源与类型快照，两阶段使用同一份数据。例如对 DamageSource 做 Matches Tag：Enemy → 被敌人击杀动画，Environment → 环境死亡动画，其他来源 → 默认死亡演出。需要更细分时，先登记 GameplayTag，再由致命伤害调用方通过现有伤害接口传入更具体的来源，例如环境下的坠落、某种敌人的处决；角色蓝图按这些 Tag 选择不同动画或 Sequence。示例细分 Tag 并非本轮已创建的资源。

DamageType 的 Normal / True / Forced 描述伤害结算方式，不等于处决、坠落等演出类别。当前事件没有具体攻击者引用或处决配对信息；需要双人同步动画时仍需后续补死亡上下文。直接设置 Health 为 0 不经过伤害记录，DamageSource 可能为空或是旧伤害，所以不同死因应尽量通过已有 ApplyDamage 路径提交，未识别来源必须留默认分支。

`OnWorldReset` 保留原逻辑，未改成读档。QTE 死亡取消仍保留 OnQTEEnded(false) / OnFinished(false)，不触发超时事件；死亡演出无需由 QTE 回调启动。

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
- 当前复活点会进存档，交互或新激活复活点时自动存档，见 [SaveSystem.md](SaveSystem.md)。

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

2026-10-23：新增 `TheAwakening.Respawn.PresentationHooks` 与 `TheAwakening.Respawn.BlueprintPresentation`。覆盖两个阶段异步等待、伤害上下文快照、重复 / 旧回调拒绝、默认分支回退、漏接完成保底、角色销毁清理，以及临时角色子蓝图的两个 Override → Complete 节点实际连线编译。UE 5.6 编译 / UHT 通过；Health / Respawn / Interaction / Input / Prompt / Time / Freeze 共 55/55 通过（41 项无警告、14 项带警告，包含测试世界、UI、GameplayCue 与 Wwise 日志），报告 `Saved/Automation/DeathPresentationFinal/index.json`。正式关卡中占位文字显示和真实动画 / Sequence 接线仍需 PIE 验收。

PIE 待验证（需要先在测试关卡放一个 `bIsLevelStart` 复活点和至少一个普通复活点）：

- `TAKill` 后：约 1.5 秒后黑屏，黑屏中传送到复活点，淡入后能动；HUD 回满。
- 复活后 2 秒内 `TADamage` 无效，之后正常。
- 交互 A、再交互 B、回头再交互 A 后死亡，在 A 复活；只路过不交互不改变复活点；交互提示只在 InteractionRange 内出现。
- 行走、疾跑、跑酷空中、落地惯性中分别 `TAKill`：角色停住，空中死亡会落地。
- 按住扫描时死亡：特效淡出，复活后松开再按才能扫描。
- 掉进杀死体积直接死亡，`TAGod` 打开时也会死。
- 死亡期间按 Esc、Tab、E、右键都没有反应；键鼠、手柄都测一遍；复活时 Alt+Tab 切出再回来没有卡住的输入锁。
