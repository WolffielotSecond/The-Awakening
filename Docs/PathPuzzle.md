# 路径解谜小游戏（C++ / UMG）

## 当前测试入口

项目已有 `Content/UI/Minigame/WBP_TestPuzzle`，父类为 `TAPathPuzzleWidget`。运行游戏后按 **G**，主角的 `DebugOpenPathPuzzle` 会加载这个 WBP，并使用其默认设置打开谜题；已有菜单占用输入时不会再打开。此按键仅在非 Shipping 构建启用，不需要额外配置 Input Action。资源改名／移动后，需要同步修改该函数中的资源路径。

默认界面使用完全不透明的全屏背景，谜题内容保持比例缩放。右侧只显示能量、单元、路径和效果，不显示 Seed、Difficulty；底部操作提示已隐藏。成功或失败后停留 1 秒，自动返回主游戏。

## 创建蓝图界面

1. 创建 Widget Blueprint，父类选择 `TAPathPuzzleWidget`，例如 `WBP_PathPuzzle`。
2. 如果 Designer 保持空白，运行时自动生成完整占位界面。也可以创建自己的布局，并添加名为 **PuzzleCanvas** 的 Canvas Panel（Is Variable）。这个容器专门承载动态节点、线和文字，不要在里面放需要保留的装饰。
3. 将 PuzzleCanvas 的尺寸设为 `Appearance.BoardSize`（默认 820×535）。移动容器或对其外层做整体缩放不会破坏内部相对位置。响应式布局可在外层套 SizeBox / ScaleBox。
4. Class Defaults 中设置 `PuzzleSettings`、`Appearance` 和可选的 `NodeWidgetClass`。在 Appearance 的各个 Brush 中选择美术 Texture / Material；默认是不同颜色的矩形 Image。计时条默认绿色。
5. 蓝图调用 **Open Puzzle**，传入 PlayerController 和上述 Widget Class，得到运行中的实例。也可使用 Create Widget → 设置参数 → Add to Viewport，默认 `bAutoStart=true`。
6. 绑定实例的 **OnSucceeded / OnFailed**。事件发生时主游戏奖惩已经处理；外部可触发门、剧情、音效。目前临时流程为成功／失败后延迟 1 秒调用 **ClosePuzzle**，移除界面、释放时停并恢复游戏输入。替换正式结算流程时，修改 HandleSettled 中有中文注释的定时器段落即可；提前移除界面会取消定时器。不会自动生成下一题、重玩或失败换题菜单。

每次真正重新进入创建新 Widget 实例；同一个实例不能通过重复 StartPuzzle 重置额度。这里没有修改任何关卡或自动挂载触发器，打开小游戏的位置由后续交互蓝图决定。

## 调用时指定本局设置

蓝图使用 **Open Puzzle With Settings**：Player 接玩家控制器，Widget Class 接自己的 WBP。右键 Settings 引脚 → Split Struct Pin，或连接 Make TAPuzzleSettings，设置 Difficulty、bEnableMinigameEffects（局内奖惩）、bEnableMainGameEffects（局外奖惩），以及撤销／重试开关和额度、时间覆盖、奖励池。Settings 整体替代 WBP 的玩法默认设置；外观仍由 WBP 决定。返回实例可绑定成功／失败事件，失败返回空。

```cpp
#include "Puzzle/TAPathPuzzleWidget.h"

FTAPuzzleSettings Settings;
Settings.Difficulty = ETAPuzzleDifficulty::Hard;
Settings.bEnableMinigameEffects = true;
Settings.bEnableMainGameEffects = false;
UTAPathPuzzleWidget* Puzzle = UTAPathPuzzleWidget::OpenPuzzleWithSettings(
    PlayerController, PuzzleWidgetClass, Settings);
```

此入口会完成生成、显示和时停。原 OpenPuzzle 继续使用 WBP 默认设置；两者均可选传入 RewardReceiver 和 Seed（-1 为随机）。

