# 通用弹窗与设置应用：从零搭建

此文档是设置系统的交互规则。设置界面的普通调整、开关只修改草稿；点击“应用设置”或触发 `IA_SettingsApply` 后才应用。长按恢复当前分类默认值则独立应用并保存，不顺带提交其他项目草稿。收藏仍然即时保存。旧的 `Button_ConfirmVideoMode`、`Button_RevertVideoMode` 已停止显示，显示确认改用通用弹窗。

## 一、已有资产

运行项目随附的 `Tools/Settings/add_settings_dialog_assets.py` 可增量创建下列资产。已有资产及已绑定的按键不会被覆盖。

| 资产 | 用途 |
|---|---|
| `/Game/Input/IMC_Settings` | 原设置输入，新增应用动作 |
| `/Game/Input/Actions/Settings/IA_SettingsApply` | 默认 Enter／手柄 Start，应用草稿 |
| `/Game/Input/IMC_CommonDialog` | 弹窗专用输入，只在弹窗接管输入时生效 |
| `/Game/Input/Actions/Dialog/IA_DialogOption1` | 默认 Enter／A |
| `/Game/Input/Actions/Dialog/IA_DialogOption2` | 当前 Delete／B |
| `/Game/Input/Actions/Dialog/IA_DialogOption3` | 当前 Escape／X |
| `/Game/Input/Actions/Dialog/IA_DialogOption4` | 默认 Space／Y |
| `/Game/UI/Common/WBP_CommonDialog` | 父类 `TACommonDialogWidget`，弹窗主体 |
| `/Game/UI/Common/WBP_CommonDialogButton` | 父类 `TACommonDialogButtonWidget`，一个带提示的按钮 |

空模板有可运行的 C++ 默认外观。一旦在 Designer 中添加根控件，就使用你手动摆放的布局。C++ 不再替你生成默认布局，此时需要完成下面的命名绑定。

## 二、制作 WBP_CommonDialogButton

1. 打开 `WBP_CommonDialogButton`，确认父类为 `TACommonDialogButtonWidget`。
2. 放置 Button，命名为 `Button_Option`，勾选 Is Variable。
3. 在 Button 内放置现有的 `WBP_ActionPrompt`，命名为 `ActionPrompt_Option`，勾选 Is Variable。它必须继承 `TAActionPromptWidget`。
4. 调整按钮背景、内边距、尺寸。按钮中的文字和按键图由 C++ 配置，不需要绑定 OnClicked 蓝图事件。
5. 编译并保存。

`ActionPrompt_Option` 同时显示动作名称和按键图。C++ 将其设为 Hit Test Invisible，让点击落在外层按钮上。这里直接复用现有统一按键提示系统，支持键鼠／手柄切换、重绑定后刷新、本地化，以及固定图标高度、按照原图片比例自动计算宽度。

若需要自制按键提示外观，创建继承 `TAActionPromptWidget` 的 WBP，并使用 `Image_Icon`、`Text_ActionName` 和包裹图标的 `SizeBox_Icon`。不要把图标宽高都强制设置成同一个固定值；`IconHeight` 决定高度，宽度由原图宽高比决定。

## 三、制作 WBP_CommonDialog

建议结构如下，实际位置由你决定：

```text
Overlay（铺满屏幕）
├─ Border（半透明遮罩，铺满，Visibility = Visible）
└─ SizeBox / Border（居中面板）
   └─ VerticalBox
      ├─ Text_Title
      ├─ Text_Message
      ├─ Text_Countdown
      └─ HorizontalBox / VerticalBox
         ├─ DialogOption_1
         ├─ DialogOption_2
         ├─ DialogOption_3
         └─ DialogOption_4
```

- `Text_Title`、`Text_Message`、`Text_Countdown` 均为 TextBlock，勾选 Is Variable。说明文字建议启用 Auto Wrap Text。
- 四个 `DialogOption_*` 均放入上一步制作的 `WBP_CommonDialogButton` 实例，并勾选 Is Variable。
- 编号表示**视觉顺序**。只有定义中使用的按钮会显示，未使用的按钮折叠。
- 遮罩必须能命中鼠标，避免空白区域的点击穿透。根容器可以 Self Hit Test Invisible，遮罩自身使用 Visible。
- 不要添加底部统一提示栏；按键提示都在各个按钮内。
- 倒计时使用真实时间，暂停游戏也会继续计时；未配置超时时折叠 `Text_Countdown`。
- 编译并保存。

主控制器会自动使用上述路径的 `WBP_CommonDialog`。如果需要替换模板，可在控制器蓝图 Class Defaults 中设置 `Common Dialog Widget Class`。`Common Dialog Input Mapping Context` 可指定弹窗 IMC；为空时加载默认 `IMC_CommonDialog`。

打包版本还需要收录通过运行时路径加载的资源。`Config/DefaultGame.ini` 的 `DirectoriesToAlwaysCook` 已包含 `/Game/Setting`、`/Game/Input` 和 `/Game/UI`，确保设置数据、弹窗蓝图、弹窗 IMC 与按键动作进入 Build。修改这些配置或 C++ 后需重新打包；已有 Build 不会自动更新。输入模式同步带有重入保护，缺失可选 IMC 时也不会重复触发提示刷新。

## 四、给设置菜单加应用按钮

打开现有 `WBP_SettingsMenu`：

1. 在你选定的位置添加 Button，命名 `Button_ApplySettings`，勾选 Is Variable。
2. 在按钮内添加 TextBlock，命名 `Text_ApplySettings`，勾选 Is Variable。
3. 不要自行绑定点击后应用设置；C++ 已处理点击、本地化及启用状态。没有草稿时按钮禁用。
4. 编译并保存。

