# 文本系统

信件、笔记等可收集文本，以及围绕它们的日记本、文本预览、拾取提示、3D 预览台。存档见 [SaveSystem.md](SaveSystem.md)；背包物品的交互菜单（检视、丢弃、拆分）见 [InventoryItemActions.md](InventoryItemActions.md)。

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
   | `Mesh` | 模型，静态网格体或骨骼网格体（骨骼网格体保持参考姿势）。空时显示占位薄片 |
   | `WorldTransform` / `PickupPreviewTransform` / `ReaderTransform` / `DiaryPageTransform` | 同一个模型在场景、拾取提示、文本预览（阅读 / 检视）、日记本页面中的偏移、旋转、缩放 |

3. 场景里放 `TAWorldDocument`（或它的蓝图子类），`Definition` 指向这份资产。它会出现在拾取列表里，可以被扫描。

### 正文插图

正文里写 `<img id="key"/>`，key 在 `InlineImages` 里查。建议单独一行。图片按原尺寸显示，宽于 `MaxImageWidth`（默认 560）时等比缩小；找不到时显示 `[image: key]` 并打 Warning。WBP 的 RichText 控件要在 Decorator Classes 里加 `TARichTextImageDecorator`。

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
- 「显示转译」（`IA_DocumentTranslate`）在模型旁叠加标题 + 正文面板，再按一次收起。面板显示时 `IA_ReaderScroll`（↑↓、右摇杆）和滚轮平滑滚动正文；面板隐藏时右摇杆改为旋转模型。
- 返回走统一的 `IA_UIBack`；「显示转译」「返回」提示可以点击；「旋转」提示只在手柄下显示。
- 手柄时隐藏光标，切回键鼠恢复。
- 打开 / 关闭淡入淡出（真实时间），动画没播完前返回和按钮无效。
- 从场景打开时自己申请时停（优先级 200）；从日记本打开时沿用日记本的时停。

控件命名：`Image_Model`、`Panel_Translation`（内含 `Text_Title`、`ScrollBox_Body` + `RichText_Body`）、`HorizontalBox_Controls`。旋转灵敏度、滚动速度、动画时长、正文字号都在 WBP 的 Class Defaults 里调。

## 日记本 `WBP_Diary`

父类 `UTADiaryWidget`。日记本本身是一个剧情物品（项目设置 `DiaryItem`，样例 `DA_Item_Diary`），场景里有掉落物 `BP_WorldItem_Diary`。

- 打开：背包剧情格里的日记本 → 交互菜单「查看内容」。关闭回到背包。
- 按收集顺序一页一份；0 份是空书（显示 `Text_Empty`），0 / 1 份不能翻页。
- 翻页 `IA_DiaryPreviousPage` / `IA_DiaryNextPage`；查看当前页文本用背包确认键或点击页面上的文本，打开文本预览，返回回到日记本。
- 退出时保存页码，下次打开回到这一页（页码也进存档）。
- 打开时从剧情格位置放大淡入，关闭时缩回去。现在是 UMG 2D 占位，以后换 3D 书本只改 `ApplyOpenCloseVisual` / `ApplyPageFlipVisual`。
- 时停沿用背包的；手柄时隐藏光标。

控件命名：`Panel_Book`、`Panel_Page`、`Button_Document`（内含 `Image_Document`）、`Text_DocumentTitle`、`Text_PageNumber`、`Text_Empty`、`HorizontalBox_Controls`。

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

把一个模型渲染到 RenderTarget 给 UI 显示。每个界面一个槽位，互不干扰：

| 槽位 | 使用者 |
|---|---|
| 0 | 拾取提示 |
| 1 | 日记本页面 |
| 3 | 文本预览 |
| 4 | 物品检视 |
| 9 | 调试命令 `TAPreview` |

- 放在远离关卡的位置，灯光和模型只用光照通道 1，不照亮关卡也不被关卡照亮；曝光固定。
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

项目设置 → Game → **The Awakening Items**（`UTAItemSettings`）：物品、文本、衣服没设置图标时的默认图标（`/Game/DeubgItem/棍母`），模型材质丢失时的兜底材质（`棍母_Mat`）。运行时兜底，不改资产。

## 输入

动作资产在 `Content/Input/Actions/UI/`，按键在 `IMC_UI` 里配置：`IA_DocumentTranslate`、`IA_DocumentRotate`（Axis2D）、`IA_ReaderScroll`（Axis1D）、`IA_DiaryPreviousPage`、`IA_DiaryNextPage`。返回统一走 `IA_UIBack`。

## 生成资源

界面和样例都由命令行工具生成，已有资源默认不覆盖：

| 命令 | 生成 |
|---|---|
| `-run=TAPreviewMaterial` | `M_UI_PreviewStage` |
| `-run=TADocumentUI` | 输入动作和所有 WBP。加 `-Rebuild`（阅读）、`-RebuildDiary`、`-RebuildBanner`、`-RebuildMenu`、`-RebuildQuantity`、`-RebuildInspect` 重建对应 WBP，旧的备份到 `Saved/LayoutBackups` |
| `-run=TADocumentSamples` | 日记本物品、样例文本、日记本掉落物、样例插图 |

## 调试命令

| 命令 | 作用 |
|---|---|
| `TAGiveDiary` | 把日记本放进剧情槽 |
| `TADocList` | 列出已收集（按页序）和已读的文本 |
| `TADocReset` | 清空收集、已读和页码（已销毁的场景文本不会回来，重开 PIE 即可） |
| `TAPreview [路径 \| off]` | 屏幕左上角显示预览台画面 |
| `TAAction [Tag]` | 播放角色动作，不填时打断 |