Player 需要有效的本地 PlayerController；Widget Class 留空时使用原生 C++ 占位界面。RewardReceiver 留空使用默认奖惩逻辑；传入对象必须实现下文的接口，否则启动失败。Seed 任意负数均随机，非负数可用于在相同设置、相同代码版本下复现题目。工厂函数不会自动防止重复打开，正式交互入口应自行管理返回的实例。

### Settings 参数

| 参数 | 默认值 | 用途 |
|---|---|---|
| Difficulty | Medium | Easy / Medium / Hard |
| bEnableMinigameEffects | true | 启用局内能量、单元相关奖惩 |
| bEnableMainGameEffects | true | 启用主游戏待结算奖惩 |
| bEnableUndo / UndoAllowance | false / 0 | 撤销开关与每局总额度 |
| bEnableRetry / RetryAllowance | false / 0 | 重试开关与每局总额度 |
| TimeLimitOverride | 0 | 大于 0 时覆盖默认时间，单位秒 |
| MainGameEffectPool | 空 | 自定义主游戏效果池；空时使用内置池 |

设置在 StartPuzzle 时复制进 Session。开局后修改 Widget.PuzzleSettings 不会改变当前局；额度调整应使用 Session 的专用接口。使用 Create Widget 手动创建时，应在 Add to Viewport 前配置参数。若关闭 bAutoStart，建议先调用 StartPuzzle，确认成功后再 Add to Viewport，让界面构建时正常接管输入和时停。

## 可选 Designer 控件

除自定义布局下必须存在的 PuzzleCanvas 外，以下均可选；存在时 C++ 自动绑定更新。

| 名称 | 类型 | 内容 |
|---|---|---|
| Progress_Time | ProgressBar | 剩余时间比例，颜色来自 Appearance.TimerColor |
| Text_Time | TextBlock | 剩余秒数 |
| Text_Stats | TextBlock | 能量、单元（不显示种子和难度） |
| Text_Path | TextBlock | 当前路径 |
| Text_Effects | TextBlock | 已获得的效果（主游戏效果仍在等待结算） |
| Text_Status | TextBlock | 兼容旧布局的绑定，运行时隐藏，不再显示底部提示 |
| Button_Undo / Text_Undo | Button / TextBlock | 撤销及剩余额度 |
| Button_Retry / Text_Retry | Button / TextBlock | 重试及剩余额度 |

节点可另建继承 `TAPathPuzzleNodeWidget` 的 WBP，设置到 NodeWidgetClass。自定义布局使用 Button_Node、Image_Node、Text_Label、Text_Effect；Button_Node 接收点击，图片用于表现。节点和线的坐标都在 PuzzleCanvas 内，连线是旋转的 Image，不是 Slate 绘线。底图可替换，标签仍是独立 TextBlock。

Appearance 中可配置 NodeNormal / NodeEndpoint / NodeSelected / NodeFailed 四种节点 Brush，以及 EdgeNormal / EdgeSelected / EdgeFailed 三种连线 Brush。BoardSize 默认 820×535，BoardPadding 默认 (55,35)，NodeSize 默认 78×52，EdgeThickness 默认 4；TimerColor 控制计时条颜色。运行时调整图片或板面参数后可调用 RefreshBoard 重建动态内容，它不会重新出题或补充次数。

`Appearance.Show Node Labels` 默认关闭，控制节点上的 1A、2A 等名称是否显示，不影响节点效果文字或内部名称。运行时修改后调用 `RefreshBoard`。手柄左摇杆移动虚拟光标，Xbox A（Gamepad Face Button Bottom）选择光标下的节点；空白处按 A 不会误触发其他焦点按钮，长按不会重复选择。光标下的撤销／重试按钮也可用 A 操作，仍遵循开关和次数限制。

全屏背景由空 Designer 的 C++ 占位布局生成。若使用自定义 Designer 根布局，需要自行添加铺满根容器、不透明的背景，并把等比缩放限制在谜题内容层，避免露出主游戏画面。

