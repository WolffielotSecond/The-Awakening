# 衣服 DataAsset、背包与衣架换衣布局

**背包、衣架换衣布局与口袋坐标/连线阶段已完成并收工（2026-11-01）。**
当前交付包含衣服的三张独立图片属性、可编辑口袋数据、背包单图切换与口袋排版、装饰线、
装备栏对齐、框内右下角提示，以及换衣界面的左右半屏布局和固定外套栏。
口袋连线已接入图片标记坐标；正式衣服美术、角色展示、技能页内容留待下一阶段。
顶部 Tab、颜色、输入和背景规格见 [InventoryTabs.md](InventoryTabs.md)。

## 编辑衣服

内容浏览器中打开 `Content/Inventory/DA_Clothing_Inner_Default` 或 `DA_Clothing_Outer`。
新增衣服使用 Data Asset → `TAClothingDefinition`，每件衣服一个独立资产。

| 字段 | 用法 |
| --- | --- |
| Layer | Inner 为内衣，Outer 为外套 |
| Equipment Icon（Icon） | 装备栏、衣架交换栏与拖拽预览的小图标 |
| Clothing Display Texture（DisplayTexture） | 背包与换衣界面右侧的穿着图，可包含内衣，与小图标独立 |
| Rack Display Texture（RackDisplayTexture，仅外套） | 换衣界面左侧衣架上的外套单独立绘，不含内衣；默认 T_DefaultItemIcon |
| Pocket Count | 整数 1–3；修改后自动调整 Pockets 列表长度 |
| Pockets → Pocket Id | 同一件衣服内唯一的口袋标识 |
| Pockets → Columns | 横向列数，1–4 |
| Pockets → Rows | 纵向行数，1–2 |
| Pockets → Pocket Image UV | 衣服正方形图片内的口袋标记，Vector2D；左下 (0,0)、右上 (1,1)，默认 (0,0) |

Pockets 顺序就是画面中从上到下的顺序。1 个口袋显示一个格子群，2 个上下排列，3 个上中下排列。
每个口袋容量为 Columns × Rows；编辑器限制输入范围，资产验证同时检查数量、形状和重复 ID。
新建资产从 1 个 1×1 口袋开始。更改衣服贴图不需要修改蓝图或背包代码。

### 口袋图片坐标与连线

衣服展示美术采用 1:1 正方形画布，衣服在画布内的位置和占比可各自调整。
每个口袋独立填写 `Pocket Image UV`（`PocketImageUV`），标记以整张图片（包括透明留白）为基准：
X 向右、Y 向上，左下 (0,0)、左上 (0,1)、右下 (1,0)、右上 (1,1)，中心 (0.5,0.5)。
两个分量限制在 0–1；新建口袋和未设置此字段的已有资产均默认 (0,0)，该值是有效角点，不作为“未设置”哨兵。
此字段通过所属口袋的 PocketId 对应格子群，与旧 `AnchorUV` 无关，不改变格子位置、容量或物品索引。
连线按图片实际显示区域换算，排除 ScaleToFit 的外侧留白，UI 向下的 Y 使用 `1-Y`。
每个口袋的终点为格子群靠图片一侧的边缘中心：两行落在行间分界线上，一行落在行高度中央。
从终点水平延伸到图片边缘，再斜连到该口袋的 Pocket Image UV。衣架左侧格子在图片左边时自动镜像方向。
线条颜色为 `#74B9B0`，在图片和格子绘制后叠加，不拦截输入。
Standalone 窗口偏移修复：连线绘制时图片与格子统一读取 `GetPaintSpaceGeometry()`，
与 NativePaint 的父几何使用同一空间，避免将 GetCachedGeometry 的桌面窗口偏移混入绘制坐标。
渲染回归覆盖 100%/150% 缩放，以及桌面偏移 (0,0)、(120,180)、(360,420)；
比较前景像素时关闭测试实例的 BackgroundBlur，避免离屏模糊历史残影，游戏实际模糊不变。
此项是模拟窗口偏移的离屏验证，真实 Standalone 仍可手动复核。
调整粗细：打开 `/Game/UI/Inventory/WBP_ClothingPanel` → Class Defaults（类默认值）→
`ClothingUI | Pocket Connections` → `Pocket Connection Thickness`，默认 1.5 UI 单位，0 隐藏。
背包与换衣界面通过各自的 ClothingPanelClass 共用此 WBP；无需修改格子 WBP 或两个主界面的 Designer。
当前每个口袋只有一套标记，在背包和衣架展示图中共用；不同展示图及外套合成图中内衣口袋的额外标记配置尚未添加。

