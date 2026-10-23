# 设置菜单：从零创建资产与接线

> 新增的“草稿→应用”、离开确认、显示回退和可供全游戏复用的弹窗，按 [通用弹窗与设置应用搭建流程](CommonDialogSystem.md) 接线。下文旧的画面确认／还原按钮不再使用；普通调整暂存草稿。当前分类恢复默认是独立提交，完成长按后立即应用并保存，显示模式仍须确认。

游戏→常规新增布尔项 `Game.ToggleSprint`（点按切换疾跑），默认关闭。该项统一控制键盘和手柄，沿用草稿、应用、保存、恢复默认及收藏。已有 `DA_Settings` 可使用 `Tools/Settings/add_toggle_sprint_setting.py` 增量加入，不要重新生成并覆盖已编辑的定义。运行时切换规则见 [移动与持续跑酷输入](MovementAndParkour.md)。

按本文顺序在 Unreal 编辑器中新建数据资产和 Widget Blueprint，最后接入现有暂停菜单。C++ 负责生成页面、子标题、选项、输入交互与实际效果；你在 UMG 中设计布局和样式。

## 1. 准备目录与创建顺序

完成项目编译并重启 Unreal 编辑器，确认可以找到本文列出的 C++ 父类。建议使用以下目录；后面的路径示例按此填写：

```text
Content/Setting/        数据资产，对应 /Game/Setting
Content/UI/Setting/     设置界面 WBP，对应 /Game/UI/Setting
```

需要创建 5 个数据资产、12 个 WBP。按下表顺序创建，后面的步骤会引用前面的资产：

| 顺序 | 资产 | 类型／父类 | 建议目录 |
| --- | --- | --- | --- |
| 1 | DA_SettingsPages | TASettingsPageDefinitionAsset | Setting |
| 2 | WBP_SettingsPage | TASettingsPageWidget | UI/Setting |
| 3 | WBP_SettingsSection | TASettingsSectionWidget | UI/Setting |
| 4 | WBP_SettingToggle | TAToggleSettingRowWidget | UI/Setting |
| 5 | WBP_SettingChoice | TAChoiceSettingRowWidget | UI/Setting |
| 6 | v | TASliderSettingRowWidget | UI/Setting |
| 7 | WBP_SettingSubmenu | TASubmenuSettingRowWidget | UI/Setting |
| 8 | DA_MenuBrightness | TASettingsMenuDefinitionAsset | Setting |
| 9 | DA_MenuKeyboardBindings | TASettingsMenuDefinitionAsset | Setting |
| 10 | DA_MenuControllerBindings | TASettingsMenuDefinitionAsset | Setting |
| 11 | WBP_SettingsBrightness | TABrightnessMenuWidget | UI/Setting |
| 12 | WBP_SettingsKeyboardBindings | TAKeyBindingsMenuWidget | UI/Setting |
| 13 | WBP_SettingsControllerBindings | TAKeyBindingsMenuWidget | UI/Setting |
| 13a | WBP_SettingsDescriptionBlock | TASettingsDescriptionBlockWidget | UI/Setting |
| 14 | WBP_SettingsMenu | TASettingsMenuWidget | UI/Setting |
| 15 | DA_Settings | TASettingsDefinitionAsset | Setting |
| 16 | WBP_AdvancedData | TAAdvancedDataWidget | UI/Setting |

统一按键提示使用项目已有的 `/Game/UI/WBP_ActionPrompt`，无需新建提示系统。

创建 Data Asset：内容浏览器右键，选择“杂项／Miscellaneous → 数据资产／Data Asset”，在类列表中选择表中的类型。

创建 WBP：新建 Widget Blueprint 时选择表中的父类。若先创建成了普通 UserWidget，可在蓝图编辑器通过“文件 → 重新设置父类／Reparent Blueprint”选择指定类。

每个 WBP 完成后执行“编译”和“保存”。绑定控件的名称必须完全一致，并勾选 **Is Variable**。

## 2. 创建页面与子标题数据

在 `/Game/Setting` 创建 `DA_SettingsPages`，类型为 `TASettingsPageDefinitionAsset`。

打开资产，在 Details 中点击 **Populate Default Pages**，生成五个普通页面及子标题。该按钮用于首次生成数据，之后在数组中编辑内容即可。

| PageId | 显示名称 | SortOrder |
| --- | --- | --- |
| Settings.Page.Game | 游戏 | 0 |
| Settings.Page.Display | 显示 | 10 |
| Settings.Page.Audio | 声音 | 20 |
| Settings.Page.MouseKeyboard | 鼠标与键盘 | 30 |
| Settings.Page.Controller | 控制器 | 40 |

展开每个页面的 `Sections`，检查以下默认子标题：

| 页面 | SectionId | 显示名称 | SortOrder |
| --- | --- | --- | --- |
| 游戏 | Settings.Section.Game.General | 常规 | 0 |
| 显示 | Settings.Section.Display.Video | 画面 | 0 |
| 显示 | Settings.Section.Display.Information | 信息显示 | 10 |
| 声音 | Settings.Section.Audio.Volume | 音量 | 0 |
| 鼠标与键盘 | Settings.Section.MouseKeyboard.Camera | 相机 | 0 |
| 鼠标与键盘 | Settings.Section.MouseKeyboard.Bindings | 按键绑定 | 10 |
| 控制器 | Settings.Section.Controller.Camera | 相机 | 0 |
| 控制器 | Settings.Section.Controller.Bindings | 按钮绑定 | 10 |
| 控制器 | Settings.Section.Controller.Cursor | 菜单光标 | 20 |

PageId 同时是页面标识与本地化键；SectionId 同时是子标题标识与本地化键。两者属于不同对象，不需要填写相同字符串。以上默认名称已经包含在项目的三份本地化文件中。

SortOrder 小的排在前面，相同数字按数组顺序排列。页面和子标题各自独立排序。PageId 全局唯一，SectionId 在所属页面内唯一；请保持 ID 与本地化键的大小写一致。

**不要在此资产中添加收藏。** 收藏由 C++ 自动创建，始终是第一项，也不会出现在设置归属勾选菜单中。`Settings.Page.Favorites` 和 `Favorites` 是保留 ID。

保存 DA_SettingsPages。

## 3. 创建左侧页面入口模板

创建 `WBP_SettingsPage`，父类为 `TASettingsPageWidget`。

在 Designer 中设计一个页面按钮，添加：

| 控件名称 | 类型 | 要求 |
| --- | --- | --- |
| Button_Page | Button | 必需 |
| Text_Name | TextBlock | 必需，作为按钮文字 |
| Image_Highlight | Image | 可选，选中标识 |

布局可使用 SizeBox、Overlay 等普通 UMG 容器。C++ 会填入页面名称、绑定点击和更新选中状态。

需要额外选中动画时，实现 `OnSelectionChanged` 事件。收藏入口也使用这个模板，不需要额外摆放收藏按钮。

编译并保存。

## 4. 创建子标题模板

创建 `WBP_SettingsSection`，父类为 `TASettingsSectionWidget`。

