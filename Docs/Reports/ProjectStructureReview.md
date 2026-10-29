# The Awakening 项目文件结构检查与优化建议

检查日期：2026-10-27　|　范围：`C:\UnrealProjects\The-Awakening`（UE 5.8.4）　|　方式：只读扫描，未改动任何文件

> 说明：本次是通过目录列表和抽样读取完成的，没有在本机跑 `git` 命令，也没有打开编辑器的 Reference Viewer。标了“需确认”的条目，请在动手前用 Reference Viewer / `git ls-files` 核实一下。
> 所有移动和重命名资产的操作都要在**编辑器内**完成（会生成 Redirector），完成后执行 *Fix Up Redirectors*。不要在资源管理器里直接拖动 `.uasset`。

---

## 一、优先处理（有实际风险）

### 1. 仓库没有启用 Git LFS
- 根目录没有 `.gitattributes`，`.git/hooks` 里也没有 LFS 钩子，说明 `.uasset/.umap` 都是按普通 Git 对象提交的。
- Content 总共约 **300 MB**，其中 `DeubgItem/EKU/Mesh/Eku.uasset` 有 **47 MB**，已经接近 GitHub 的 50 MB 警告线（硬上限 100 MB）。二进制资产每改一次都会在历史里多存一整份，仓库会越来越大，clone 越来越慢。
- 建议：添加 `.gitattributes`，至少包含 `*.uasset`、`*.umap`、`*.fbx`、`*.png`、`*.wav`、`*.ttf` 走 LFS，并对 `*.uasset`、`*.umap` 加上 `lockable`，防止两个人同时改同一个蓝图。
- 是否用 `git lfs migrate` 改写历史需要全队一起决定（所有人要重新 clone），可以先只对以后的提交生效。

### 2. 默认地图和 GameMode 还是第三人称模板
- `DefaultEngine.ini` 里 `GameDefaultMap`、`EditorStartupMap` 都是 `/Game/ThirdPerson/Lvl_ThirdPerson`，`GlobalDefaultGameMode` 是 `BP_ThirdPersonGameMode`。
- 真正的关卡在 `Content/Levels/`（`Persistant_Level`、`Map_Chapter_1_01`）。
- `AssetManagerSettings` 里 Map 类型扫描的是 `/Game/Maps`，这个目录**不存在**。
- 建议：定好正式入口地图后，把默认地图、GameMode 指过去，并把 Map 扫描目录改成实际的关卡目录。BP_ThirdPerson* 可以重命名为项目自己的 `BP_TAPlayerCharacter` 等。注意测试代码（`TAHealthTests.cpp`、`TASettingsTests.cpp`）里硬编码了 `/Game/ThirdPerson/Blueprints/...` 路径，移动后要一起更新。

### 3. 物品 DataAsset 不在 AssetManager 扫描目录里（需确认）
- `PrimaryAssetTypesToScan` 中 `TAItem` 只扫描 `/Game/Inventory`，`TADocument` 只扫描 `/Game/Document`。
- 但 `DA_Item_Herb`、`DA_Item_Keycard`、`DA_Money` 放在 `/Game/DeubgItem/`，`DA_Item_Diary` 放在 `/Game/Document/`（会被当成文档目录扫描，而不是物品）。
- 如果代码有通过 AssetManager / PrimaryAssetId 查找物品，这些物品会找不到，打包后也可能不会被 cook。建议把所有物品 DA 集中到一个目录（例如 `/Game/Data/Items/`），并让扫描路径与之对应。

### 4. 调试资产会进入正式包
- `DirectoriesToAlwaysCook` 里包含 `/Game/DeubgItem/QTE`（注释说明 QTE 默认模板是用软路径加载的），以及整个 `/Game/UI`（其中有 `WBP_TestPuzzle`、4.5 MB 的截图 `屏幕截图_2026-10-07_194206`）。
- `DeubgItem/EKU` 的模型和贴图加起来 **100 MB 以上**，只要有任何关卡或蓝图引用它，就会进入包体。
- 建议：把 QTE 默认模板（`DA_Debug_QTE_*`、`IA_QTE_*`、`IMC_QTE`）移到正式目录（例如 `/Game/Gameplay/QTE/`），同步更新 C++ 里的软路径和 AlwaysCook 配置；纯调试内容统一放到 `/Game/_Dev/` 并加入 `DirectoriesToNeverCook`。

