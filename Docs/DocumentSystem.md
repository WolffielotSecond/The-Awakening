# 文本系统

信件、笔记等可收集文本，以及围绕它们的日记本、文本预览、拾取提示、3D 预览台。日记本本轮交付与清理记录见 [Diary3DWrapUp.md](Diary3DWrapUp.md)。存档见 [SaveSystem.md](SaveSystem.md)；背包物品的交互菜单（检视、丢弃、拆分）见 [InventoryItemActions.md](InventoryItemActions.md)。

文本**不是物品**：不占背包格子、不能丢弃、不能堆叠。收集记录只由 `UTADocumentSubsystem` 持有，只存 `DocumentId`。

## 新建一份文本

1. 在 `Content/Document/` 下新建数据资产，类选 `TADocumentDefinition`（资产管理器扫描 `/Game/Document`，放在别处剧情条件和存档找不到）。
2. 填写字段：

   | 字段 | 说明 |
   |---|---|
   | `DocumentId` | 唯一 ID。存档、剧情条件 `HasDocument` / `ReadDocument`、`TADocList` 都用它，**定下后不要改** |
   | `TitleTextId` / `BodyTextId` | 标题、正文的本地化 ID，三种语言写进 `Content/Localization/*.json` |
   | `ScanDescriptionTextId` | 扫描悬停介绍。项目设置开着统一介绍时不用填（见下方配置） |
   | `Icon` | 拾取列表图标，可空（空时用物品默认图标） |
   | `InlineImages` | 正文插图：key → 图片 |
   | `Mesh` | 模型，静态网格体或骨骼网格体。空时显示占位薄片 |
   | `Presentation` | 可选的共用展示配置：预览基础朝向、纸张动画蓝图、取出／归位时长与取景留白。没有配置时保留原来的静态展示 |
   | `Paper Front Material` / `Paper Back Material` | 每份文本独立的正反材质，默认 `/Game/Models/Document/M_Document_Front` / `M_Document_Back`；分别覆盖模型的 `M_Paper_Front` / `M_Paper_Back` 命名槽（亦支持 `M_Document_Front` / `M_Document_Back` 槽）。场景、拾取、日记夹页和阅读共用；清空后使用模型原材质，不改共享模型资产，其他材质槽保留 |
   | `WorldTransform` / `PickupPreviewTransform` / `ReaderTransform` / `DiaryPageTransform` | 同一个模型在场景、拾取提示、文本预览（阅读 / 检视）、日记本页面中的偏移、旋转、缩放 |

3. 场景里放 `TAWorldDocument`（或它的蓝图子类），`Definition` 指向这份资产。它会出现在拾取列表里，可以被扫描。

### 正文插图

正文里写 `<img id="key"/>`，key 在 `InlineImages` 里查。建议单独一行。图片按原尺寸显示，宽于 `MaxImageWidth`（默认 560）时等比缩小；找不到时显示 `[image: key]` 并打 Warning。WBP 的 RichText 控件要在 Decorator Classes 里加 `TARichTextImageDecorator`。

### 折叠纸张（6767676767）

`DA_Document_67` 使用 `/Game/Models/Document/DA_PaperPresentation`，其中 `PreviewRotation` 为编辑器 XYZ **90 / 0 / 90**。这是纸模型自身的轴向修正，拾取提示、日记本与阅读器共同使用；三处数据资产变换仍可单独手调，不要重复添加这次轴向修正。`WorldTransform` 独立保留，用于场景摆放。

`ABP_Paper` 继承 `TAPaperAnimInstance`，用 Sequence Evaluator 显式采样 `SelectedAnimation` / `SampleTime`：Openness 为 0 时使用 `AS_Folded`，1 时使用 `AS_Opened`，中间使用 `AS_Fold`。原动画长 1 秒，打开到折叠，展开时反向采样；不循环播放过渡。动画与 UI 共用进度，在背包时停中仍然更新。