添加必需的 TextBlock，命名为 **Text_Title**。手动设计字体、上下间距、分隔线或背景。

这个模板用于右侧分组标题：普通页面显示 SectionId 的名称，收藏页显示所属大页面的名称。标题不参与设置项的键盘／控制器导航。

编译并保存。

## 5. 创建四种设置行模板

分别创建 `WBP_SettingToggle`、`WBP_SettingChoice`、`WBP_SettingSlider`、`WBP_SettingSubmenu`，父类按第 1 步表格选择。

每个模板都必须包含：

| 控件名称 | 类型 | 用途 |
| --- | --- | --- |
| Button_Option | Button | 整行确认／激活 |
| Text_Name | TextBlock | 设置名称 |

再按类型添加建议控件：

| 模板 | 建议添加 |
| --- | --- |
| Toggle | Image_ToggleThumb、Image_ToggleBackground、Button_Favorite、Image_Favorite、Image_Highlight；Text_Value 可选 |
| Choice | Text_Value、Button_Decrease、Button_Increase、Button_Favorite、Image_Favorite、Image_Highlight |
| Slider | Text_Value、Slider_Value、Button_Decrease、Button_Increase、Button_Favorite、Image_Favorite、Image_Highlight |
| Submenu | Text_Value、Button_Favorite、Image_Favorite、Image_Highlight，可设计进入箭头 |

控件类型：`Text_Value` 为 TextBlock；`Button_...` 为 Button；`Image_...` 为 Image；`Slider_Value` 为 Slider。

C++ 自动填写名称与数值，绑定增减、拖动、收藏和确认事件。无需另写 OnClicked 或 OnValueChanged。Slider_Value 的范围由 C++ 设置为 1–100。

WBP_SettingSubmenu 还会用于重绑定菜单的动作行：Text_Value 显示当前按键，收藏控件会自动隐藏。

四个模板分别编译并保存。

### 开关外观：圆点左右对齐与开启背景

在 `WBP_SettingToggle` 的 `Button_Option` 内容中，设置名称左对齐，右侧放一个固定尺寸的 SizeBox（例如 52 × 32）。SizeBox 中添加 Overlay，按以下顺序放置三个 Image：

```text
SizeBox
└─ Overlay
   ├─ Image_ToggleTrack    常驻底图，名称自定
   ├─ Image_ToggleBackground    仅开启时显示的背景，名称必须一致
   └─ Image_ToggleThumb   圆点，名称必须一致
```

- `Image_ToggleTrack`：放置关闭状态底图，水平、垂直均填充，始终显示。
- `Image_ToggleBackground`：放置开启状态背景图，水平、垂直均填充。C++ 在值为 1 时显示，值为 0 时隐藏（Hidden）。不修改图片颜色。
- `Image_ToggleThumb`：必须直接放在 Overlay 下，使其 Slot 为 Overlay Slot。圆点 Brush 的 Image Size 可设为 28 × 28，垂直居中，左右 Padding 各为 2；初始水平左对齐。C++ 在关闭时改为 Left，开启时改为 Right，不使用位移或动画。
- 所有装饰 Image 均设为 Not Hit-Testable（Self & All Children），让点击落到 Button_Option。圆点不要再套 SizeBox，否则 C++ 无法直接修改其 Overlay Slot 的水平对齐。

无需蓝图绑定点击或刷新事件。菜单首次打开、鼠标点击、键盘／控制器调整、恢复默认及同一设置的其他显示位置，都根据当前整数值刷新外观。旧的圆点位移动画和颜色动画不要再绑定或播放。

### 收藏星星：两张图片切换

在每个需要收藏功能的设置行中，添加 `Button_Favorite`（Button），将 `Image_Favorite`（Image）放入这个按钮内部。收藏按钮与 `Button_Option` 并列放置，不要嵌套在整行按钮中。

1. 导入两张尺寸一致、背景透明的星星图片：未收藏使用空心星，已收藏使用实心星。
2. 将 `Button_Favorite` 的 Normal、Hovered、Pressed、Disabled 背景设为透明；设置 Content Padding 为 0。按钮保持 Visible，图片设为 Not Hit-Testable（Self & All Children），点击由外层按钮接收。
3. 用 SizeBox 设置点击区域，例如 40 × 40；图片居中，例如 24 × 24。位置由你在蓝图中摆放。
4. 在行蓝图的 Class Defaults → Settings → Favorite 中，设置 `FavoriteIconTexture` 为已收藏图片，`NotFavoriteIconTexture` 为未收藏图片。两项都必须填写。
5. 编译、保存。无需在蓝图中绑定 OnClicked，也无需编写图片切换逻辑。

C++ 根据收藏状态切换图片；点击星星直接切换收藏，不触发设置项自身的开关或进入操作。取消收藏后显示未收藏图片；在收藏页面取消时，该设置会从收藏列表移除。不可收藏的设置会隐藏收藏按钮。四种设置行都支持同样的配置。

未同时指定两张图片的旧模板保留原来的表现：只在已收藏时显示 Image_Favorite。

## 6. 创建三个二级菜单定义资产

在 `/Game/Setting` 创建以下三个 Data Asset，类型均为 `TASettingsMenuDefinitionAsset`：

| 资产 | MenuId | TitleTextId | MenuKind |
| --- | --- | --- | --- |
| DA_MenuBrightness | Menu.Brightness | Settings.Display.BrightnessMenu.Name | Brightness |
| DA_MenuKeyboardBindings | Menu.KeyBindings | Settings.MouseKeyboard.KeyBindingsMenu.Name | KeyBindings |
| DA_MenuControllerBindings | Menu.ControllerKeyBindings | Settings.Controller.KeyBindingsMenu.Name | ControllerKeyBindings |

填写 `SettingIds`：

- DA_MenuBrightness：添加一个元素 `Display.Brightness`。
- DA_MenuKeyboardBindings：保持空数组。
- DA_MenuControllerBindings：保持空数组。

亮度菜单通过 SettingIds 生成滑块；重绑定菜单从实际输入映射生成动作列表。

保存三个资产。

## 7. 创建亮度二级菜单 WBP

创建 `WBP_SettingsBrightness`，父类为 `TABrightnessMenuWidget`。

必需控件：`Box_SettingsOptions`，类型为 VerticalBox。

可选控件：`Text_Title`、`Text_Description`、`HorizontalBox_Controls`、`Button_RestoreDefaults`。其中两个 Text 为 TextBlock，提示栏为 HorizontalBox，恢复默认按钮为 Button。

手动摆放暗、中、亮校准图案。二级菜单不需要左侧 Box_Pages。

在 **Class Defaults** 中填写：

| 属性 | 值 |
| --- | --- |
| DefaultMenuDefinition | DA_MenuBrightness |
| RowWidgetClasses.Toggle | WBP_SettingToggle |
| RowWidgetClasses.Choice | WBP_SettingChoice |
| RowWidgetClasses.Slider | WBP_SettingSlider |
| RowWidgetClasses.Submenu | WBP_SettingSubmenu |
| ActionPromptWidgetClass | WBP_ActionPrompt |

