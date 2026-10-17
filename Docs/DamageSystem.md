# 伤害、保护与回滚

## 伤害入口

所有伤害只走 `UTADamageLibrary::ApplyDamage(Instigator, Target, Amount, DamageType, Source)`（蓝图可调用）。

| 类型 `ETADamageType` | Tag | 计算 | 保护 |
|---|---|---|---|
| Normal | `Damage.Type.Normal` | 完整公式（见下） | 生效 |
| True（真实伤害） | `Damage.Type.True` | X × Π真伤倍率 | 生效 |
| Forced（强制伤害） | `Damage.Type.Forced` | 直接扣 X | **无视所有保护和修改器** |

- `Source` 必填，必须是 `Damage.Source.*` 的子 Tag：`Enemy`、`Puzzle`、`Console`、`Environment`、`Script`。缺失时拒绝并打 Warning。新增来源在 `Core/TAGameplayTags.h/.cpp` 加一行。
- 死亡状态下所有伤害（包括强制伤害）都被忽略。
- 伤害保留小数，HUD 向上取整显示。
- 回血、MaxHealth 变化不是伤害，仍直接改属性。

## 普通伤害公式

```
D1 = X + Σ数值易伤
D2 = D1 × Π(1 + 百分比易伤)
D3 = max(0, D2 − 防御力 × (1 + 防御力加成))
D4 = max(0, D3 − Σ固定值减伤)
最终 = D4 × Π(1 − 百分比减伤)
```

- 防御力 `Defense`、防御力加成 `DefenseBonus` 是角色基础属性（`UTAAttributeSet`），不参与 Tag 匹配。原 `DamageReduction` 属性已删除。
- 例：X=100，数值易伤 10，百分比易伤 20%，防御 5、加成 50%，固定减伤 15，百分比减伤 30% 与 20% → 61.32。
- 每一步单独截断到 0；负的固定值减伤等于固定加伤，在防御之后仍生效。

## 伤害修改器（列表）

挂在角色的 `UTAHealthComponent` 上：`AddDamageModifier(Modifier, Owner)` → 句柄；`RemoveDamageModifier(Handle)`。

| 字段 | 说明 |
|---|---|
| `Kind` | 数值易伤 / 百分比易伤 / 固定值减伤 / 百分比减伤 / 真伤倍率 |
| `Value` | 百分比写小数（0.3 = 30%） |
| `Condition` | `FGameplayTagRequirements`，匹配本次伤害的类型 Tag + 来源 Tag；为空对所有伤害生效 |
| `Duration` | > 0 为临时修改器：从添加开始按**游戏时间**计时，到期自动移除并广播 `OnDamageModifierExpired`；<= 0 为永久 |

- 结算时遍历整个列表，每一条条件匹配的都计入；数值类相加，百分比 / 倍率类相乘，不需要上限。
- 只能由持有者用句柄移除；持有者销毁后自动失效。
- 复活时只清临时修改器（`ClearTemporaryDamageModifiers`），永久修改器保留。

## 保护

`AddProtection(Kind, Owner, AllowedDamageSources)` → 句柄；`RemoveProtection(Handle)`。同时在 ASC 上维护 `State.Invulnerable` / `State.Undying` 供查询，不要直接加减这两个 Tag。

| 保护 | 效果 |
|---|---|
| 无敌 Invulnerable | 普通 / 真实伤害完全无效；来源匹配 `AllowedDamageSources` 的放行 |
| 不会死 Undying | 普通 / 真实伤害照常扣血，Health 最低 1 |

两者都挡不住强制伤害。多条保护并存时，只要有一条无敌不放行该来源就挡住。

| 场景 | 保护 |
|---|---|
| 小游戏（打开到关闭） | 无敌（放行 `Damage.Source.Puzzle`）+ 不会死 |
| 对话（开始到结束） | 无敌 + 不会死 |
| `TAGod` | 无敌 |
| 复活后保护 | 无敌（第 4 步实现） |

小游戏扣血惩罚 = 真实伤害 + 来源 Puzzle，因此小游戏期间只有惩罚能扣血，且扣不死。

## 死亡时：关闭所有页面 + 回滚

死亡清理第 3 步先执行 `UTARollbackSubsystem::RollbackAll()`，再关通用弹窗、设置、暂停、背包、扫描。

回滚日志（`Health/TARollbackSubsystem.h`）：

- 对话、小游戏开始时 `BeginScope`，正常结束时 `CommitScope`（丢弃记录）。
- 期间会改游戏状态的代码调用 `RecordUndo(Label, 撤销操作)`；蓝图用 `Record Undo by Event`。
- 死亡时从最内层作用域到最外层，各自按记录的**逆序**撤销，然后执行作用域的中止动作：对话 `StopDialogue`、小游戏 `ClosePuzzle`。回滚过程中产生的改动不会再被记录。

当前已记录的操作：

| 来源 | 记录内容 |
|---|---|
| 所有伤害（`ApplyDamage`） | 扣血前的 Health |
| 对话 | 剧情旗标（原来没有的会被删除）、剧情进度、完成标记、触发器“已播放” |
| 小游戏结算（`TAPuzzleRewards::ApplyDefault`） | 金钱实际变化量、物品实际添加数、GE（持续型移除效果；即时型恢复属性快照）、Health / MaxHealth 属性快照 |

**新增功能的约定**：以后的 `OnStoryEvent` 监听、自定义 `ITAPuzzleRewardReceiver`、对话或小游戏中会改游戏状态的任何代码，都必须自己 `RecordUndo`，否则死亡时不会回滚。

## 调试命令

`TADamage <数值>`（普通）、`TATrueDamage <数值>`（真实）、`TAKill`（强制，扣光）、`TAGod`（切换无敌）、`TAHeal [数值]`（回血，不填回满；不是伤害）。伤害来源均为 `Damage.Source.Console`。

## 已知限制

- 物品回滚按物品定义移除：格子顺序会重新整理；武器按当时的实例移除，耐久不保证与原实例一致。剧情格没有增删接口，目前也没有会改剧情格的结算。
- 即时型 GE 奖励的回滚靠 `UTAAttributeSet` 属性快照，只覆盖这套属性。
- 金钱撤销用 `AddMoney(−实际变化量)`，若期间金钱被花掉会在 0 处截断（小游戏期间不能消费，目前不会发生）。
- 回滚会把致死那一下的 Health 也恢复回去；角色仍处于死亡状态，复活时会回满血。
- `OnDialogueEnded` 对正常结束和死亡中止一视同仁地广播，没有参数区分。
- 对话期间“无敌 + 不会死”是按小游戏的规则类推的，需要策划确认。

## 验证

自动化测试：`TheAwakening.Health.DamageFormula`、`DamageEntry`、`Protection`、`ModifierList`、`RollbackJournal`、`RollbackOnDeath`，以及原有的死亡、绑定测试；`TheAwakening.Puzzle.Rewards` 保持通过。

PIE 待验证：

- `TADamage` / `TATrueDamage` / `TAKill` 在 Output Log 中的数值符合公式。
- 小游戏中：`TADamage` 无效，惩罚扣血但最多扣到 1；`TAKill` 后小游戏关闭，结算的金钱、物品、血量都恢复，可以重新打开。
- 对话中：`TADamage` 无效；`TAKill` 后对话关闭，旗标恢复，同一个触发器可以再次触发。
- 死亡时暂停、设置、通用弹窗、背包都会关闭。