日记本中的纸作为真实网格体放在书页上，使用 `WBP_Diary` 的 `DocumentSize` 和 `RightDocumentPosition` 调整尺寸与位置，点击区域由文档的三维包围盒投影到书本画面。点击后复用书本相机与同一个文档组件：取出放大并转向相机 → 展开。返回时先消除玩家手动旋转 → 折叠 → 旋转并缩回书页位置，再恢复日记本输入。过渡时不能翻页、再次查看、转动模型或连续返回。强制关闭时跳过过渡并恢复书页，借用的预览台仍归日记本管理。阅读取景固定，不随折叠包围盒改变。

可调参数在 `DA_PaperPresentation`：`TravelDuration`（默认 0.35 秒）、`OrientDuration`（0.25 秒）、`FramingScale`（1.15）。折叠时长来自动画自身。从场景直接打开的普通文档不配置 Presentation 时仍使用原淡入淡出；从日记本打开则物理取出/归位，未配置纸张动画的文档保持原网格体，不会凭空获得折叠动画。

`FoldedDiaryRotation` 默认 90 度，表示书中折叠纸在画面内逆时针转 90 度、开口朝左；仅影响书中姿态和归位目标，不修改 `ReaderTransform` 或打开后的相机正面朝向。

编辑器命令 `-run=TAPaperSetup` 生成 ABP 和共用配置，并一次性迁移 67 文档原有的三处展示变换，保持有效朝向不重复叠加；已绑定的文档不会再次迁移。`-run=TAPaperSetup -Verify` 只验证，不保存资产。自动化测试：`TheAwakening.Document.Paper`。

本次进度（2026-10-20）：已生成并绑定动画与配置，用户确认纸页位置和朝向调整可行；编辑器编译通过，文档系统 11 项自动化测试通过。自动化验证包含实际骨骼姿态、展开前面对相机、逆时针旋转方向、共享预览台归位及强制关闭恢复，不等同于所有分辨率与手柄流程的人工验收；人工检查仍按下方关联清单执行。地面物品模型碰撞本次仅分析，尚未调整。

## 拾取：两条路

交互的那一帧检查玩家剧情槽里**有没有日记本**，之后两条路完全分开：

| | 没有日记本 | 有日记本 |
|---|---|---|
| 数据 | 只标记**已读**，不收集 | **收集**（追加到日记本末页，同时标记已读） |
| 场景 Actor | 保留，以后还能再拿起；拿到日记本后回来可以正式收集 | 立即销毁 |
| 表现 | 拿起动作（`Anim.Pickup`，关键帧隐藏模型）→ 时停并打开文本预览 → 关闭后放下动作（`Anim.PutDown`，关键帧显示模型） | 塞入日记本动作（`Anim.InsertPage`，只是表现，被打断也不影响收集）+ 拾取提示 |
| 阅读中死亡 | 已读旗标随回滚撤销，模型恢复 | — |

已收集的文本在读档后不再出现在场景里。

## 文本预览 `WBP_DocumentReader`

父类 `UTADocumentReaderWidget`，所有文本预览共用：场景拿起、日记本查看、拾取提示快速查看。

- 默认显示 3D 模型。键鼠在模型上按住拖动旋转，手柄用 `IA_DocumentRotate`。旋转轴固定，上下可无限翻转。
- 「显示转译」（`IA_DocumentTranslate`）在屏幕中央显示宽度为屏幕 1/3 的白色文本区，无灰黑底框。标题居中；正文两端对齐，通过扩展合法换行位置之间的间距排版，不拉伸文字；段落末行保持自然左对齐。含插图的行保留原布局。模型及背景随转译淡入而虚化、变暗，底部按键提示与转译文字保持清晰。再按一次淡出并恢复模型检视。显示时 `IA_ReaderScroll`（↑↓、右摇杆）和滚轮平滑滚动正文；隐藏时右摇杆改为旋转模型。
- 返回走统一的 `IA_UIBack`；「显示转译」「返回」提示可以点击；「旋转」提示只在手柄下显示。
- 手柄时隐藏光标，切回键鼠恢复。
- 普通文档打开 / 关闭淡入淡出；配置纸张动画的文档按取出、转正、展开／折叠、归位流程过渡（真实时间）。过渡期间返回和按钮无效。
- 从场景打开时自己申请时停（优先级 200）；从日记本打开时沿用日记本的时停。