以下两个重绑定 WBP 和主菜单也需要配置同一份 RowWidgetClasses 与 ActionPromptWidgetClass。每个 WBP 独立保存这些默认属性。

编译并保存。

## 8. 创建键鼠与控制器重绑定 WBP

创建 `WBP_SettingsKeyboardBindings`，父类为 `TAKeyBindingsMenuWidget`。

| 控件名称 | 类型 | 要求／用途 |
| --- | --- | --- |
| Box_SettingsOptions | VerticalBox | 必需，动作列表 |
| Text_Title | TextBlock | 可选，标题 |
| Text_Description | TextBlock | 可选，说明 |
| Text_CaptureStatus | TextBlock | 建议，显示等待输入、冲突与结果 |
| Button_CancelCapture | Button | 建议，取消捕获 |
| Button_RestoreDefaults | Button | 建议，恢复当前设备默认绑定 |
| HorizontalBox_Controls | HorizontalBox | 建议，统一按键提示 |

Class Defaults：配置第 7 步的四种行模板和 WBP_ActionPrompt；设置 `DefaultMenuDefinition = DA_MenuKeyboardBindings`。

编译并保存，然后复制此资产为 `WBP_SettingsControllerBindings`。打开副本，将 DefaultMenuDefinition 改为 `DA_MenuControllerBindings`，编译并保存。

两个菜单的布局可以相同。点击动作行后开始捕获输入；Escape 或控制器 Menu 键取消。恢复默认只影响该菜单对应的设备。

## 9. 创建主设置菜单 WBP

创建 `WBP_SettingsMenu`，父类为 `TASettingsMenuWidget`。

主菜单使用左、中、右三栏：左栏为页面导航，中栏为动态设置选项，右栏为当前选项的标题和说明（支持图片）。底部放统一按键提示与操作按钮。

建议布局：

```text
CanvasPanel
└─ VerticalBox
   ├─ Text_Title                         整个菜单的标题
   ├─ HorizontalBox                     三栏主体
   │  ├─ SizeBox                        左栏：手动设置宽度
   │  │  └─ ScrollBox
   │  │     └─ Box_Pages
   │  ├─ SizeBox                        中栏：手动设置宽度或 Fill
   │  │  └─ ScrollBox
   │  │     └─ Box_SettingsOptions
   │  └─ VerticalBox                    右栏：剩余宽度 Fill
   │     ├─ Text_SettingTitle
   │     └─ ScrollBox
   │        └─ VerticalBox
   │           ├─ Text_Description      可选：兼容纯文字说明
   │           └─ Box_Description       动态生成图文内容块
   ├─ 操作按钮区域
   └─ HorizontalBox_Controls
```

主体与 ScrollBox 需要在有限高度中铺满，才能正常滚动。宽度、边距、背景和分隔线由你手动设计。

| 控件名称 | 类型 | 要求／用途 |
| --- | --- | --- |
| Box_Pages | VerticalBox | 主菜单必需，左侧动态入口 |
| Box_SettingsOptions | VerticalBox | 必需，中栏动态子标题与设置行 |
| ScrollBox_SettingsOptions | ScrollBox | 建议，中栏滚动区域 |
| ScrollBox_Description | ScrollBox | 建议，右栏说明滚动区域 |
| Text_Title | TextBlock | 可选，主标题 |
| Text_SettingTitle | TextBlock | 建议，右栏当前设置名称 |
| Box_Description | VerticalBox | 图文说明必需，右栏动态内容块 |
| Text_Description | TextBlock | 可选，未配置图文模板时显示原有纯文字说明 |
| Text_Empty | TextBlock | 建议，空页提示 |
| HorizontalBox_Controls | HorizontalBox | 建议，统一按键提示 |
| Button_RestoreDefaults | Button | 建议，恢复当前页默认值 |
| Text_RestoreDefaults | TextBlock | 可选，放在恢复默认按钮内；C++ 自动本地化 |
| Button_ConfirmVideoMode | Button | 建议，确认显示设置 |
| Button_RevertVideoMode | Button | 建议，撤销显示设置 |

Box_Pages、Box_SettingsOptions、Box_Description 和 HorizontalBox_Controls 保持为空，由 C++ 添加内容。Text_SettingTitle 与 Text_Title 用途不同，不要交换名称。

### 创建图文说明块模板

创建 `WBP_SettingsDescriptionBlock`，父类为 `TASettingsDescriptionBlockWidget`，必须包含 `Text_Content`（TextBlock）和 `Image_Content`（Image）。按以下层级搭建，让图片占满右栏可用宽度，高度随原图比例自动变化：

```text
VerticalBox
├─ Text_Content
└─ ScaleBox_Image          ScaleBox，名称必须一致
   └─ Image_Content
```

- Text_Content：开启 Auto Wrap Text，设计字体、颜色、行间距与上下留白。
- ScaleBox_Image：VerticalBox Slot 的 Size 设为 Auto，Horizontal Alignment 设为 Fill，Vertical Alignment 设为 Top。Stretch 设为 Scale To Fit X，Stretch Direction 设为 Both；C++ 也会确保这两个缩放属性正确。
- Image_Content：ScaleBox Slot 的 Horizontal / Vertical Alignment 均设为 Fill，Brush Draw As 设为 Image，Color and Opacity 保持白色、不透明。运行时 C++ 使用每张纹理的原始尺寸作为 Brush Image Size，提供准确比例，无需手填宽高。
- 右栏、Box_Description 和模板在父容器中的水平对齐均需为 Fill，才能获得整栏宽度；图片会填满扣除 Padding 后的可用宽度。图片高度 = 可用宽度 × 原图高度 ÷ 原图宽度。
- 不要在图片外添加固定 Height Override，也不要用 Scale To Fill（会裁剪）或 Fill（可能拉伸）。横图、竖图可以混用；很高的图片通过右栏 ScrollBox 滚动查看。
- C++ 每个块只显示文字或图片其中一种；文字块会同时折叠 ScaleBox_Image，图片块会折叠 Text_Content，不留下图片容器空白。
- 所有内容为展示控件，不接收点击；不需要蓝图刷新事件。

保存模板，再在主菜单 Class Defaults 中指定 DescriptionBlockWidgetClass。

在 DA_Settings 的每个设置中，按需要编辑 `DescriptionBlocks` 数组：

| 块 Type | 字段 | 用途 |
| --- | --- | --- |
| Text | TextId | 填写现有本地化系统中的文字 ID |
| Image | Image | 选择导入的 Texture2D 图片 |

数组顺序就是显示顺序，例如：Text（说明）→ Image（示意图）→ Text（补充说明）。图片无需填写 TextId；切换语言时所有文字块自动刷新。此处是独立段落／图片块，不是把图片嵌入同一行文字。

DescriptionBlocks 为空时，图文区自动使用原有 DescriptionTextId 生成一个文字块；非空时使用数组替代原有纯文字说明。无选项的页面会清空右栏标题和内容，避免显示上个页面的信息。

在 Class Defaults 中填写：