按钮不是必需控件：即使暂未摆放，也能使用 IMC 中的应用动作，原提示栏会加入“应用设置”。旧的画面确认／还原按钮可以删除。

设置项标题使用 `Text_Name`。有草稿时 C++ 对该文字施加倾斜，默认 `Dirty Title Shear = -12` 度，因此中文字体没有 Italic 字体面也能显示斜体效果。它会保留你原本的文字变换，应用、放弃或改回已应用值后恢复。名称、选项值和右侧说明不会被一起倾斜。

## 五、玩家流程

### 调整与重置

开关、候选选择、滑块写入同一份设置草稿；同一个 Setting ID 出现在多个页面时仍是同一个值。长按恢复默认从当前分类收集设置 ID，保留语言、跳过子菜单，暂存默认值后立即通过 `ApplyDraftForSettings` 提交这一组项目；对应草稿清除，其他项目草稿保留。左右区域均可发起恢复，无需先进入中栏。滑块长按逻辑、重置长按动画保持有效；显示模式恢复仍须通过现有 Keep/Revert 弹窗确认，超时回退。

显示器／模式／比例／分辨率的候选项按草稿联动，调整时不改变窗口。实际运行、存档和游戏中的设置读取继续使用已应用值。收藏按稳定 Setting ID 即时保存，保存收藏不会保存草稿。

### 有修改时离开页面

切换页面、打开设置二级菜单或关闭设置等离开操作会先检查草稿。右侧 Back 返回左侧时只显示应用／放弃两项（A／B），完成后回到原分类；无草稿则直接返回。其他离开操作保留第三项取消：

| 按钮 | 输入槽 | 行为 |
|---|---:|---|
| 应用设置 | 1 | 应用草稿，然后继续离开；如果改了显示参数，先进入显示确认 |
| 放弃更改 | 2 | 丢弃草稿，然后继续离开 |
| 取消 | 3 | 保留草稿，留在当前页面 |

### 应用显示参数后

仅显示器、模式、比例和分辨率需要 15 秒确认：

| 按钮 | 输入槽 | 行为 |
|---|---:|---|
| 保留显示设置 | 1 | 保存新显示参数 |
| 还原显示设置 | 3 | 恢复应用前的显示参数 |

视觉上这是第一、第二个按钮；第二个按钮绑定输入槽 3，当前资产用 Escape／X 还原（按键由 IMC 决定，不由按钮位置决定）。倒计时结束等同于还原。其他已经成功应用的设置不还原。如果此前有离开操作，无论保留、还原还是超时，均继续离开；单独应用或恢复默认则留在原页面。

如果部分设置应用失败，会保留已成功应用的结果，未成功的内容继续作为草稿，并显示错误弹窗，停留在当前页面。

## 六、其他游戏页面如何调用

弹窗属于 `TACommonDialogSubsystem`，不依赖设置菜单。任意持有本地玩家的界面均可：

1. Get Local Player Subsystem，类型选择 `TACommonDialogSubsystem`。
2. 创建 `TACommonDialogDefinition`：填写 `TitleTextId`、`MessageTextId`，本地化 ID 使用现有 JSON 系统。
3. 填写 `Options` 数组，最多四项。每项有 `ResultId`、`LabelTextId`、`InputSlot`。
4. 调用 `ShowDialog`。
5. 检查返回控件非空，绑定其 `OnResult`；按 `ResultId` 执行业务操作。

`ResultId` 是业务标识，如 `Delete`、`Save`、`Cancel`；不充当本地化文本。`InputSlot` 为 1～4，对应弹窗专用 IMC 的四个动作。视觉顺序等于数组顺序，输入槽可以不连续，但不能重复；Result ID 也不能重复。

设置 `TimeoutSeconds > 0` 时，必须设置一个确实存在于 Options 中的 `TimeoutResultId`。不需要倒计时则保持 0。

`bAllowGamepadCursor` 默认 true，保留其他调用者的指针行为。设置弹窗声明 false：手柄隐藏并禁用虚拟光标，仍按 IMC 的 Slot 执行业务；真实键鼠显示指针并可点击。Cursor／Focus／InputMode 仍统一由 PC 应用，弹窗只声明需求。

同一本地玩家只允许一个活动弹窗，重复调用返回空，防止覆盖正在等待玩家选择的弹窗。正常选择或超时会先关闭弹窗并释放输入，再广播结果，因此结果回调中可以立即打开下一个弹窗。主动 `Dismiss` 只关闭，不广播业务结果；调用方销毁时应主动关闭它拥有的弹窗。

弹窗只接受自己的四个 IMC 动作，原设置 IMC 和共享 UI IMC 会暂时移除。关闭后重新裁决剩余 Request，并应用当前 winner 的 Presentation，不恢复历史焦点快照。四个动作都允许暂停时执行，鼠标可以直接点击按钮。

## 七、建议在编辑器中的验收

1. 修改开关、选择和滑块，确认标题倾斜、游戏效果未变；改回原值后倾斜消失。
2. 左栏选中分类后长按恢复默认，确认语言保留、结果立即生效且保存；退出／放弃其他更改后仍为默认值。确认其他分类草稿未被提交，并检查显示模式的保留／回退。
3. 切换页面分别验证应用、放弃、取消。
4. 在独立游戏窗口中修改显示参数，验证候选联动不改变窗口，应用后出现确认。
5. 同时修改普通设置与显示参数，按还原或等待超时，确认只还原显示参数。
6. 从离开确认进入显示确认，验证保留、还原、超时均继续原来的离开操作。
7. 切换键鼠与手柄，检查按钮内图标变化、宽高比和文字不被挤压；修改 IMC 后检查输入跟随映射。

真实显示器切换和全屏回退需要独立运行窗口验收，嵌入式 PIE 不会移动编辑器窗口。