控件命名：`Image_Model`、`Panel_Translation`（内含 `Text_Title`、`ScrollBox_Body` + `RichText_Body`）、`HorizontalBox_Controls`。转译与操作栏需为同一 CanvasPanel 的直接子项。C++ 在构建时兼容现有 WBP，将面板居中、移除底色，并创建 `Blur_TranslationBackground` / `Image_TranslationDim` 两个背景层；操作栏位于它们上方。旋转灵敏度、滚动速度、动画时长、正文字号都在 WBP 的 Class Defaults 里调；`Translation Blur Strength`（默认 12）、`Translation Dim Opacity`（默认 0.6）控制转译背景效果。

## 日记本 `WBP_Diary`

父类 `UTADiaryWidget`。日记本本身是一个剧情物品（项目设置 `DiaryItem`，样例 `DA_Item_Diary`），场景里有掉落物 `BP_WorldItem_Diary`。

- 打开：背包剧情格里的日记本 → 交互菜单「查看内容」。关闭回到背包。
- 按收集顺序一页一份；0 份是空书（显示 `Text_Empty`），0 / 1 份不能翻页。
- 翻页 `IA_DiaryPreviousPage` / `IA_DiaryNextPage`（默认 Q / E、LB / RB，PlayStation 对应 L1 / R1）。短按到首页/末页停止，不循环；长按达到 `PageHoldDuration`（默认 0.65 秒）跳到首页/末页。按住只触发一次，正在翻页时等当前动作完成，失去输入所有权取消长按；查看当前页文本用背包确认键或点击页面上的文本，打开文本预览，返回回到日记本。
- 退出时保存页码，下次打开回到这一页（页码也进存档）。
- 打开时读取剧情格内图标的实际中心和尺寸，转换到 ScaleBox 内的书本布局坐标，以合书投影范围对齐起点；书本可见地从图标放大到中央，同时翻开，背景和操作栏独立淡入，关闭时缩回去。3D 展示台播放真实开合和翻页，动画完成后才解锁操作和提交页码。
- 日记和文本阅读的键盘/手柄输入由 Enhanced Input 的 Action Started/Triggered/Completed/Canceled 驱动，映射来自 `IMC_UI`，长按从 Action 生命周期计时；旋转/滚动使用 Action 评估后的值。Slate 只把被 UI 截获的物理事件转交 PlayerInput，不执行按键命令。点击和拖动仍走指针交互。`-run=TADocumentUI -ConfigureDiaryInput` 备份并修正翻页默认映射为 Q/E、LB/RB，移除 A/D。
- 快速预览保留背包剧情栏日记图标的弱引用，首帧没有布局时等待，之后按实际几何更新返回位置及大小；Esc 返回日记后再关闭，会缩回该图标，分辨率/DPI 变化后也重新读取。
- 时停沿用背包的；手柄时隐藏光标。

底部操作栏包含上一页、下一页、长按回到首页、长按前往末页、查看、返回。两条长按说明复用对应翻页 Action 的实时按键图标，不作为立即跳页的点击按钮；0/1 份文本时四条翻页相关提示一起隐藏。简中、繁中、英文均已补齐。

控件命名：`Panel_Book`、`Image_Book`、`Panel_Page`、`Text_DocumentTitle`、`Text_PageNumber`、`Text_Empty`、`HorizontalBox_Controls`。

### 同场景夹页与阅读

`ATADiaryPreviewStage` 在预览槽位 1 中同时渲染书体、翻动纸和三个 `UTADiaryDocumentComponent`（左页、右页、移动页）。文档组件只包含网格体，没有独立相机或灯光。`Image_Book` 按 3:2 比例显示唯一的 RenderTarget；已移除 `Image_Document` 和固定位置的文档按钮。