当前内衣展示图继续使用原来的白色草图，容量 1 行 × 2 列。外套 `DisplayTexture` 占位使用
`/Game/UI/Common/T_DefaultItemIcon`，`Icon` 小图标使用原衣服草图；目前外套仍有两个口袋，分别为 1 行 × 2 列、2 行 × 2 列。
正式美术应将外套和露出的内衣画在同一张完整贴图中。
已有衣服资产将原 Icon 复制到 DisplayTexture 以保留展示，再独立配置小图标。
显示时两个字段互不回退；缺图各自使用项目默认占位图，不会因为更换小图标而改动完整展示图。
第三张 `RackDisplayTexture` 与前两张独立，外套资产可单独指定。美术绘制时仅包含外套，不画内衣。
默认占位路径为 `/Game/UI/Common/T_DefaultItemIcon`。衣架换衣界面左侧已接入此图；右侧穿着图仍使用 DisplayTexture。

## 背包显示规则

- 左边只显示一张衣服图：无外套时读取内衣 DisplayTexture，穿上外套时改读外套 DisplayTexture，脱下恢复；右侧衣服装备栏只读取 Icon。
- 右边统一显示口袋；内衣与外套的每个格子群左边缘对齐。
- 无外套时，内衣组在衣服展示区域高度中间，独立 Grid 外框四边等宽留白，外套框隐藏。
- 穿外套后，内衣框位于上方，外套框位于其下；所有外套口袋放进同一个大 Grid 外框。
- 根据实际行列数测量格子群和外框，口袋之间留相同间距，不生成占位空格。
- 同屏格子同尺寸；按可用区域统一计算缩放，预留外套最大 6 行 × 4 列及内衣框、留白与间距。
- 沿用 Grid 与 `#799FA7` 调色。原来的 AnchorUV、SlotSide、内衣位置覆盖字段保留旧数据兼容，新背包不使用，也不再作为可编辑排版参数。
- 口袋连线使用每个口袋的 Pocket Image UV；顶栏、剧情格、外套装备栏保持各自功能。

背包面板外侧装饰线参照效果图按整屏比例绘制：左线 x=4.1%、y=14%–91.5%，
右线 x=95.2%、y=10.5%–95%，底线 y=95.2%、x=20.9%–57.5%。
两条竖线保留不对称的实心端帽，线身带轻微柔光；线身和端帽颜色统一为 `#74B9B0`，
粗细随界面高度缩放，不参与鼠标或手柄命中。
衣服装备栏与内衣口袋行的中心对齐；装备栏右边缘到右线的距离，等于剧情格左边缘到左线的距离。
装备栏尺寸沿用剧情格，穿脱外套时随内衣口袋行移动。
按键提示整排放在装饰线围出的区域右下角，末端右对齐；右/下边缘位于屏幕 94%，
相对右线与底线各留 1.2% 边距，提示内容及 IMC 键鼠/手柄逻辑沿用。
该提示栏图标复用顶部 Q/E 的 `M_InventoryTabIcon` 反色配色材质：深色 `#26343A`、
浅色 `#B9D7D8`；说明文字使用未选中 Tab 的亮青色 `#B9D7D8`。
每个提示独立持有材质实例，切换设备或改绑后继续从 IMC 读取图标并保持配色；其他界面的提示不启用此配色。

`UTAInventoryPanelWidget` 将原内衣 NamedSlot 扩为组合展示区域，原外套 NamedSlot 收起；
`UTAClothingPanelWidget` 在背包与衣架换装中共享组合布局和 Grid 外框，
衣服实例、物品拖拽、内衣优先的格子索引及衣服能力沿用现有系统。

## 衣架换衣界面

| 左侧：衣架上待更换的外套 | 右侧：玩家当前穿着 |
| --- | --- |
| 外套口袋在左，RackDisplayTexture 在右 | DisplayTexture 在左，内衣/外套口袋在右 |
| 只有外套口袋，统一包进一个大 Grid 框 | 内衣独立框在上、外套统一框在下；无外套时仅内衣框居中 |
| 空衣架隐藏衣服大图与口袋 | 无外套时恢复内衣大图与内衣口袋 |

左右各占半屏，在各自一半内分配独立的图片区与格子区；图片保持原始宽高比缩放。
图片区和口袋区之间保留空隙，外框不会覆盖图片。两侧使用同一套最大容量预算：
外套 6 行 × 4 列，加内衣 1 行 × 2 列及外框留白、组间距；左侧虽然没有内衣口袋，
也预留相同缩放预算，因此左右格子同尺寸。实际口袋行列仍按 DataAsset 生成。