占位界面的文字目前是英文；自定义标签可使用 FText，后续可接项目本地化。不要把根界面设为 Hidden/Collapsed 或把 Tick Frequency 设为 Never 来隐藏它但保留对局；计时由显示中的 Widget Tick 驱动。需要退出时调用 ClosePuzzle。

## 规则和状态

- 点击 Start 才启动倒计时。边双向通行，允许回头换路线，不允许重复访问节点。起点、终点不消耗单元。
- 先支付连线能量和节点数量成本，超限立即失败；合法移动后先应用边效果，再应用节点效果，并再次检查上限。到达 End 且未超限则成功。
- `bEnableMinigameEffects` 和 `bEnableMainGameEffects` 分别控制小游戏内奖惩、主游戏待结算效果。两者互不依赖。
- `Difficulty` 为 Easy / Medium / Hard 枚举。随机种子使用独立 FRandomStream，不改变主游戏全局随机流的生成过程（未指定种子时只取一次随机种子）。
- 生成器使用与运行时相同的效果模拟验证解法，尝试筛选至少三个有效解和回头路线。搜索次数有限，失败使用有已知可行路线的固定拓扑兜底；Definition.bUsedFallback 可查询。
- 默认时间按规模生成 15–30 秒；TimeLimitOverride > 0 时使用指定秒数。

当前难度主要改变效果密度及是否允许惩罚：Easy 不生成按 Amount 判定的惩罚效果；Medium 和 Hard 允许惩罚，Hard 的效果出现概率更高。它不是固定地图规模或固定倒计时的档位。

局内效果包括 EnergyLimit（调整能量上限）、UnitLimit（调整单元上限）、EnergyUsed（调整已消耗能量，负数为返还能量）。前两者最低为 1，已消耗能量最低为 0。等于上限仍合法，超过才失败；如果移动成本已经超限，不能靠目标节点的增益挽救。当前生成器只在节点生成主游戏效果，连线只生成局内效果。

### 查询状态和操作接口

Widget 提供 SelectNode（传入节点数组索引）、Undo、Retry、RefreshBoard、ClosePuzzle，均可从蓝图调用。StartPuzzle 每个实例只成功初始化一次；重新进入请创建新实例。ClosePuzzle 才是完整退出入口，单独调用 Session.Abort 只终止规则状态，不会移除界面或释放输入／时停。

通过 Widget.Session 可查询：

| 数据／函数 | 用途 |
|---|---|
| Definition | 节点、边、起终点、初始上限、时间、种子及兜底标记 |
| Settings | 本局实际使用的设置 |
| Progress | 当前路径索引、消耗、当前上限及收集的效果 |
| State | Ready、Playing、Succeeded、Failed、Aborted |
| TimeRemaining | 剩余秒数 |
| LastMessage | 内部提示，默认界面不显示，可供自定义 UI 使用 |
| Result | 结算后的结果快照 |
| CanUndo / CanRetry | 当前是否允许操作 |
| GetUndoRemaining / GetRetryRemaining | 剩余额度 |
| IsTerminal | 是否成功、失败或中止 |

Session.OnChanged 通知路径、额度等变化，不会每帧通知倒计时；自定义计时显示可在 Widget Tick 中读取 TimeRemaining。不要额外重复调用 AdvanceTime，以免时间扣两次。

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

RewardReceiver 传对象实例引用，不是类引用，例如实现接口的关卡终端 Actor。接口参数为 Effect（本条效果）、Player（玩家控制器）、Result（本局结算快照）。局外奖惩关闭或没有收集主游戏效果时，不会调用。普通测试可留空。

效果条目使用 Type、Amount、Label；Item 类型还需 Item 资产，GameplayEffect 类型需 GameplayEffectClass 和 EffectLevel。Amount 是有符号数量，Label 只影响显示，不改变规则。自定义池只采纳主游戏类型；Easy 还会过滤负数惩罚，非空池若全部被过滤不会自动补回内置奖励。