| 属性 | 值 |
| --- | --- |
| PageWidgetClass | WBP_SettingsPage |
| SectionWidgetClass | WBP_SettingsSection |
| DescriptionBlockWidgetClass | WBP_SettingsDescriptionBlock |
| RowWidgetClasses.Toggle | WBP_SettingToggle |
| RowWidgetClasses.Choice | WBP_SettingChoice |
| RowWidgetClasses.Slider | WBP_SettingSlider |
| RowWidgetClasses.Submenu | WBP_SettingSubmenu |
| ActionPromptWidgetClass | WBP_ActionPrompt |
| DefaultMenuDefinition | 留空 |
| OptionWidgetClass | 可留空，作为通用行模板备用 |

主菜单的页面数据由后面创建的 DA_Settings 提供。

编译并保存。

## 10. 创建完整设置定义资产

在 `/Game/Setting` 创建 `DA_Settings`，类型为 `TASettingsDefinitionAsset`。

1. 设置 **PageDefinitionAsset = DA_SettingsPages**。
2. 点击 **Populate Default Definitions**，生成当前支持的设置、默认参数和归属。
3. 展开 Definitions，检查生成的条目。
4. 设置第 11 步的三个二级菜单引用，再保存。

Populate Default Definitions 会重置 Definitions 及其目标 WBP 引用。首次创建时使用一次，后续直接编辑数组。

默认分组：

| 页面／子标题 | SettingId |
| --- | --- |
| 游戏／常规 | Game.Language、Game.DialogueTextSpeed |
| 显示／画面 | Display.Monitor、Display.WindowMode、Display.AspectRatio、Display.Resolution、Display.VSync、Display.FrameRateLimit、Display.BrightnessMenu |
| 显示／信息显示 | Display.AdvancedData |
| 鼠标与键盘／相机 | MouseKeyboard.CameraSensitivity、MouseKeyboard.InvertCameraX、MouseKeyboard.InvertCameraY |
| 鼠标与键盘／按键绑定 | MouseKeyboard.KeyBindingsMenu |
| 控制器／相机 | Controller.CameraSensitivity、Controller.InvertCameraX、Controller.InvertCameraY |
| 控制器／按钮绑定 | Controller.KeyBindingsMenu |
| 控制器／菜单光标 | Controller.MenuCursorSpeed |

另有 `Display.Brightness`，它的 bSubmenuOnly=true，只在亮度二级菜单显示。声音页暂时为空，等待 Wwise 音量接口。当前不包含自动推进／跳过对话、FOV、震动强度和提示设备设置。

## 11. 为三个入口指定目标 WBP

在 DA_Settings 中找到以下 Submenu 条目，逐一填写：

| SettingId | TargetMenuWidgetClass | TargetMenuDefinition |
| --- | --- | --- |
| Display.BrightnessMenu | WBP_SettingsBrightness | DA_MenuBrightness |
| MouseKeyboard.KeyBindingsMenu | WBP_SettingsKeyboardBindings | DA_MenuKeyboardBindings |
| Controller.KeyBindingsMenu | WBP_SettingsControllerBindings | DA_MenuControllerBindings |

点击入口时直接创建 TargetMenuWidgetClass。TargetMenuDefinition 提供该菜单的内容与设备分类，并优先于 WBP 的 DefaultMenuDefinition。

保存 DA_Settings。三个入口都必须指定 WBP 类才能打开。

## 12. 检查两级勾选归属

在 DA_Settings 的 Definitions 中展开一个设置，找到 **Locations**：

1. 点击下拉按钮，第一层显示 DA_SettingsPages 中的普通页面。
2. 展开页面，第二层显示该页面的子标题复选框。
3. 勾选需要的子标题，可跨页面多选。
4. 页面内的全选复选框可选中或清除该页全部子标题。
5. 保存资产。

例如 Game.Language 已属于“游戏 → 常规”。若希望它也出现在另一页面，给同一条定义增加归属即可，不要复制第二个 Game.Language。

设置身份始终是 SettingId，多个位置共享同一值与收藏状态。没有任何归属的普通设置不会出现在主页面中。子标题内选项按 Definitions 数组顺序排列。

收藏由玩家交互添加：它始终是第一页面，其子标题是所属大页面名称。多页面设置会显示在对应的多个收藏分组中；同一大页面分组内只显示一次，空分组隐藏。

## 13. 配置启动加载与暂停菜单入口

打开项目文件 `Config/DefaultGame.ini`，添加或更新以下内容：

```ini
[TheAwakening.PlayerSettings]
DefinitionAsset=/Game/Setting/DA_Settings.DA_Settings
```

如果资产放在不同目录，请替换成实际对象路径。这里是数据资产对象路径，不是 Windows 文件路径，也不是 WBP 的 _C 类路径。

打开项目实际使用的玩家控制器蓝图。当前项目已有 `/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController`，在 Class Defaults 中设置：

```text
SettingsMenuWidgetClass = WBP_SettingsMenu
```

确认当前关卡使用的 GameMode 选中了配置好的玩家控制器。现有暂停菜单“设置”入口已经由 C++ 接入，WBP 无需额外创建菜单或实现暂停逻辑。

进入设置后，C++ 会折叠暂停菜单，使上一页不再显示或接收鼠标点击；关闭设置后恢复暂停菜单原本的可见状态与输入。切换期间游戏保持暂停，暂停菜单原有页面状态保留。无需在蓝图里额外 Remove From Parent 或关闭暂停。

停止当前 PIE，保存资产与配置后重新开始游戏，让本地玩家子系统加载 DA_Settings。

## 14. 设置手动按钮的本地化文字

页面、子标题、设置名称、说明、候选值和统一按键提示由 C++ 填充。恢复默认按钮中的 TextBlock 命名为 `Text_RestoreDefaults`，C++ 会在打开菜单和切换语言时自动填充 `Settings.RestoreDefaults`，无需蓝图绑定 Text 或单独刷新。

恢复当前页默认值会跳过语言设置（Game.Language 或使用该 ApplyHandlerId 的设置）。即使语言出现在收藏页，也会保留当前语言；其余本页设置照常恢复。

其他手动创建的静态按钮标签需要使用现有本地化系统：

| 按钮 | 传给 GetText 的 ID |
| --- | --- |
| 确认显示设置 | Settings.ConfirmVideo |
| 撤销显示设置 | Settings.RevertVideo |
| 取消输入捕获 | Settings.CancelCapture |

建议在包含这些按钮的 WBP 中创建一个 `RefreshStaticTexts` 函数：获取 GameInstance 的 TALocalizeSubsystem，调用 GetText，将结果设置到相应 TextBlock。

在 Construct 中调用该函数，并绑定 OnLanguageChanged 再调用它；在 Destruct 中解除自己绑定的事件。这样切换语言时，静态按钮文字也会同步刷新。

无需为动态页面入口、子标题或设置行再写一次文字刷新逻辑。

## 15. 创建并放置高级数据显示

创建 `WBP_AdvancedData`，父类为 `TAAdvancedDataWidget`。添加必需 TextBlock：**Text_AdvancedData**，勾选 Is Variable，编译并保存。