### 5. 打包输出放在项目目录里
- `Build/Windows/` 下有一整份打包产物：`The_Awakening.exe`（317 MB）、`.pdb`（438 MB）、`.pak/.ucas`（约 255 MB）、崩溃转储和日志，总共约 1 GB。
- 目前 `.gitignore` 的 `Build/*` 规则能把它们挡住，但这套规则有好几层取反（`!Build/*/`、`!Build/Automation/` 等），以后改规则很容易误提交。
- 建议：打包时把输出目录设到项目外面，例如 `C:\UnrealProjects\Builds\The-Awakening\`，让 `Build/` 里只保留 `Automation/` 这类需要进仓库的东西。

---

## 二、Content 资产组织

### 2.1 目录拼写和命名不一致
| 现状 | 建议 |
|---|---|
| `DeubgItem/` | 拼写错误，改为 `_Dev/`（或 `Debug/`） |
| `Levels/Persistant_Level` | 拼写错误，改为 `Persistent`；地图建议加前缀 `L_` 或 `Map_` 统一 |
| `Setting/`（数据）和 `UI/Setting/`（界面），而 C++ 用 `Settings` | 统一成 `Settings` |
| `UI/PickUp/`，而 C++ 和 Input 都用 `Pickup` | 统一成 `Pickup` |
| Wwise 事件 `TA_Mob_ChaceSequence_1` | `Chace` 拼写错误；这是 Wwise 生成的，要在 Wwise 工程里改名 |

### 2.2 中文、截图名和乱码文件名
以下资产名在协作、命令行工具和打包路径上都容易出问题，也看不出用途：
- `UI/Setting/`：`矩形_1`、`矩形_2`、`收藏`、`未收藏`、`新菜单`、`左箭头`、`右箭头`、`圆形框`、`圆形填充`、`长方形框`、`长方形填充`、`屏幕截图_2026-10-07_194206`
- `Inventory/屏幕截图_2026-08-25_160927`
- `DeubgItem/棍母`、`棍母_Mat`，以及哈希命名的 `5751a89d…-512`、`5c1d0d6d…-512`、`5d235b4d…-512`
- `DeubgItem/testmodel/front_cloth_不知道`

建议统一改成英文并加类型前缀，例如 `T_UI_Arrow_Left`、`T_UI_Favorite_On`、`T_UI_Frame_Circle`。截图如果只是参考图，就移出 Content。

### 2.3 资产前缀不统一
项目大部分资产已经遵循 `BP_`、`WBP_`、`DA_`、`IA_`、`IMC_`、`M_`、`MI_`、`MF_` 的规范，做得不错。剩下的例外：
- 贴图缺少 `T_`：`Textures/Noise`、`Noise2`、`UI/Inventory/BackGround`、`Models/Document/Lavender_Normal_Map_Paper_Texture`（建议 `T_Paper_Lavender_N`）、`Paper_Default_Front/Back`、`fadedNoteTest`、`DeubgItem/testmodel/body/face/...`
- 材质缺少 `M_`：`Materials/Outline/Outline_Master`
- 动画缺少 `AS_`：`Models/Diary/Book_Close`、`Book_Open`、`Page_Turn_*`、`Document_Extract/Insert`
- 剧情资产没有前缀：`Stories/Demo_01`、`Encounter`（建议 `STA_` 或 `DA_Story_`）
- 精灵图：`Characters/Debug_Character2/` 根目录堆了 **121 个** `Bunny_base_Sprite_*`，建议放进 `Sprites/` 子目录，并加 `SPR_` 前缀
- 两个目录都有 `M_Document_Back/Front`（`Models/Diary` 和 `Models/Document`），同名不同资产，搜索时容易拿错

### 2.4 源文件混在 Content 里
UE 不会使用 Content 里的 `.png`、`.fbx`，但它们会被 Git 跟踪：
- `UI/Setting/*.png`（`矩形 1.png`、`未标题-1.png` 等）、`Setting/TestImages/*.png`
- `LevelPrototyping/**/*.fbx`（10 个，其中 `SM_SM_ChamferCube.fbx` 前缀重复）

建议移到根目录的 `SourceArt/`。`.gitignore` 已经为 `SourceArt` 写好了规则，但这个目录目前还不存在。

### 2.5 两套按键图标（需确认）
- `Textures/Keys/`：Keyboard_Mouse / PS / Xbox，共 274 张，被配置为 AlwaysCook
- `UI/KeyImage/`：key / mouse / ps5 / switch / xboxseries，共 176 张，也会被 `/Game/UI` 的 AlwaysCook 一起打包

请用 Reference Viewer 确认 `TAInputIconSubsystem` 和 WBP 实际用的是哪一套，保留一套。另一套如果是 Switch 等平台的补充，就合并到同一个目录下按平台分子目录。

### 2.6 模板残留
这是一个 Paper2D 横版游戏，下面这些内容大概率用不上，删掉可以减少几十 MB：
- `Characters/Mannequins/`（含 Pistol、Rifle 动画，`CR_Mannequin_Body` 16 MB，两个 `SKM_*_Simple` 各 15 MB）
- `LevelPrototyping/`（JumpPad、WobbleTarget、Door）
- `Input/Touch/`、`Input/Vibration/FFE_Test`
- `ThirdPerson/`（处理完上面第 2 条之后）

删除前务必用 Reference Viewer 或 *Size Map* 确认没有引用，`BP_ThirdPersonCharacter` 很可能还挂着 Mannequin 网格。

### 2.7 同类资产分散在不同目录
- Input Action 分在三个地方：`Input/Actions/`、`InteractionSamples/IA_QTEPrimary` + `IMC_SpecialAction`、`DeubgItem/QTE/IA_QTE_*` + `IMC_QTE`。建议全部收进 `Input/`。
- `InteractionSamples/` 里既有示例 BP，也有正式的事件 DA（`DA_PipeAction`、`DA_PoolEvent`）。如果这些已经用于正式关卡，就不应该再叫 Samples。
- `LevelDesign/` 只有一个 `BP_ParkourMarker`，`Blueprints/` 只有一个 `BP_StoryTrigger` 和镜头抖动、Notify。

### 2.8 建议的目标结构（参考）
不需要一次全部迁移，可以每次碰到哪个系统就整理哪个：
```
Content/
├─ Characters/        Player/、NPC/、每个角色下分 Sprites/、Animations/、Blueprints/
├─ Gameplay/          Interaction/、QTE/、Parkour/、Inventory/、Document/、Story/
├─ Data/              Items/、Documents/、Clothing/、Settings/、WorldEvents/（所有 DA_ 放这里）
├─ Maps/              正式关卡（与 AssetManager 扫描目录对齐）
├─ UI/                按功能分子目录，图标贴图放 UI/Textures/
├─ Input/             Actions/<功能>/、Contexts/（IMC_）
├─ Materials/  FX/  Fonts/  Textures/
├─ Audio/             WwiseAudio（注意改 WwiseStagingDirectory 配置）
├─ Localization/  ThirdPartyNotices/
└─ _Dev/              调试角色、EKU、testmodel、测试 WBP（加入 NeverCook）
```

---

## 三、Source（C++）

整体结构是健康的：Runtime 模块 + Editor 模块分开，Runtime 里按系统分目录（Core、Health、Inventory、Story、Settings……），测试都包在 `WITH_DEV_AUTOMATION_TESTS` 里（抽查了 `TASettingsTests`、`TAHealthTests`）。下面是可以改进的地方。

> 现在另一个线程正在改 Inventory 代码，下面涉及文件移动的建议请等那边合并后再做。

### 3.1 根目录的两个大类
- `The_AwakeningPlayerController.cpp` 有 53 KB，`The_AwakeningCharacter.cpp` 有 35 KB，`.h` 都在 15 KB 左右。
- 建议先移到 `Player/` 目录，再逐步把输入路由、UI 弹窗管理、交互检测这类职责拆成组件（项目里已经有 `TAInputRouter`、`TAPromptComponent` 这种拆分方式，可以沿用）。

### 3.2 文件名与类名不一致
| 文件 | 问题 | 建议 |
|---|---|---|
| `Story/ATAStoryTriggerActor.h/.cpp` | 文件名带了类前缀 `A` | 改为 `TAStoryTriggerActor.h/.cpp` |
| `Scan/TA_HighlightPPActor.h/.cpp` | 多了下划线 | 改为 `TAHighlightPPActor` |
| `Movement/TAParkourMarkerPreview.cpp` | 没有对应头文件，实际实现的是 `ATAParkourMarker` 的方法 | 合并进 `TAParkourMarker.cpp`，或改名为 `TAParkourMarker_Preview.cpp` 表明是拆分文件 |
| `Story/TAWorldEvents.cpp` | 没有同名头文件，实现的是 `FTAInteractionConditions` 等 | 改名为与头文件对应的名字 |

注意：改动 UCLASS 所在的文件名不影响资产，但如果改了**类名**，需要在 `DefaultEngine.ini` 加 `CoreRedirects`，否则已有蓝图会失效。

### 3.3 UI 代码的放置规则不统一
- 一部分 Widget 在 `UI/<功能>/`（Inventory、Pickup、Pause、Document、Cursor）
- 另一部分在各自的功能目录里：`Story/TADialogue*Widget`、`Settings/*Widget`、`Puzzle/TAPathPuzzleWidget`、`Scan/TAScanInfoWidget`

建议二选一并写进文档。考虑到 `UI/` 目录已经很成熟，推荐统一放到 `UI/<功能>/`。

### 3.4 其他
- `Document/TACharacterActionComponent` 是角色动作组件，放在 Document 下不太直观，可以考虑移到 `Player/` 或 `Interaction/`。
- `Health/` 里还有 Checkpoint、KillVolume、Respawn、Rollback，目录名可以改为 `Respawn/`，或者再分一层。
- Editor 模块有约 50 个文件全部平铺，建议分为 `Commandlets/`、`Customizations/`、`StoryGraph/`、`Localization/`、`Tests/`。其中一批 `TA*SetupCommandlet` 是一次性搭建资产用的，已经执行过的可以归档或删除。
- `The_Awakening.Build.cs`：`PublicIncludePaths` 同时加了 `"The_Awakening"` 和 `ModuleDirectory`，重复了；文件末尾还有模板注释（`// Uncomment if you are using Slate UI`），可以清理掉。
- `.uproject` 中 Runtime 模块的 `AdditionalDependencies`（GameplayAbilities、Engine、UMG）一般不需要写；`OnlineSubsystemSteam` 那一段缩进是乱的。
- 测试文件已经有 50 个左右，以后如果继续增长，可以考虑拆成一个独立的 `The_AwakeningTests` 模块，不过目前不急。

---

## 四、Config / Plugins / Wwise

- `Platforms/Android/Config/AndroidGame.ini`、`Config/Linux`、`Config/Mac` 是模板自带的。如果只发布 Windows，可以删掉，减少干扰。
- `DefaultGame.ini` 打包配置里有很多 `-X` / `+X` 成对的行，这是编辑器自动写的，不用手动整理。
- Wwise 工程目录叫 `GDIM-61-Unreal-Project_WwiseProject`，是之前课程项目留下的名字。可以改成 `TheAwakening_WwiseProject`，但要同步修改 `AkSettings` 里的 `WwiseProjectPath`，并通知所有成员重新集成。
- Wwise 工程自带的 `.gitignore` 没有排除 `GeneratedSoundBanks/`。需要全队决定：是每人本地生成，还是统一提交（每人本地生成就把它加入忽略）。
- `Plugins/VisualStudioTools` 是商城插件，建议在 README 的“开发环境”里注明来源和版本，就像 PaperZD 那样。

## 五、.gitignore

建议调整的地方：
1. **`.vsconfig` 不应该被忽略**。它记录了 VS 需要的工作负载，提交后新成员打开 `.sln` 时会自动提示安装缺少的组件。
2. 补充 `.claude/`（本地已有 `.claude/worktrees`）、`*.pdb`、`*.log`、`shadertoolsconfig.json`、`.idea/`、`*.code-workspace`。
3. Wwise 部分有重复的规则：`Plugins/Wwise*/` 写了两遍，其中一行末尾还带空格；`Plugins/Wwise/`、`Plugins/WwiseSoundEngine/` 已经被 `Plugins/Wwise*/` 覆盖了。
4. 第 1 条里提到的 `SourceArt/` 规则目前只忽略 png/tga，如果把 fbx 等源文件移进去，要决定源文件是否入库（推荐走 LFS 入库，或者放到 Google Drive）。

## 六、Docs / Tools / 根目录

- `Docs/` 有 32 篇文档全部平铺，README 只链接了其中 4 篇。建议加一个 `Docs/README.md` 作为索引，按 输入/UI/玩法/工具 分类。`InputRouting.md`（59 KB）和 `SettingsSystem.md`（52 KB）偏长，可以拆成“概览”和“参考”两篇。
- 项目共享文件夹里的 UE 5.8 升级指南也可以放进 `Docs/`，方便队友在仓库里找到。
- `Tools/` 里大多是一次性的编辑器 Python 脚本（`create_*`、`add_*`）。建议分成 `Tools/Editor/`（可重复使用的）和 `Tools/Archive/`（已经执行过的迁移脚本），并在每个子目录里放一个简短的 README（`Tools/Diary/` 已经有了）。
- 根目录的 `_pdftool/` 是空目录，可以删掉。
- `Content/Developers/`（`92010`、`CodexSandboxOffline`）和 `Content/Collections/` 都是空的，不影响什么，可以忽略。

---

## 建议的执行顺序

1. **低风险、马上可以做**：修改 `.gitignore`；删除 `_pdftool`；把打包输出目录移到项目外；添加 `Docs/README.md` 索引。
2. **需要全队协调**：启用 Git LFS（加 `.gitattributes`），确定是否迁移历史；确定 Wwise SoundBank 的提交策略。
3. **在编辑器里分批整理**（每批之后 Fix Up Redirectors 并提交）：修正拼写错误 → 重命名中文和截图资产 → 物品 DA 集中并对齐 AssetManager → 调试资产移入 `_Dev` 并配置 NeverCook → 清理模板残留。
4. **代码整理**（等 Inventory 线程合并之后）：修正文件名 → 统一 UI 放置规则 → 拆分 PlayerController / Character → Editor 模块分目录。

如果需要，我可以先帮你做第 1 步，或者把某一批重命名做成具体的清单。