OnRewardApplied 会报告每个效果的实际应用量。背包满时可能只放入部分物品，缺少目标／资产时返回 0；外部可在此决定如何处理未发放部分，组件不会自动重试发奖。每个结果只交付一次。OnSucceeded / OnFailed 中不要再次无条件发放 Result.MainGameEffects。

### 结算事件与结果

调用顺序为：锁定胜负 → 应用全部主游戏效果 → 逐条广播 OnRewardApplied → 广播 OnSucceeded 或 OnFailed。1 秒关闭计时器在外部事件广播前安排；若事件内提前关闭界面，会取消定时器。OnAborted 只用于未结算退出，不表示失败。

OnSucceeded / OnFailed 的 Result 包含 bSucceeded、Failure、Progress、MainGameEffects、TimeRemaining 和 Seed。Failure 为 None、EnergyLimit、UnitLimit 或 TimeExpired。MainGameEffects 是本次尝试交付的效果，实际发放数量看 OnRewardApplied。Session.OnSettled 是底层规则事件；外部需要在奖励处理完后做事时，应绑定 Widget 的成功／失败事件。

## 输入与退出

使用项目 PlayerController 时成对调用已有 BeginUIInputMode / EndUIInputMode，鼠标和菜单输入由现有系统管理，期间禁止打开背包。小游戏界面显示时向 TAFreezeSubsystem 请求扫描同款时停，直到 ClosePuzzle／移除 Widget 才释放；结算后仍显示界面时继续冻结。挂有 TAFreezeComponent 的实体被冻结，UMG Tick 和倒计时正常运行；未接入实体、世界计时器和物理模拟不会被全局暂停。时停请求按来源管理，不会清除其他来源的冻结。退出也会释放输入模式；未结算退出标为 Aborted 并触发 OnAborted，不视为胜负结算，不发放待结算效果。

目前有测试 WBP 和 G 键入口，正式场景交互触发器、美术资产替换、联网权威和跨关卡保存对局尚未接入。

## 代码位置与后续修改

以下文件均位于 `Source/The_Awakening/Puzzle/`：

| 文件 | 职责 |
|---|---|
| TAPathPuzzleTypes.h / .cpp | 枚举、配置、数据结构及效果标签 |
| TAPathPuzzleRules.h / .cpp | 出题、解法验证、路径成本与效果规则 |
| TAPathPuzzleSession.h / .cpp | 对局状态、倒计时、撤销重试、结算快照 |
| TAPathPuzzleWidget.h / .cpp | 打开／关闭入口、UI、时停、结算奖励交付 |
| TAPathPuzzleNodeWidget.h / .cpp | 单个节点按钮和图片 |
| TAPuzzleRewardReceiver.h | 自定义奖惩接口 |
| TAPuzzleRewards.h / .cpp | 默认金钱、属性、物品和 GameplayEffect 对接 |

常见修改点：取消失败发奖，修改 Session::Settle 的 MainGameEffects 收集条件；替换 1 秒自动关闭流程，修改 Widget::HandleSettled 中的中文注释段；调整 G 键或测试资源路径，修改 The_AwakeningCharacter.cpp 的 DebugOpenPathPuzzle / SetupPlayerInputComponent。界面不出现时检查资源路径、Owning Player、自定义布局的 PuzzleCanvas 和 Receiver 接口；手动 StartPuzzle 失败时可读取 SetupError。

## 验证

自动化测试前缀：`TheAwakening.Puzzle`。覆盖会话额度、重试不重置倒计时、独立效果开关、边界失败、双向连接、随机种子与可通关解、默认奖励对接。

需要在 UE 里用最终 WBP 确认：节点鼠标点击、整体缩放位置、替换贴图后的尺寸、结算事件连接及物品容量不足时的外部处理。