本地玩家控制器会在 BeginPlay 自动创建一次信息面板，指定自己作为 Owning Player，再以层级 100 加入该玩家的屏幕；退出时自动移除。默认使用现有 `/Game/UI/WBP_AdvancedData`。无需在 HUD 或蓝图中重复 Create Widget / Add to Viewport，否则会显示两份。

可在玩家控制器蓝图的 Class Defaults → UI → Advanced Data 中修改 **Advanced Data Widget Class**，选择其他继承 `TAAdvancedDataWidget` 的 WBP。屏幕位置、锚点、字体和背景仍在该 WBP 中自行设计。面板自动设置为不参与鼠标命中，不接管输入或焦点。

保留控件拥有者 Tick。关闭模式由 C++ 清空文字，默认每 0.25 秒刷新；不要因关闭模式将整个拥有者 Collapse 并停止更新。

Display.AdvancedData 的三个候选项：

| 保存下标 | 模式 | 显示 |
| --- | --- | --- |
| 0 | Off | 空文字 |
| 1 | Basic | 当前帧率（帧率上限）FPS |
| 2 | Full | 当前帧率（帧率上限）FPS-游戏版本-构建ID-画质-语言-输入设备 |

游戏版本和构建 ID 统一通过 UTAVersionLibrary 读取。ProjectVersion 是唯一游戏版本来源；构建 ID 由版本管理脚本在打包前生成，日常编辑器开发为 local。完整流程见 [版本号管理](VersionManagement.md)。DefaultGame.ini 中的配置示例：

```ini
[TheAwakening.Build]
BuildId=local
SaveFormatVersion=1
```

## 16. 检查打包目录

DA_Settings 通过配置字符串加载。到 Project Settings → Packaging → Additional Asset Directories to Cook，添加：

```text
/Game/Setting
/Game/UI/Setting
```

若采用其他目录，填写实际目录。这样数据资产和设置 WBP 会进入打包内容。

## 17. 第一次运行验收

1. 打开暂停菜单，点击“设置”，确认出现新主菜单。
2. 左侧第一项是收藏，其后是游戏、显示、声音、鼠标与键盘、控制器。首次没有收藏时，收藏页为空属于正常行为。
3. 切换普通页面，确认子标题和对应设置行正确出现；手柄上一项／下一项跳过子标题，键鼠使用悬停选中。
4. 调整开关、选择项、滑块，确认显示和实际效果一致。
5. 收藏几个不同页面的设置，确认收藏页按大页面分组；取消收藏后同步移除。
6. 打开亮度、键鼠绑定、控制器绑定三个菜单，检查返回能恢复父菜单输入。
7. 检查重绑定捕获、冲突提示、取消、恢复默认和统一按键提示更新。
8. 切换语言，确认页面、子标题、选项、说明和手动按钮标签都刷新。
9. 修改显示模式／分辨率，确认按钮出现；确认、撤销和 15 秒超时回退均正确。
10. 测试高级数据显示的三个模式，退出并重新进入，确认值和收藏保存。
11. 在 Standalone 或打包版本中验证真实窗口模式与分辨率变化。

保存的默认值仅在没有有效保存值时使用。重新创建资产不会清空已有玩家设置；需要检查默认效果时，可使用当前页的“恢复默认”。

## 18. 参数参考与后续扩展

普通设置统一保存整数：Toggle 为 0／1，Choice 为从 0 开始的数组下标，Slider 为 1–100。Submenu 不保存数值，WBP 引用属于定义资产。

滑块字段：Step 是整数位置的步长；Minimum／Maximum 是实际参数范围；PhysicalStep 是实际舍入步长，0 表示不舍入；DisplayFormat、DecimalPlaces、UnitTextId 控制实际参数显示。

换算：`实际值 = Minimum + (整数值 - 1) / 99 × (Maximum - Minimum)`，随后按 PhysicalStep 舍入。端点 1、100 对应实际最小值、最大值。多个整数位置可能对应同一实际值。

| 设置 | 整数默认值 | 实际范围 | PhysicalStep | 默认实际值 |
| --- | --- | --- | --- | --- |
| Game.DialogueTextSpeed | 34 | 10–100 | 5 | 40 字／秒 |
| Display.Brightness | 60 | 1–3 | 0.1 | Gamma 2.2 |
| 两种 CameraSensitivity | 32 | 0.1–3 | 0.1 | 1 倍 |
| Controller.MenuCursorSpeed | 43 | 0.25–2 | 0.05 | 1 倍 |

Choice 默认候选数组：

| 设置 | 数组顺序 | 默认下标 |
| --- | --- | --- |
| Game.Language | zh-CN、zh-TW、en | 0 |
| Display.WindowMode | Fullscreen、WindowedFullscreen、Windowed | 1 |
| Display.FrameRateLimit | 30、60、90、120、144、165、240、Unlimited | 1 |
| Display.AdvancedData | Off、Basic、Full | 0 |

每个候选含 Value 与 NameTextId；保存下标，实际效果使用 Value。显示屏、宽高比和分辨率由动态 Provider 生成，随当前显示屏与显示模式刷新，详见第 22 节。

新增页面时，在 DA_SettingsPages 增加 PageId 和 Sections，再在三个本地化 JSON 中加入对应键，然后使用 Locations 勾选设置归属。例如：

```json
"Settings.Page.Accessibility": "辅助功能",
"Settings.Section.Accessibility.General": "常规"
```

同样的键需添加到 zh-CN、zh-TW、en 三份文件。页面与子标题无需修改 C++ 枚举；新增实际设置效果仍需对应 C++ 应用逻辑。

已发布的 Choice 数组重排会改变保存下标的含义。页面排序、子标题排序和归属调整不改变 SettingId 对应的值。

## 19. 常见接线问题

| 现象 | 检查 |
| --- | --- |
| 找不到父类或生成默认数据按钮 | 当前原生代码是否已编译，编辑器是否已重启 |
| 左侧没有入口 | Box_Pages 名称、控件类型、PageWidgetClass |
| 右侧没有选项 | Box_SettingsOptions、RowWidgetClasses、DA_Settings 启动路径、Locations |
| 没有子标题 | SectionWidgetClass、WBP_SettingsSection 的 Text_Title |
| 二级菜单打不开 | 该设置的 TargetMenuWidgetClass 是否填写 |
| 重绑定菜单显示错误设备 | MenuKind 和 TargetMenuDefinition／DefaultMenuDefinition |
| 按键提示栏为空 | HorizontalBox_Controls、ActionPromptWidgetClass 是否配置 |
| 文本显示 ID | 三份本地化文件中是否存在完全一致的键 |
| 自定义配置未生效 | 输出日志中的无效定义、重复 ID 或失效页面／子标题引用提示 |
| Locations 出现 Missing | 对应页面／子标题被删除或改名，取消失效项并重新勾选 |

最终资产布局和编辑器两级勾选交互需要在 Unreal 中人工验收。

## 20. 当前已创建的测试设置（暂不使用二级菜单）

