# 更新日志

## 0.4.0-alpha.1

### 新增
- 文本数据资产 TADocumentDefinition（标题 / 正文本地化 ID、正文插图表、模型及场景 / 拾取提示 / 文本预览 / 日记本四处展示变换），模型可为静态或骨骼网格体（参考姿势）；资产管理器扫描 /Game/Document。
- 文本收集记录子系统：按收集顺序记录文本、已读旗标（含未收集的）、日记本页码与存档数据；对话、小游戏期间的改动记入回滚日志。
- 物品定义新增 bCanSell、PreviewMesh / PreviewTransform、剧情槽绑定 StorySlotIndex 与升级来源 UpgradesFrom。
- 剧情物品按 StorySlotIndex 自动进入对应剧情槽，升级版替换原位；新增 HasStoryItem（持有升级版也算持有原版）；剧情槽改动记入回滚日志。
- 角色动作组件：PlayAction 按 Tag（Anim.Pickup / Anim.PutDown / Anim.InsertPage）播放动作，PaperZD 动画蓝图认领并回报关键帧和结束；无动画时按游戏时间回退完成；动作期间按配置限制移动 / 跑酷 / 交互并可无敌；死亡、复活时自动打断。
- 调试命令 TAAction <Tag>：播放角色动作，不填 Tag 时打断当前动作。
- 3D 预览台 TAPreviewStage：用 SceneCapture 把单个模型渲染到 RenderTarget 供 UI 显示；支持静态与骨骼网格体（参考姿势）；自动居中取景、自动旋转，手动旋转为固定轴（左右绕竖直轴、上下绕镜头水平轴）且上下可无限旋转；独立光照通道与固定曝光，时停期间照常旋转，模型缺失时显示占位立方体；多个预览台按槽位错开。
- 调试命令 TAPreview [MeshPath | off]：在屏幕左上角显示预览台画面（静态或骨骼网格体）。
- 场景文本 TAWorldDocument：出现在拾取列表中、可扫描；没有日记本时拿起阅读（标记已读、时停）后放回，模型在拿起 / 放下关键帧隐藏和显示，阅读中死亡随回滚撤销；有日记本时交互帧立即收集并销毁，角色播放塞入日记本动作；已收集的文本不再出现在场景中。
- 项目设置 The Awakening Documents：配置日记本物品与阅读界面类；拾取列表行的名称和图标改由场景物体提供，文本拿起放回时不再提示无法拾取。
- 文本预览 TADocumentReaderWidget（WBP_DocumentReader），所有文本预览共用：默认显示文档 3D 模型（鼠标拖动或 IA_DocumentRotate 旋转），“显示转译”（IA_DocumentTranslate）在模型旁叠加标题与正文，再按收起；正文用滚轮、方向键与右摇杆平滑滚动（IA_ReaderScroll，映射在 IMC_UI）；底部操作栏为可点击的按键提示（显示 / 隐藏转译、旋转、返回），“旋转”只在手柄下显示，手柄时隐藏光标；打开 / 关闭淡入淡出，动画期间返回无效；场景直接打开时自行时停，关闭后角色放下文本，阅读中死亡强制关闭。
- 物品交互菜单 TAItemActionMenuWidget（WBP_ItemActionMenu / WBP_ItemActionOption）：背包里光标停在物品上按 IA_ItemActions（IMC_UI 配置）在光标旁弹出；始终有一个选中项（打开时为第一项），上下选择（IA_ChoicePrevious / IA_ChoiceNext）或光标移到选项上改变选中，确定键（IA_Inventory_Confirm / IA_ChoiceConfirm）执行选中项，鼠标点击执行被点项；点击菜单以外关闭菜单且不穿透到背包；返回键关闭。选项按物品类型：普通物品检视 + 丢弃（可堆叠时为部分丢弃 + 拆分堆叠），日记本查看内容，其他剧情物品检视。
- 丢弃：先试脚下，脚下有物品时从正前方起每 60° 试一个方向；被墙挡、高低差超过 30 cm、夹角超过 45°、已有物品的位置不能落；都不行时提示“当前位置无法掉落物品”，物品留在原格子。物品定义新增 WorldItemClass（丢弃时生成的世界物品）；世界物品没有模型时用 PreviewMesh 或占位方块；背包新增可选 Text_Message 提示文字。
- 拆分堆叠与部分丢弃：数量小窗口 TAQuantityDialogWidget（WBP_QuantityDialog，“< 数量 >”加确定 / 取消），数量 1 ~ 全部；拆分放进第一个空格且不与其他堆叠合并，背包没有空格时「拆分堆叠」置灰；部分丢弃按丢弃的落点规则。调数量用 IA_QuantityDecrease / IA_QuantityIncrease（IMC_UI 配置），确定键确认，返回键取消，窗口外点击不穿透。
- 物品检视 TAItemInspectWidget（WBP_ItemInspect）：背包变暗变模糊，物品 3D 模型（PreviewMesh，缺失时占位）在屏幕中央，名称和介绍在下方；鼠标拖动或 IA_DocumentRotate 旋转（固定轴、可无限翻转）；操作栏“旋转”仅手柄显示、“返回”可点击；手柄时隐藏光标；返回回到背包。
- 日记本可从地上拾取：BP_WorldItem_Diary；草药、门卡、钱的物品定义关联各自的世界物品蓝图。
- 日记本 TADiaryWidget（WBP_Diary）：从剧情格位置放大打开、关闭时缩回；右页显示当前文本的 3D 模型与标题，点击或确认打开文本预览，返回回到日记本；按收集顺序一页一份，0 / 1 份不能翻页，0 份显示空书提示；翻页（IA_DiaryPreviousPage / IA_DiaryNextPage）为 2D 过渡占位；退出时保存页码；底部操作栏为可点击的按键提示，手柄时隐藏光标。
- 文本子系统新增 FindDefinition：按 DocumentId 在资产注册表中查文本定义。
- 富文本插图装饰器 TARichTextImageDecorator：正文 <img id="key"/> 按文本定义的 InlineImages 显示图片，超宽等比缩小。
- 编辑器命令行 -run=TADocumentUI 生成输入动作 IA_ReaderScroll（含 IMC_UI 映射）、IA_DocumentTranslate、IA_DocumentRotate、IA_ItemActions、IA_DiaryPreviousPage、IA_DiaryNextPage、IA_QuantityDecrease、IA_QuantityIncrease，以及 WBP_DocumentReader、WBP_ItemActionMenu、WBP_ItemActionOption、WBP_Diary、WBP_QuantityDialog、WBP_ItemInspect、WBP_PickupBanner；已存在的不覆盖，-Rebuild / -RebuildMenu / -RebuildDiary / -RebuildQuantity / -RebuildInspect / -RebuildBanner 先备份到 Saved/LayoutBackups 再重建对应 WBP。
- 示例数据：日记本物品 DA_Item_Diary（剧情槽 0）与示例文本 DA_Document_Sample01（长文）/ 02（含插图）及三种语言文本；编辑器命令行 -run=TADocumentSamples 生成。
- 调试命令 TAGiveDiary、TADocList、TADocReset。
- 预览台默认透明背景：以 SceneColorHDR 捕获，新增 UI 显示材质 M_UI_PreviewStage（GetDisplayMaterial 返回已绑定的材质实例）；编辑器命令行 -run=TAPreviewMaterial 生成该材质。
- 项目设置 The Awakening Items：物品（含文本、衣服）没有图标时显示默认图标（DeubgItem/棍母）；世界物品和 3D 预览台上的模型丢失材质（槽位为空或引擎默认材质）时换成兜底材质（DeubgItem/棍母_Mat）。运行时兜底，不修改物品资产。
- 文本扫描介绍统一为“一张写满了字的纸。”（项目设置 The Awakening Documents 的 bUseUnifiedScanDescription，可关闭改回各自的介绍）。
- 新增文本 DA_Document_67（模型 SK_A4_Paper_With_Fold，三种语言的标题与正文）。
- 拾取提示 TAPickupBannerWidget（WBP_PickupBanner）：物品进背包、文本收集后屏幕右侧滑入（0.5 秒）、停留、滑出（0.5 秒），共 5 秒，真实时间不受扫描时停影响；上方顺时针旋转的 3D 模型（每秒 180°），中间名称，下方“[背包键] 查看详细”；文本先显示标题 1.85 秒，淡出淡入 0.3 秒后显示正文前 20 字 + “...”（跳过图片标记）1.85 秒；显示中再拾取时当前条立即滑出、下一条滑入。钱不弹提示。
- 快速查看：拾取提示显示期间按背包键，打开背包并直接进入对应界面——普通物品与其他剧情物品进入检视，日记本进入查看内容，文本由日记本翻到该页并跳过入场动画直接进入文本预览；返回逐层回到背包。
- 最小存档层：TASaveGame 与 TASaveSubsystem，单槽；保存存档格式版本、游戏版本、时间、关卡、复活点、剧情旗标与进度、文本收集记录、背包（按格子位置，含外套与剧情槽）与钱；玩家交互复活点或走进范围新激活时自动存档；读档恢复数据、传送到存档复活点并销毁已收集的场景文本，关卡不同时先打开存档关卡；更高存档格式拒绝读取。背包新增 GetSaveData / RestoreSaveData。
- 调试命令 TASave、TALoad、TASaveDelete、TASaveInfo。
- 剧情条件新增 HasDocument（已收集指定文本）与 ReadDocument（已读过指定文本），条件新增字段 Document（文本 ID，JSON 字段 "document"）；剧情编辑器的条件类型下拉中可选，选中后只显示文本 ID 字段。