- 每份文档仍占一个逻辑页，右侧显示当前可点击文档，左侧始终不显示静置文档；上一份数据仅保留用于反向翻页，不再首尾循环。
- 前翻页前先把下一份文档放在右页下层，旧右页文档跟随 `Document_Attach` 骨骼保持在翻动纸的原始正面，不再抬到左页上方；翻到背面后按面朝向遮住文档，落页时已不可见，避免结束瞬间消失。后翻页则从左侧带回文档，提前准备更早的左页文档。上下层留出间隙并平滑调整，避免共面闪烁；动画结束后提交页码并隐藏移动副本，不在结束时才生成目标文档。
- 稳定文档跟随 `Visible_Left` / `Visible_Right` 的姿态；翻页纸仍使用 Leader Pose 共享书体骨架。文档有自己的折叠动画，不与书本骨架混用。四个书本动画仍为 Book_Open、Book_Close、Page_Turn_Forward、Page_Turn_Backward。
- `Diary|Book` 调速度、俯视角、取景留白和书本变换；`Diary|Paper` 的 `DocumentSize`（厘米）与 `RightDocumentPosition`（书本模型坐标）调夹页尺寸及位置，重新打开日记后应用。
- 点右侧文档时，以书本 Image 的实际几何尺寸、相机投影和文档世界包围盒计算命中，支持 DPI 和 ScaleBox 缩放；左页不显示静置文档，当前阅读焦点保持在右侧。
- 阅读器复用书本 RenderTarget，显示位置与原书本画面一致。当前右页文档在同一个场景中移出、转向相机、展开；鼠标/手柄只旋转该文档，不旋转书本。关闭时先恢复手动旋转，再折叠并放回原位置。强制关闭恢复同一个组件，阅读器不销毁书本台。
- 实体冻结期间用展示帧时间推进；全局暂停仍停止。0/1 份不能翻页；快速查看直接采样展开姿态，在后续帧持续发布书体骨骼数据，等跨帧刷新和布局完成后启动阅读，防止首个快捷预览沿用初始化帧的骨骼渲染缓存。正常和强制移除日记会释放整个展示台，槽位 2 已不再用于日记。

默认文本（`DA_Document_Sample01`、`DA_Document_Sample02`）已统一为 67 文档的 `SK_A4_Paper_With_Fold`、`DA_PaperPresentation` 及四种展示变换。它们在书中保持折叠，取出后展开，支持旋转并折叠归位；场景和拾取预览也复用同一套纸张。各自的 ID、标题、正文、扫描介绍、图标和正文插图保留。纸面初始使用 6767676767 的默认正反材质；之后用每份 Document DataAsset 的 `Paper Front Material` / `Paper Back Material` 独立更换，不必复制模型，也不必修改共享网格材质。转译显示各文本自己的内容。未使用 Document_Extract / Document_Insert。

`-run=TAPaperSetup -UnifyDefaults` 可重复同步默认示例和使用空模型/兜底材质（M_MissingMaterialFallback）的文本；变更前备份至 `Saved/DocumentPresentationBackups/<时间>/Game/...`，独立定制模型不变。加 `-Verify` 只检查、不保存。新增文本可复制现有默认示例并修改 ID 与本地化字段，保留共享 Presentation；需要独立外观时再替换模型和贴图。

评估入口：PIE → 背包 → 日记本 →「查看内容」。没有日记本时用 `TAGiveDiary`，收集至少两份文档后测试前后翻页。`-run=TADocumentUI -RebuildDiaryOnly` 仅备份并重建日记布局。

离屏预览：`-run=TADocumentUI -PreviewDiary -AllowCommandletRendering`。加 `-DiaryDocument=<文档资产路径>`、`-DiaryNextDocument=<另一份文档资产路径>` 准备两份文档，`-DiaryTurn -DiarySample=0.5` 捕获前翻页中间帧，`-DiaryRead` 捕获同场景的展开阅读姿态，`-DiaryUnlit` 做无光照对照。`-DiaryQuick` 检查跳过开书动画的首帧，`-DiaryFromIcon -DiarySample=0` 检查图标尺寸起点，`-DiaryQuickReturn` 模拟图标首次布局后的快捷关闭终点（这些图标预览使用独立测试位置）。导出 `Saved/Diary3DPreview.png` 与书本原始捕获 `Diary3DBook.png`；独立空场景不代替具体关卡的 PIE 验收。

自动化验证：`TheAwakening.Document.Diary`，包括 EnhancedInput、Preview、PhysicalTurn、SharedReader、WidgetIntegration 及原页码测试。覆盖提前摆放目标、骨骼跟随、起落姿态连续、前后翻页、点击投影、固定相机、取出/展开/旋转/归位、快速打开、存档页码及强制清理。原 `TheAwakening.Document.Paper` 验证非日记入口的纸张阅读流程兼容性。

