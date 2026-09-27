# 路径解谜小游戏（C++ / UMG）

## 创建蓝图界面

1. 创建 Widget Blueprint，父类选择 `TAPathPuzzleWidget`，例如 `WBP_PathPuzzle`。
2. 如果 Designer 保持空白，运行时自动生成完整占位界面。也可以创建自己的布局，并添加名为 **PuzzleCanvas** 的 Canvas Panel（Is Variable）。这个容器专门承载动态节点、线和文字，不要在里面放需要保留的装饰。
3. 将 PuzzleCanvas 的尺寸设为 `Appearance.BoardSize`（默认 820×535）。移动容器或对其外层做整体缩放不会破坏内部相对位置。响应式布局可在外层套 SizeBox / ScaleBox。
4. Class Defaults 中设置 `PuzzleSettings`、`Appearance` 和可选的 `NodeWidgetClass`。在 Appearance 的各个 Brush 中选择美术 Texture / Material；默认是不同颜色的矩形 Image。计时条默认绿色。
5. 蓝图调用 **Open Puzzle**，传入 PlayerController 和上述 Widget Class，得到运行中的实例。也可使用 Create Widget → 设置参数 → Add to Viewport，默认 `bAutoStart=true`。
6. 绑定实例的 **OnSucceeded / OnFailed**。事件发生时主游戏奖惩已经处理；外部决定触发门、剧情、音效，以及何时调用 **ClosePuzzle**。不会自动生成下一题、重玩或失败换题菜单。

每次真正重新进入创建新 Widget 实例；同一个实例不能通过重复 StartPuzzle 重置额度。这里没有修改任何关卡或自动挂载触发器，打开小游戏的位置由后续交互蓝图决定。

## 可选 Designer 控件

除自定义布局下必须存在的 PuzzleCanvas 外，以下均可选；存在时 C++ 自动绑定更新。

| 名称 | 类型 | 内容 |
|---|---|---|
| Progress_Time | ProgressBar | 剩余时间比例，颜色来自 Appearance.TimerColor |
| Text_Time | TextBlock | 剩余秒数 |
| Text_Stats | TextBlock | 能量、单元、种子、难度 |
| Text_Path | TextBlock | 当前路径 |
| Text_Effects | TextBlock | 已获得的效果（主游戏效果仍在等待结算） |
| Text_Status | TextBlock | 操作提示／结果原因 |
| Button_Undo / Text_Undo | Button / TextBlock | 撤销及剩余额度 |
| Button_Retry / Text_Retry | Button / TextBlock | 重试及剩余额度 |

节点可另建继承 `TAPathPuzzleNodeWidget` 的 WBP，设置到 NodeWidgetClass。自定义布局使用 Button_Node、Image_Node、Text_Label、Text_Effect；Button_Node 接收点击，图片用于表现。节点和线的坐标都在 PuzzleCanvas 内，连线是旋转的 Image，不是 Slate 绘线。底图可替换，标签仍是独立 TextBlock。

占位界面的文字目前是英文；自定义标签可使用 FText，后续可接项目本地化。不要把根界面设为 Hidden/Collapsed 或把 Tick Frequency 设为 Never 来隐藏它但保留对局；计时由显示中的 Widget Tick 驱动。需要退出时调用 ClosePuzzle。

## 规则和状态

- 点击 Start 才启动倒计时。边双向通行，允许回头换路线，不允许重复访问节点。起点、终点不消耗单元。
- 先支付连线能量和节点数量成本，超限立即失败；合法移动后先应用边效果，再应用节点效果，并再次检查上限。到达 End 且未超限则成功。
- `bEnableMinigameEffects` 和 `bEnableMainGameEffects` 分别控制小游戏内奖惩、主游戏待结算效果。两者互不依赖。
- `Difficulty` 为 Easy / Medium / Hard 枚举。随机种子使用独立 FRandomStream，不改变主游戏全局随机流的生成过程（未指定种子时只取一次随机种子）。
- 生成器使用与运行时相同的效果模拟验证解法，尝试筛选至少三个有效解和回头路线。搜索次数有限，失败使用有已知可行路线的固定拓扑兜底；Definition.bUsedFallback 可查询。
- 默认时间按规模生成 15–30 秒；TimeLimitOverride > 0 时使用指定秒数。

