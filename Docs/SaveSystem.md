# 存档

最小存档层：一个存档槽，复活点激活时自动存。读档入口目前只有调试命令 `TALoad`（主菜单还没做）。版本号规则见 [VersionManagement.md](VersionManagement.md)。

## 存了什么

存档文件 `Saved/SaveGames/TA_Slot0.sav`（`UTASaveGame`），各系统各自提供一块数据，由 `UTASaveSubsystem` 收集和回写：

| 数据 | 来源 | 内容 |
|---|---|---|
| 存档头 | — | `SaveFormatVersion`、游戏完整版本、存档时间、关卡短名、当前复活点 `CheckpointId` |
| 剧情 | `UTADialogueSubsystem` | 旗标、每段剧情最后到达的节点、已完成的剧情、世界剧情 Tags、一次性世界事件完成记录和互斥分支结果 |
| 文本 | `UTADocumentSubsystem` | 已收集（按页序）、已读、日记本页码 |
| 背包 | `UTAInventoryComponent` | 内衣 / 外套定义、每个口袋**按格子位置**的物品（资产路径、数量、耐久）、剧情槽 |
| 钱 | `ATAPlayerState` | 金额 |

**不存**：场景里普通物品被捡走 / 丢下的状态（读档后关卡里的物品回到关卡原样）、生命值（读档后满血）。已收集的文本例外：读档后场景里对应的文本会被销毁。

## 什么时候存

- 玩家交互复活点时。
- 走进 `bActivateOnOverlap` 的复活点范围、它**新成为**当前复活点时（来回走动不重复存）。
- 调试命令 `TASave`。

读档恢复过程中、正在等待跨关卡读档时、没有玩家角色时不存。活动特殊动作 / QTE 期间（包括暂停挂起）拒绝手动及自动存档，不覆盖原有槽位；阶段进度不保存。

## 读档

1. 没有存档，或存档的 `SaveFormatVersion` **高于**当前程序：拒绝读档，打 Warning，不覆盖原存档。
2. 存档关卡和当前关卡不同：先打开存档关卡，玩家角色生成后再恢复（每 0.1 秒试一次，最多 5 秒）。
3. 恢复顺序：关闭背包 → 剧情 → 文本 → 背包 → 钱 → 销毁场景里已收集的文本 → 激活存档的复活点，并把角色传送到它的出生位置。

容错：

- 背包里的物品资产找不到时丢弃并打 Warning；衣服的口袋布局改了，多出的格子丢弃。外套定义找不到时视为没穿；内衣找不到时用角色默认内衣。
- 文本记录中的空 ID 和重复项丢弃，页码重新限制在范围内。
- 存档的复活点在关卡里找不到时，角色留在原地并打 Warning。

## 给新系统加存档

1. 在系统里定义一个 `USTRUCT` 存档数据（字段都加 `UPROPERTY()`），提供 `GetSaveData()` / `RestoreSaveData()`。
2. 在 `UTASaveGame` 里加一个字段。
3. 在 `UTASaveSubsystem::BuildSaveGame` 里取数据、在 `ApplySaveGame` 里回写。
4. 旧存档没有这个字段时读出来是默认值；默认值不合适、或改了已有字段的含义时，提升 `DefaultGame.ini` 的 `SaveFormatVersion` 并实现迁移。

`RestoreSaveData` 是整体覆盖，不记录回滚。

## 蓝图接口

`UTASaveSubsystem`（GameInstance 子系统）：`SaveGame`、`LoadGame`、`HasSaveGame`、`DeleteSaveGame`、`PeekSaveGame`（只读出存档头给界面显示，不应用）。以后的主菜单「继续游戏」用 `HasSaveGame` + `LoadGame`。

## 调试命令

| 命令 | 作用 |
|---|---|
| `TASave` | 立即存档 |
| `TALoad` | 读档 |
| `TASaveDelete` | 删除存档 |
| `TASaveInfo` | 在日志和屏幕上显示存档头（版本、时间、关卡、复活点、文本数） |

## 世界事件与特殊动作（2026-10-23）

读档时取消当前特殊动作、停止当前对话并取消运行中的世界事件，再恢复持久结果。QTE 进度、输入句柄、IMC、Sequence 运行位置和 Actor 引用不存档。新增 Story 字段对旧档使用空默认值；背包恢复已持有物品时补回其 GrantedStoryTags，兼容旧档已取得日记本的情况。具体恢复场景通过 Director.ApplyPersistentState 配置，见 [WorldInteractionSystem.md](WorldInteractionSystem.md)。