`/Game/Setting/DA_Settings` 已创建，初始 12 项测试设置已增量加入 4 项显示设置；启动配置已指向它，玩家控制器 BP_ThirdPersonPlayerController 的 SettingsMenuWidgetClass 已指定 WBP_SettingsMenu。

| 页面 | 测试设置 |
| --- | --- |
| 游戏 | 语言（Choice）、对话文字速度（Slider） |
| 显示 | 使用的显示屏、显示模式、宽高比、分辨率、帧率上限、高级数据显示模式（Choice），垂直同步（Toggle） |
| 鼠标与键盘 | 相机灵敏度（Slider）、反转相机 X / Y（Toggle） |
| 控制器 | 相机灵敏度（Slider）、反转相机 X / Y（Toggle）、菜单光标速度（Slider） |

声音页保持空页，收藏页由系统生成，没有添加二级菜单入口。这批数据用于验证普通设置行、显示参数联动、收藏和三栏说明。

鼠标与键盘的相机灵敏度说明包含“文字 → 16:9 横图 → 补充文字 → 9:16 竖图”；控制器的相机灵敏度说明包含竖图。两张测试纹理位于 `/Game/Setting/TestImages`，用于检查图片是否始终占满右栏宽度且保持比例。

重新打开编辑器后运行 PIE，从暂停菜单打开设置。首次进入收藏页可能为空，请先点击其他页面。依次检查：

1. 开关的圆点左右对齐和开启背景显示／隐藏。
2. 选择项的左右切换和文字更新；切换语言时，页面、选项、标题与说明同步更新。
3. 滑块拖动、箭头调整、实际数值格式以及恢复默认。
4. 收藏星星切换、收藏页面出现／移除选项、重新进入游戏后保存状态保留。
5. 两个灵敏度项目的右栏图片比例和滚动；切换到无图片项目或空页后，没有残留图片或标题。

高级数据显示的实际 HUD 效果还需要放置 WBP_AdvancedData；对话文字速度通过下一次对话检查。已有玩家保存值不会因这批资产创建而重置。

创建脚本保存在 `Tools/Settings/create_settings_test_assets.py`，再次运行会拒绝覆盖已经存在的 DA_Settings。后续请直接编辑这个 DA 的 Definitions。

## 21. 设置菜单输入与交互（当前规则）

设置菜单使用独立 `/Game/Input/IMC_Settings`，操作资产位于 `/Game/Input/Actions/Settings`。玩家控制器 SettingsInputMappingContext 默认指向该 IMC；设置打开时以优先级 20 启用，退出设置后停用。共享 IMC_UI 保留用于其他菜单，C++ 不再运行时创建设置按键映射。按键修改和重绑定后的路由、统一按键提示均读取实际 IMC 映射。

| 操作／Action 名称后缀 | 默认键鼠 | 默认手柄 |
| --- | --- | --- |
| PreviousPage／NextPage | Q／E（当前资产；可在 IMC 改绑） | 不配置 |
| PreviousItem／NextItem | 不配置 | 十字键上／下 |
| AdjustLeft／AdjustRight | 左／右箭头 | 十字键左／右 |
| Confirm | 不配置 | A／底部面键 |
| Favorite | F | X／左侧面键 |
| ResetPage | R | Y／顶部面键 |
| Back | Esc | B／右侧面键 |
| ScrollUp／ScrollDown | 不配置，鼠标使用滚轮 | RS 上／下 |
| 原生指针操作 | 鼠标移动、左键及滚轮 | 禁用虚拟光标；LS／摇杆按下不执行指针行为 |

手柄按键文字以 Xbox 命名展示，其他设备的提示由现有统一系统转换。可以直接在 IMC_Settings 编辑这些默认映射。PreviousItem、NextItem、Confirm、ScrollUp、ScrollDown 为手柄操作，给它们添加键盘键也不会启用键鼠确认或项目导航。资产中原有 PointerMove／PointerClick 映射保留，但设置菜单不再读取或执行它们。

提示栏随当前选中项目变化：开关、子菜单不显示减少／增加，手柄显示确认；选择、滑块不显示确认。选择的 AdjustLeft／AdjustRight 提示文字为“上一项／下一项”，滑块仍为“减少／增加”。这里选择候选的上一项／下一项仍使用 AdjustLeft／AdjustRight，手柄切换设置行的 PreviousItem／NextItem 独立保留。键鼠不显示确认或设置行导航；指针移动、点击不占用提示栏；手柄按当前区域显示 RS 上／下滚动设置列表或说明提示；目标没有实际可滚动内容时隐藏，键鼠始终隐藏。

主设置界面为键鼠和手柄显示“长按3秒重置当前页面”。`IA_SettingsResetPage` 默认绑定 R／Y，可在 IMC_Settings 编辑，Action 保持 Boolean，无需添加 Hold Trigger。左侧选中分类后即可长按，中栏也可使用，不必先按 A。持续按住对应按键或鼠标按住 Button_RestoreDefaults 满 3 秒才触发，每次长按只重置一次；短按及单击提示栏不会重置。只重置当前页面包含的非子菜单设置，始终保留语言；收藏页只重置其中设置，不清空收藏。恢复结果通过 ApplyDraftForSettings 单独应用并保存，对应草稿清除；其他项目的草稿不提交。之后放弃其他更改不会撤销已完成的恢复。显示模式恢复仍走 Keep/Revert 与 15 秒超时回退。切换页面、进入二级菜单或失去输入权限会取消当前长按，需要松开重新按住；二级菜单不显示或接受该页面操作。光标显隐继续按当前输入设备决定。

重置按钮进度由 C++ 驱动，不需要创建蓝图 Widget Animation。优先使用菜单蓝图中名为 ProgressBar_ResetHold 的 Progress Bar（勾选 Is Variable，并编译保存蓝图），保留它的颜色、Brush、填充样式、Padding 与布局；C++ 只更新 Percent，并设置为不拦截点击。手动控件的 Bar Fill Type 请设为 Left to Right。找不到控件时，系统才自动在 Button_RestoreDefaults 内包裹 Overlay：底层是左到右填充的 ProgressBar_ResetHold，上层保留原有内容和文字，填充覆盖按钮内部全宽。Settings／Style 中的 ResetHoldFillColor 只控制自动生成版本。自定义外观时把进度层放在按钮背景之上、文字之下，横纵 Fill、Padding=0；不要放在文字上方或添加不透明的前景背景遮挡进度。

长按进度线性从 0 到 1，松开后以 `释放进度 × (1 - t)³` 在 1 秒内回到 0，先快后慢。回退途中再次按住时，不清零显示：回退继续，新长按从 0 重新计时，显示两条进度的较大值。旧回退进度不会抵扣新的 3 秒确认时间。

恢复默认完成后立即从满进度播放同样的 1 秒回退动画，无需松开按键。继续按住不会重新填充或再次触发；中途松开不会重启动画，必须重新按住满 3 秒才能再次重置。