## 撤销、重试与技能

两个开关默认关闭，额度默认 0。额度是**每次进入小游戏的总使用次数**，不是一次能回退多少步。

- Undo：撤销一个已经走过的移动，恢复路径、消耗、上限和待结算效果；消耗一次额度，不退还时间。选择 Start 本身不算可撤销的一步。
- Retry：仅 Playing 且未结算时可用，重置同一道题的路径和效果，恢复初始能量／单元上限，清空撤销历史；消耗一次重试额度。**不重置计时器，等待重新点击 Start 时也继续倒计时**。
- 重试不补回撤销或重试额度。成功、超限或超时一旦结算，不允许再撤销或重试。
- 蓝图通过 Session → SetUndoAllowance / SetRetryAllowance 修改当前局总额度和开关。例如总额 1 已用 1，再设总额 2，则剩余 1 次。关闭再打开不会抹掉已使用次数。
- 下次进入的新实例使用那次传入的 PuzzleSettings 或 WBP 默认值重新分配额度。技能的永久加成需要由外部技能系统写入新实例的设置；本模块不替代技能存档。

## 主游戏奖惩：只在结算时应用

**成功和失败都应用当前路径已收集的主游戏效果。** 重试／撤销清掉的效果不发放。策略集中在 TAPathPuzzleSession.cpp 的 Settle 中，已加中文注释；以后若只想成功发放，在收集 Result.MainGameEffects 的条件加 Result.bSucceeded 即可。

默认对接：

| Effect.Type | 结算行为 |
|---|---|
| Money | TAPlayerState.AddMoney，支持负数并保持余额不低于零 |
| MaxHealth / Health | 通过玩家 ASC 修改对应属性，生命值和上限做边界保护 |
| Item | Inventory.TryAddItem，需要指定 ItemDefinition 和正数 Amount |
| GameplayEffect | 通过 ASC 应用指定 GameplayEffectClass / EffectLevel，可做攻击等 Buff |

MainGameEffectPool 留空使用金钱／生命默认效果，不生成没有定义资产的“道具”。自定义池可添加物品或 GameplayEffect，Label 用于节点显示。对于自定义 GameplayEffect，建议提供明确 Label；默认难度惩罚过滤按 Amount 的正负判断。

若需要完全自己接管，给接收对象添加 **TAPuzzleRewardReceiver** 接口，实现 **ApplyPuzzleReward**，并把对象传入 RewardReceiver。这个接口替代默认发放，不会再重复调用内置实现；返回实际应用的有符号数量（GameplayEffect 成功返回 1）。

OnRewardApplied 会报告每个效果的实际应用量。背包满时可能只放入部分物品，缺少目标／资产时返回 0；外部可在此决定如何处理未发放部分，组件不会自动重试发奖。每个结果只交付一次。OnSucceeded / OnFailed 中不要再次无条件发放 Result.MainGameEffects。

## 输入与退出

使用项目 PlayerController 时成对调用已有 BeginUIInputMode / EndUIInputMode，鼠标和菜单输入由现有系统管理。外部 ClosePuzzle／移除 Widget 会释放输入模式；未结算退出标为 Aborted 并触发 OnAborted，不视为胜负结算，不发放待结算效果。世界是否需要冻结由打开小游戏的外部流程决定。

本次未创建任何 WBP、美术资产或场景触发器，也未实现联网权威或跨关卡保存对局。

## 验证

自动化测试前缀：`TheAwakening.Puzzle`。覆盖会话额度、重试不重置倒计时、独立效果开关、边界失败、双向连接、随机种子与可通关解、默认奖励对接。

需要在 UE 里用最终 WBP 确认：节点鼠标点击、整体缩放位置、替换贴图后的尺寸、结算事件连接及物品容量不足时的外部处理。