### 修复
- 扫描被背包取消后在界面里松开扫描键，下一次按下不触发扫描：扫描在等待松开期间改为读取物理按键观测，确认松开即解除等待。
- 设置英文布局自动化测试：底部三个按钮或其 Slot 缺失时逐个报出并判为失败，不再空指针崩溃；全量测试不再中途退出。

### 调整
- 背包不再自动整理：拾取时先补满已有同类堆叠再放进第一个空格，移除 / 丢弃后格子留空，其他物品位置不变；SortInventory 保留为手动整理。
- 背包打开时时停（完全冻结角色与敌人，UI 照常），关闭或被强制关闭时释放。
- 扫描中或扫描淡出期间按背包键：取消扫描并立即打开背包，冻结无缝交接；扫描键需松开后才能重新扫描。
- 剧情槽不再能拖拽、互换、丢弃，剧情物品不再进入口袋；未绑定或槽已被占用的剧情物品拾取会被拒绝。

### 文档
- 新增 Docs/DocumentSystem.md（文本、日记本、文本预览、拾取提示、预览台、角色动作、剧情条件）、Docs/InventoryItemActions.md（背包交互菜单、丢弃、拆分、数量窗口、检视）、Docs/SaveSystem.md（存档内容、时机、读档流程、接入方法）与 Docs/DocumentSystemPIEChecklist.md（PIE 检查清单）。
- VersionManagement.md 的存档版本说明、HealthAndRespawn.md 的复活点存档说明按现状更新。