上述 3 秒和 1 秒是默认值。打开 WBP_SettingsMenu 的 Class Defaults，在 Settings／Reset 中调整 Reset Hold Duration（ResetHoldDuration，长按确认秒数）和 Reset Return Duration（ResetReturnDuration，回退动画秒数），两项最小值均为 0.01 秒。按键提示中的长按秒数会自动读取 ResetHoldDuration，并随语言切换刷新；无需修改本地化文本或 IMC。三份 UI_Settings_ResetPage 文本中的 `{0}` 是时长占位符，请保留。

填充仍有空隙时，检查 Button 的 Style／Normal Padding 和 Pressed Padding；它们会与内容 Slot 的 Padding 相加。自动生成版本由 C++ 将这些 Padding 清零并使用 Scale 填充及无透明边缘的纯色 Brush。手动提供进度条时请自行设置相关 Padding 与布局；若 Fill Image 纹理自身带透明边缘，请换成无透明边缘的图片或纯色 Brush。

滑块支持长按 IMC 中的 AdjustLeft／AdjustRight：按下立即调整一次，持续按住 0.35 秒后每 0.08 秒调整一次，使用设置定义的 Step，并限制在 1–100。键盘和手柄均支持；松开、切换选中项、失去输入权限或同时按住两个方向时停止连续调整。选择类型只在按下时切换一次，不连续循环。

| 类型 | 大按钮点击（真实鼠标） | 手柄确认 | 减少／增加 |
| --- | --- | --- | --- |
| Toggle | 切换开关 | 切换当前开关 | 无效 |
| Choice | 只选中，不改值 | 不改值 | 切换当前候选；也可点箭头 |
| Slider | 只选中，不改值 | 不改值 | 调整当前值；也可点箭头或拖动 |
| Submenu | 打开目标菜单 | 打开当前目标菜单 | 无效 |

所有设置行取消按钮按下／悬停的默认深色效果，保留 Normal 外观。行选中使用 Image_Highlight，建议四种行模板都包含这个控件；左侧页面按钮则由 C++ 将当前页背景设为蓝色，其余页恢复原背景色。不要在蓝图点击事件里另行改变按钮颜色或设置值。

输入设备切换为手柄时立即隐藏鼠标并关闭悬停选择，保持原来选中的分类或设置项。设置菜单禁用虚拟光标消费者，LS／摇杆按下不能移动或点击隐藏指针。切回键鼠时立即显示鼠标，由 PC 将光标放到当前选中分类或设置行的中心；之后继续支持真实鼠标悬停、点击与滚轮。暂停菜单采用相同设备切换规则，手柄通过原有 IMC_UI 上下导航、确认和返回；切回键鼠时光标回到选中按钮。

上一项／下一项选择中栏设置行，自动将目标行滚入可见区域；子标题不参与选择。减少、增加和收藏作用于当前选中行。悬停用于选中，点击大按钮还会执行表中的对应行为。

手柄 ScrollUp／ScrollDown（当前 IMC_Settings：RS 上／下）按当前操作区域选择目标：左侧分类栏操作时滚动中间设置列表；A 进入设置项区域后滚动右侧当前选中项的说明；B 返回分类后目标切回中间列表。没有实际溢出的目标不滚动，也不会回退滚动另一栏。滚动不要求指针悬停，不改变选中项；持续偏转连续滚动，松开停止。建议将两个 ScrollBox 命名为 `ScrollBox_SettingsOptions` 和 `ScrollBox_Description`；旧模板未命名时，C++ 会沿 Box_SettingsOptions、Box_Description（或旧 Text_Description）的父容器查找最近的 ScrollBox，无需重新摆放布局。

RS 操作先修改受范围约束的目标偏移，再使用 FInterpTo 平滑趋近；原有首次步进与长按重复时序保留。WBP_SettingsMenu 的 Settings／Scroll → ScrollInterpolationSpeed 默认 12，可调滚动响应速度。切换区域、输入设备或失去授权时停止旧目标的插值，不修改物理 Held observation。

设置的应用／放弃、错误和显示确认弹窗通过通用 Definition 的 `bAllowGamepadCursor=false` 同样禁用手柄指针；A/B 仍由 IMC_CommonDialog 对应 Slot 决定。真实鼠标可继续点击，切回键鼠时回到首个弹窗选项。其他调用者未声明此要求的通用弹窗保持原有指针行为。

运行检查：键鼠切到手柄时指针立即消失且选择保留，LS 不移动隐藏指针；切回鼠标时指针落在选中项上，随后悬停、点击正常。暂停菜单同样检查。左侧分类栏中拨动 RS，检查只平滑滚动中间设置列表；A 进入右侧选项并选中长说明设置（例如鼠标相机灵敏度），拨动 RS 检查只平滑滚动说明及上下边界；B 返回后检查恢复列表滚动，短内容不显示滚动提示；将 IMC 的一个按键改到其他键，检查旧键失效、新键和提示同步生效。

## 22. 显示屏、显示模式、宽高比与分辨率

四项均使用现有 WBP_SettingChoice，已加入 `/Game/Setting/DA_Settings` 的显示／画面子标题，无需新增设置行蓝图。

| SettingId | 名称 | ChoiceProviderId／候选 | ApplyPolicy |
| --- | --- | --- | --- |
| Display.Monitor | 使用的显示屏 | DisplayMonitors：连接设备；显示“序号 — 设备名称” | VideoMode |
| Display.WindowMode | 显示模式 | Fullscreen、WindowedFullscreen、Windowed | VideoMode |
| Display.AspectRatio | 宽高比 | DisplayAspectRatios：从所选设备和模式的可用分辨率归纳 | VideoMode |
| Display.Resolution | 分辨率 | ScreenResolutions：按当前宽高比筛选 | VideoMode |

存储仍为 Choice 数组下标。显示设置实际值以 GameUserSettings 和游戏窗口为准，运行时将设备 ID、模式、宽高比和分辨率重新映射到当前候选下标，避免把旧设备的下标直接套到新列表。显示屏选择通过窗口位置保存；确认前不持久化待定显示值。Windows 使用每个显示屏自己的系统显示模式列表，普通窗口模式补充不超过该桌面尺寸的常用窗口分辨率。

切换显示屏尽量保留当前宽高比，再选择最接近当前像素数量的可用分辨率；目标显示屏不支持该宽高比时回落到接近目标桌面的分辨率。切换宽高比会筛选分辨率并同时应用最接近当前像素数量的候选。宽高比控制输出尺寸的比例，不是黑边或相机裁剪设置。21:9 包含常见的 2560×1080、3440×1440 等超宽尺寸。

无边框全屏固定使用所选显示屏的桌面尺寸，因此宽高比、分辨率各只有一个候选。Choice 只有一个候选时左右箭头禁用；单显示屏设备的显示屏选择也一样。打开设置、改变显示参数，以及设置界面每秒刷新时都会同步动态候选；手动移动窗口到其他屏幕后也会更新选中显示屏。