## 拾取提示 `WBP_PickupBanner`

父类 `UTAPickupBannerWidget`，由角色持有，捡到物品或收集文本时从屏幕右侧滑入。

- 时间线（真实时间，不受扫描时停影响）：滑入 0.5 秒 → 停留 → 滑出 0.5 秒，共 5 秒。
- 内容：上方顺时针旋转的 3D 模型，中间名称，下方「按 [背包键] 查看详细」。
- 文本：先显示标题 1.85 秒，淡出换成正文前 20 个字 + `...`（跳过图片标记），淡入后再停 1.85 秒。
- 显示期间又捡到东西：当前这条立刻滑出，下一条再滑入（排队）。
- **快速查看**：提示显示期间按背包键，提示立即隐藏，并直接打开：
  - 文本 → 日记本翻到这一页，跳过入场动画直接进入文本预览；返回 → 日记本 → 背包。
  - 日记本 → 日记本「查看内容」。
  - 其他物品 → 「检视」。
- 打开背包时提示被背包盖住；不接收输入、不申请时停。

时长、旋转速度（默认 180 度 / 秒）、预览字数都在 WBP 里调。

## 3D 预览台 `ATAPreviewStage`

把一个模型渲染到 RenderTarget 给 UI 显示。独立预览使用以下槽位；从日记本打开文档时，阅读器复用槽位 1 的整个书本场景，关闭后归还：

| 槽位 | 使用者 |
|---|---|
| 0 | 拾取提示 |
| 1 | 3D 日记书本、夹页与书中文档阅读 |
| 3 | 文本预览 |
| 4 | 物品检视 |
| 9 | 调试命令 `TAPreview` |

- 放在远离关卡的位置，灯光和模型只用光照通道 1，不照亮关卡也不被关卡照亮；曝光固定。
- 默认主光强度 2、颜色 `FFFEEEFF`，补光强度 0.5；透明模式下曝光补偿由 UI 显示材质计算。
- 不受时停影响（扫描、背包、阅读期间照常旋转），全局暂停时停止。
- 模型按包围球自动居中取景，再叠加资产里的展示变换。
- 默认透明背景：UI 的 Image 必须用 `GetDisplayMaterial()`（材质 `M_UI_PreviewStage`，由 `-run=TAPreviewMaterial` 生成），直接显示 RenderTarget 会看到反的透明度。
- 模型为空时显示占位立方体并打 Warning。

## 角色动作 `UTACharacterActionComponent`

拿起 / 放下 / 塞入日记本这些动作和 PaperZD 动画解耦：

1. `PlayAction(Tag)` 广播 `OnActionRequested`。
2. 动画蓝图要播这个动作，就在**同一帧内**调用 `AcceptAction`，需要的时刻调用 `NotifyActionKeyframe`（文本用 `HideWorldMesh` / `ShowWorldMesh`），播完调用 `NotifyActionFinished`。
3. 没人认领时按配置的 `FallbackDuration` 自动完成，开始时立即触发 `FallbackKeyframes`，所以没有动画也能跑通。

每种 Tag 可配置动作期间是否禁止移动 / 跑酷 / 交互、是否无敌。同一时间只有一个动作，死亡、复活时自动打断。

## 剧情条件

剧情编辑器的条件类型下拉里：

| 类型 | 满足条件 | 字段 |
|---|---|---|
| `HasDocument` | 已收集这份文本 | `Document` = DocumentId |
| `ReadDocument` | 读过这份文本（含没有日记本时拿起读过、没有收集的） | `Document` = DocumentId |

编辑器里的对话预览拿不到收集记录，这两种条件在预览中永远不满足，要在 PIE 里验证。

## 配置

项目设置 → Game → **The Awakening Documents**（`UTADocumentSettings`，存 `DefaultGame.ini`）：

| 设置 | 说明 |
|---|---|
| `DiaryItem` | 日记本物品，剧情槽里有它就走收集路线 |
| `bUseUnifiedScanDescription` | 开着时所有文本扫描都显示同一句介绍（当前开着） |
| `UnifiedScanDescriptionTextId` | 统一介绍的本地化 ID，默认 `Doc_Scan_Unified` |
| `ReaderWidgetClass` | 文本预览界面类 |

