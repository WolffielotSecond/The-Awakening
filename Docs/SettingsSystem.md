# 设置菜单：从零创建资产与接线

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
| Text_Title | TextBlock | 可选，主标题 |
| Text_SettingTitle | TextBlock | 建议，右栏当前设置名称 |
| Box_Description | VerticalBox | 图文说明必需，右栏动态内容块 |
| Text_Description | TextBlock | 可选，未配置图文模板时显示原有纯文字说明 |
| Text_Empty | TextBlock | 建议，空页提示 |
| HorizontalBox_Controls | HorizontalBox | 建议，统一按键提示 |
| Button_RestoreDefaults | Button | 建议，恢复当前页默认值 |
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
| 显示／画面 | Display.WindowMode、Display.Resolution、Display.VSync、Display.FrameRateLimit、Display.BrightnessMenu |
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

停止当前 PIE，保存资产与配置后重新开始游戏，让本地玩家子系统加载 DA_Settings。

## 14. 设置手动按钮的本地化文字

页面、子标题、设置名称、说明、候选值和统一按键提示由 C++ 填充。你手动创建的静态按钮标签需要使用现有本地化系统：

| 按钮 | 传给 GetText 的 ID |
| --- | --- |
| 恢复默认 | Settings.RestoreDefaults |
| 确认显示设置 | Settings.ConfirmVideo |
| 撤销显示设置 | Settings.RevertVideo |
| 取消输入捕获 | Settings.CancelCapture |

建议在包含这些按钮的 WBP 中创建一个 `RefreshStaticTexts` 函数：获取 GameInstance 的 TALocalizeSubsystem，调用 GetText，将结果设置到相应 TextBlock。

在 Construct 中调用该函数，并绑定 OnLanguageChanged 再调用它；在 Destruct 中解除自己绑定的事件。这样切换语言时，静态按钮文字也会同步刷新。

无需为动态页面入口、子标题或设置行再写一次文字刷新逻辑。

## 15. 创建并放置高级数据显示

创建 `WBP_AdvancedData`，父类为 `TAAdvancedDataWidget`。添加必需 TextBlock：**Text_AdvancedData**，勾选 Is Variable，编译并保存。

将它放入现有 HUD，由你设计屏幕位置。HUD 应由本地玩家控制器创建，确保该控件能够取得 Owning Player。也可以在现有 HUD 初始化处创建一次 WBP_AdvancedData，指定本地玩家控制器作为 Owning Player，再加入视口。

保留控件拥有者 Tick。关闭模式由 C++ 清空文字，默认每 0.25 秒刷新；不要因关闭模式将整个拥有者 Collapse 并停止更新。

Display.AdvancedData 的三个候选项：

| 保存下标 | 模式 | 显示 |
| --- | --- | --- |
| 0 | Off | 空文字 |
| 1 | Basic | 当前帧率（帧率上限）FPS |
| 2 | Full | 当前帧率（帧率上限）FPS-游戏版本-构建ID-画质-语言-输入设备 |

游戏版本来自 ProjectVersion。构建 ID 可在 DefaultGame.ini 配置：

```ini
[TheAwakening.Build]
BuildId=你的构建ID
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
3. 切换普通页面，确认子标题和对应设置行正确出现，键盘／控制器导航跳过子标题。
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

每个候选含 Value 与 NameTextId；保存下标，实际效果使用 Value。分辨率由 ChoiceProviderId=ScreenResolutions 在启动时生成。

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

`/Game/Setting/DA_Settings` 已创建，包含 12 个实际设置；启动配置已指向它，玩家控制器 BP_ThirdPersonPlayerController 的 SettingsMenuWidgetClass 已指定 WBP_SettingsMenu。

| 页面 | 测试设置 |
| --- | --- |
| 游戏 | 语言（Choice）、对话文字速度（Slider） |
| 显示 | 垂直同步（Toggle）、帧率上限（Choice）、高级数据显示模式（Choice） |
| 鼠标与键盘 | 相机灵敏度（Slider）、反转相机 X / Y（Toggle） |
| 控制器 | 相机灵敏度（Slider）、反转相机 X / Y（Toggle）、菜单光标速度（Slider） |

声音页保持空页，收藏页由系统生成。没有添加二级菜单入口，也没有窗口模式与分辨率项目；这批数据用于先验证普通设置行、收藏和三栏说明。

鼠标与键盘的相机灵敏度说明包含“文字 → 16:9 横图 → 补充文字 → 9:16 竖图”；控制器的相机灵敏度说明包含竖图。两张测试纹理位于 `/Game/Setting/TestImages`，用于检查图片是否始终占满右栏宽度且保持比例。

重新打开编辑器后运行 PIE，从暂停菜单打开设置。首次进入收藏页可能为空，请先点击其他页面。依次检查：

1. 开关的圆点左右对齐和开启背景显示／隐藏。
2. 选择项的左右切换和文字更新；切换语言时，页面、选项、标题与说明同步更新。
3. 滑块拖动、箭头调整、实际数值格式以及恢复默认。
4. 收藏星星切换、收藏页面出现／移除选项、重新进入游戏后保存状态保留。
5. 两个灵敏度项目的右栏图片比例和滚动；切换到无图片项目或空页后，没有残留图片或标题。

高级数据显示的实际 HUD 效果还需要放置 WBP_AdvancedData；对话文字速度通过下一次对话检查。已有玩家保存值不会因这批资产创建而重置。

创建脚本保存在 `Tools/Settings/create_settings_test_assets.py`，再次运行会拒绝覆盖已经存在的 DA_Settings。后续请直接编辑这个 DA 的 Definitions。