两个外套装备栏固定在各自半屏上方（中心 x=25%/75%，顶部 y=13%），
不会随外套图或口袋隐藏而重新居中。只会按窗口大小同比调整方格尺寸。
背景沿用 `#101820` 半透明遮罩与 BlurStrength=0.4，Grid 使用 `#799FA7`，装备栏使用 Grid_Solid，标题使用亮青色。

布局由 `UTAClothingSwapWidget::BuildSwapLayout` 的运行时 Canvas 管理；旧 WBP 居中列不再控制排版。
外套栏互拖仍负责穿上、脱下和交换整件外套；口袋物品继续支持衣架与玩家之间移动，物品随衣服保留。

## 存档与调试

加载旧衣服资产不会自动裁剪原口袋容量，旧 SlotCount 数据仍可读取。
新布局也会容纳实例中已有格子。修改资产中的实际容量与移动 UI 不同：
现有存档恢复/换装规范化按口袋和格子顺序匹配，缩减口袋或格子仍可能丢弃超出新容量的物品。
调整容量请先备份存档，或使用新开局调试；本次不改写 SaveGames 或自动搬移物品。

玩家开局继续只有内衣。可通过已有衣架穿脱外套检查布局。
自动化 `TheAwakening.Inventory.Clothing.WornLayout` 穷举 1–3 个口袋的合法形状组合，
检查边界、左边缘对齐、留白、间距与无外套居中。
`WornPresentation` 使用实际背包蓝图检查单图切换、最大 24 个外套格子、不同形状重建、
穿脱后内衣物品保留、格子索引和装备小图标/完整衣服图独立显示。
带 `-ClothingPreview` 且启用渲染时生成 `Saved/ClothingPreview/` 三张预览；普通测试不生成截图。
换衣界面的 `SwapLayout` 专项还检查了左右图片区隔离、最大容量缩放、外套栏固定位置、
空衣架/穿脱/两件外套互换，以及从衣架口袋拖到玩家内衣口袋。
它会额外生成 `SwapRackOccupied.png`、`SwapRackEmpty.png`、`SwapBothCoats.png` 和 720p 预览。

2026-11-01 最新验证：UE 5.8 Development Editor 编译通过；衣服专项与
`TheAwakening.Input.UIPlayerIngress` 共 **10/10 通过**（包含口袋连线路径测试）。覆盖无外套、三个最大口袋、混合尺寸口袋，
背包装备栏与内衣行对齐/距两侧装饰线等距，以及换衣界面 1600×1000、1280×720 的边界与不重叠检查。
穿上、脱下、两件外套交换和衣架口袋向内衣口袋移动均经实际控件入口验证。
测试物品和最大容量外套只存在于临时运行实例，不写入正式关卡、衣服资产或存档。

## 阶段收尾

临时资产配置/检查脚本与过期日志已清理，正式回归代码保留。
本机 `Saved/ClothingPreview/` 保留以下结果：

- 背包：`InnerOnly.png`、`ThreePocketsMaximum.png`、`MixedPockets.png`。
- 换衣：`SwapRackOccupied.png`、`SwapRackEmpty.png`、`SwapBothCoats.png`，分别对应衣架有外套、衣架取空和两边均有外套。
- 最新 `Validation.log` 和 `Report/index.json`（10/10 通过）；含背包和衣架连线渲染预览。
- `Before/DA_Clothing_Outer.uasset` 与 `Before/ClothingImageSplit/` 的迁移前备份和结果 JSON。

正式 `TAPocketConnectionTests.cpp`、`TAClothingSwapLayoutTests.cpp` 和已有衣服/输入回归保留；本轮没有一次性工具源码需要移除。
不再保留旧报告 HTML、重复截图、调试日志备份与临时 PDF 页面渲染；清单见
[Maintenance20261031.md](Maintenance20261031.md)。最终预览不作为资产配置来源。

2026-11-01 连线阶段收尾删除 19 个重复日志、冗余 720p 预览、报告 HTML 和可重建 ShaderDebugInfo，
约 5.70 MiB。保留六张主预览、10/10 验证日志与 JSON、原资产备份和正式测试。
清单为 `Saved/Cleanup/20261101-pocket-connections/manifest.json`，存档清理前后内容校验一致。
后续若不同图片中的口袋位置不同，需要再拆分坐标配置；当前每个口袋在两个界面共用同一个坐标。