改变上述四项后沿用 15 秒确认机制：`Button_ConfirmVideoMode` 保留，`Button_RevertVideoMode` 回退；超时或关闭主设置菜单也回退。快照包含分辨率、模式、显示屏和窗口位置，多次连续修改共用修改前的快照。回退时原显示屏若已断开，会把窗口放到当前主显示屏。确认／回退按钮仍由你在蓝图中摆放，名称和类型保持为 Button。

实际窗口移动和显示模式切换只针对 Standalone Game／打包游戏的游戏窗口，不移动编辑器窗口或嵌入式 PIE。请用 Standalone Game 验收：

1. 切换显示屏，确认游戏窗口移到目标屏；分别点击保留、回退，并等待 15 秒验证超时恢复。
2. 切换全屏、无边框全屏和窗口，核对候选值与实际窗口尺寸。
3. 在全屏或窗口下选择另一宽高比，核对分辨率过滤和实际输出变化。
4. 确认后退出并重新启动游戏，核对窗口位置、模式和分辨率；测试只有一块显示屏或断开原显示屏后的回落行为。

新增硬件应用逻辑位于 TADisplaySettingsService；定义仍在 DA，当前值与确认快照仍由 TASettingsSubsystem 管理，UI 和输入继续使用现有设置行、IMC 与统一提示系统。增量资产脚本为 `Tools/Settings/add_display_settings_assets.py`，仅补充缺失项，不替换已编辑定义。

## 设置手柄两区域导航（当前实现）

所有操作键由 `IMC_Settings` / `IMC_CommonDialog` 的 resolved Action mappings 决定；菜单不判断具体物理按键名称。

- 主菜单默认选中左侧收藏分类。左侧 PreviousItem / NextItem（默认十字键上下）切换分类；Confirm（A）进入右侧。LB/RB 不再绑定分类翻页。
- 右侧上下选择设置项，左右调整选择或滑块；Confirm 保留开关和子菜单操作。Favorite（X）收藏/取消收藏。ResetPage（Y）在左侧分类和右侧设置区域均可长按 3 秒，独立应用并保存当前页默认设置，保留语言；左侧不处理调整与收藏操作。
- 右侧 Back（B）在无草稿时直接返回左侧原分类；有草稿时使用现有通用弹窗，Slot 1（A）应用、Slot 2（B，当前实际 IMC）放弃，完成后返回左侧。显示模式修改仍保留原 Keep/Revert 确认流程，处理完成才返回。
- 左侧 Back 关闭设置。键鼠原有翻页、悬停、点击、应用/退出流程保留；二级菜单退出返回父菜单右侧。
- 两区域是 Widget 内的业务选择状态，不增加 Router 模式或额外 Slate Focus 写入。所有操作仍须通过当前 owner、精确 handle 和 capability 授权。
- 提示栏按当前区域与设置项生成，支持简中、繁中、英文。设备切到手柄立即关闭指针及悬停选行；Confirm 只执行当前区域的导航或业务行为，不合成指针点击。
- 键位录入的取消键改为 `IA_SettingsCancelCapture`（默认 Esc / Start），可编辑 IMC，不再硬编码物理取消键。录入中的任意新按键属于待绑定数据，不能先由旧 mapping 限制。

自动化覆盖区域切换、草稿应用/放弃、干净返回、覆盖 owner 拒绝、IMC 改绑、弹窗 A/B slot 解析；交互式 PIE 需验证真实高亮、弹窗、显示确认及混用指针。

### 本轮收尾与验证（2026-10-11）

- Editor Development 编译、`git diff --check` 通过。只读资产验证确认当前 IMC、弹窗 A/B/X/Y Slot 和 Widget 模板引用正确，命令退出 0；日志 `Saved/Logs/SettingsWrapUpAssetVerify.log`。
- 最近一次 42 项回归：30 项 clean success、11 项带 warning success、1 项既有 `TheAwakening.Puzzle.Scope` failure；报告 `Saved/Automation/SettingsTwoAreaRegression/index.json`。不是全量全部通过。
- 全量 43 项曾被旧 `Settings.EnglishLayout` 夹具空按钮读取阻断（`TASettingsEnglishLayoutTests.cpp:106`）。该测试未删除或弱化，后续 42 项运行明确未包含它；需要单独适配当前资产。Puzzle.Scope 继续按已登记延期事项处理。
- MenuInput / CommonDialog / Drafts 覆盖两区域导航、IMC 改绑、覆盖 owner 拒绝、弹窗 Slot、RS 插值与目标取消、设置／暂停禁用虚拟指针、选中控件回位目标、左栏长按恢复、短按无效、单次长按只执行一次、配置落盘、放弃不撤销恢复、其他分类及显示草稿不提交、语言保留。
- 用户允许本段收尾；自动化不证明真实鼠标回位、高亮和插值手感。真实窗口中的关闭／重开、设备混用及显示模式 Keep/Revert/超时没有本轮逐项人工 PASS 记录，不将其写为已验证。
- 已移除本轮一次性 IMC 迁移脚本；正式创建／验证工具保留，创建工具不再生成无消费者的 PointerMove／PointerClick。既有资产与映射不删除，避免扩大到序列化引用迁移；设置运行路径不消费它们。未清空 Saved／Intermediate／DDC，未创建 commit。

### 左摇杆切换大类栏与设置项栏

设置主界面新增两个暂停时可执行的 IMC 动作：

| 动作 | 默认手柄绑定 | 操作 |
| --- | --- | --- |
| IA_SettingsFocusCategories | Gamepad Left Thumbstick Left | 从设置项栏返回大类栏 |
| IA_SettingsFocusDetails | Gamepad Left Thumbstick Right | 从大类栏进入当前大类的设置项 |

两者位于 `/Game/Input/Actions/Settings`，映射在现有 `/Game/Input/IMC_Settings` 中。可直接编辑 IMC 改绑；代码没有固定物理键兜底。已有项目可使用 `Tools/Settings/add_settings_column_input.py` 增量添加，保留其他输入映射及已有这两个动作的自定义绑定。

返回大类保留当前大类和选中设置项；再次进入时继续选中之前的设置项。左拨复用现有返回确认流程：当前有未应用修改时，先选择应用或放弃，随后返回大类。右拨只进入设置项栏，不确认开关、不修改选择或滑块、不打开二级菜单。

同一次持续拨住只处理一次，重复按下事件不会再次执行。大类栏的上一项／下一项继续选择大类，设置项栏继续选择设置项。手柄提示栏按区域显示对应的摇杆方向提示；二级菜单和输入录入时不执行这两个主界面动作。弹窗拥有输入时，底层设置菜单不能切换栏位。

### 设置项列表滚动与进入提示

“进入设置项”的提示不再显示，包括大类栏中的 A 确认提示和左摇杆右拨提示；它们的 IMC 操作仍保留。设置项中的开关／二级菜单确认提示继续按选中项类型显示。

右摇杆上下在大类栏和设置项栏均滚动中间的 `ScrollBox_SettingsOptions`。长按连续滚动、平滑插值、松开停止以及列表没有溢出时隐藏滚动提示的规则一致；不会把列表滚动变为选择上一项／下一项，也不会调整设置值。右侧说明栏不再是这两个右摇杆滚动动作的目标。