## 0.3.0-alpha.1

### 新增
- 生命、伤害与死亡：统一伤害入口（普通 / 真实 / 强制三种伤害，来源 Tag 必填）；伤害公式为易伤 → 防御 → 减伤，修改器以列表存储、按伤害 Tag 匹配，支持临时修改器；新增防御力与防御力加成属性。
- 保护：无敌（可按来源放行）与不会死（最低 1 血），均挡不住强制伤害；小游戏、对话、复活后、TAGod 统一走保护入口。
- 死亡流程：死亡时锁住输入、关闭所有页面并取消扫描；对话与小游戏期间的改动记入回滚日志，死亡时按逆序撤销并恢复到未开始状态。
- 复活：复活点（最后交互的生效，无复活点时回到 PlayerStart）、死亡中 → 黑屏 → 复位 → 淡入流程、复活无敌、OnWorldReset、杀死体积；参数在项目设置 The Awakening Respawn 中调整。
- 血量 HUD 的 C++ 父类与生命组件事件；调试命令 TADamage、TATrueDamage、TAKill、TAHeal、TAGod、TACheckpoint。
- 统一时停：冻结与全局暂停按来源申请和释放，新增游戏时间时钟与游戏时间计时器；扫描、小游戏、暂停菜单改用统一入口。
- 设置“显示高级数据”的完整模式新增平台（读构建配置，当前为 Steam）和游戏时间（HH:MM:SS(倍率X)）。
- 手柄：通过 Steam Input 识别 PlayStation 手柄并显示对应按键图标（Switch 仍显示 Xbox 图标）；摇杆与扳机的最小 / 最大死区设置及预览。
- 统一世界交互提示：跑酷标记、复活点、剧情触发器通过提示锚点显示提示，默认控件在项目设置 The Awakening Prompts 中配置；交互提示与执行共用同一目标查询，冻结时隐藏。
- UE 5.6 编辑器原生 Windows 打包入口自动在 Stage 阶段生成构建 ID，源码配置保留 local。
- 统一游戏版本、构建 ID 与存档格式版本读取接口。
- 打包时生成构建 ID、构建记录和带版本号的输出目录。

### 调整
- 小游戏扣血惩罚改为真实伤害（来源 Puzzle）；小游戏期间仅惩罚能扣血且不致命。
- 掉落物品不再显示头顶提示，交互提示只由拾取列表负责。
- 繁体中文“越过”更正为“越過”。

### 移除
- 删除属性 DamageReduction（由百分比减伤修改器替代）。

## 0.2.3-alpha.1

### 调整
- 生成拾取条目时上下 Padding 默认设为 -15；面板暴露 RowVerticalPadding，并支持蓝图即时更新已有条目。

## 0.2.2-alpha.1

### 修复
- 补齐上下滚轮与滚轮轴的图标映射，修复拾取选择提示缺失；资源测试覆盖三种滚轮图标。

## 0.2.1-alpha.1

### 调整
- 拾取条目支持 VerticalBox_Keys 中的上下选择按键提示，仅选中行显示；按实际设备和 IMC 绑定刷新。

## 0.2.0-alpha.1

### 新增
- 附近多物品拾取列表、稳定选择、范围复核和拾取结果反馈；沿用部分拾取与货币处理。
- 拾取面板和条目 C++ 父类；WBP 由设计者拼装，超过可视高度自动滚动到选中条目。
- 可编辑 IMC_Pickup、滚轮／手柄方向键选择动作，以及多语言和实际绑定按键提示。

## 0.1.0-alpha.1

- 建立版本管理起点；该编号不代表此前已经发布过同名版本。