项目设置 → Game → **The Awakening Items**（`UTAItemSettings`）：物品、文本、衣服没设置图标时的默认图标（`/Game/UI/Common/T_DefaultItemIcon`），模型材质丢失时的兜底材质（`/Game/Materials/Common/M_MissingMaterialFallback`）。运行时兜底，不改资产。

## 输入

动作资产在 `Content/Input/Actions/UI/`，按键在 `IMC_UI` 里配置：`IA_DocumentTranslate`、`IA_DocumentRotate`（Axis2D）、`IA_ReaderScroll`（Axis1D）、`IA_DiaryPreviousPage`、`IA_DiaryNextPage`。返回统一走 `IA_UIBack`。

## 生成资源

### 打包依赖与初始化失败处理

日记阅读使用 `/Game/Models/Diary` 下的 `SK_Journal_Body`、`SK_TurningPage`、共享骨架及
`Book_Open`、`Book_Close`、`Page_Turn_Forward`、`Page_Turn_Backward`。这些资源由原生代码的软路径加载，
场景拾取物引用的 `SM_Diary_Closed` 不会把骨骼模型和动画带入 Cook。`DefaultGame.ini` 已将
`/Game/Models/Diary` 加入 `DirectoriesToAlwaysCook`；预览材质仍由既有 `/Game/UI` Cook 目录覆盖。
该目录整体参与 Cook（包括保留的源包资产），实际播放的书本动画仍只有上述四个。

快捷预览与背包「查看内容」共用打开流程：先加载校验日记模型、动画和预览材质，再加入视口。
失败时返回空指针并销毁预览 Actor，不显示模态层；控件构造失败则在加入视口后统一移除，
避免在 `NativeConstruct` 内提前移除但随后又被挂上视口。缺失资源会记录具体路径。
等待图标布局期间可直接返回；已失效的预览台也可关闭。正常书本播放期间仍维持原有输入限制。

2026-10-24 排查时，旧 Windows Cook 目录只有合书静态模型及其材质，缺少阅读所需的两个骨骼模型及四个动画。
修正配置后的实际 Windows Cook 已包含这些资源及骨架、翻动纸材质，Cook 完成 0 错误、0 警告。
证据：`Saved/Logs/DiaryPackagingCook.log`。更新代码和配置后需重新打包；已有的 StagedBuilds/导出包不会自动更新。
产物核对清单：`Saved/DiaryPackagingCookVerification.json`。Editor 编译通过，日记组 6/6 通过（其中输入测试有一条 `UIInputMappingContext is not available` 警告）；
覆盖正常打开/关闭、等待图标时取消、两个入口缺资源时拒绝显示及 Actor 清理。报告 `Saved/Automation/DiaryPackaging/`，日志 `Saved/Logs/DiaryPackagingTests.log`。
本次未重新导出整包或执行打包游戏的视觉验收，需按打包回归清单复测。

### 生成命令

界面和样例都由命令行工具生成，已有资源默认不覆盖：

| 命令 | 生成 |
|---|---|
| `-run=TAPreviewMaterial` | `M_UI_PreviewStage` |
| `-run=TADocumentUI` | 输入动作和所有 WBP。加 `-Rebuild`（阅读）、`-RebuildDiary`、`-RebuildBanner`、`-RebuildMenu`、`-RebuildQuantity`、`-RebuildInspect` 重建对应 WBP，旧的备份到 `Saved/LayoutBackups` |
| `-run=TADocumentSamples` | 日记本物品、样例文本、日记本掉落物、样例插图 |
| `-run=TAPaperSetup` | 纸张 ABP、共用展示配置，以及 67 文档一次性朝向迁移；`-Verify` 只验证不保存 |

## 调试命令

| 命令 | 作用 |
|---|---|
| `TAGiveDiary` | 把日记本放进剧情槽 |
| `TADocList` | 列出已收集（按页序）和已读的文本 |
| `TADocReset` | 清空收集、已读和页码（已销毁的场景文本不会回来，重开 PIE 即可） |
| `TAPreview [路径 \| off]` | 屏幕左上角显示预览台画面 |
| `TAAction [Tag]` | 播放角色动作，不填时打断 |
