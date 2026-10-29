# 资产整理第一批：执行清单（待确认）

日期：2026-10-27　|　状态：**已执行（2026-10-27，未提交）**，本批和“推迟”部分已一并完成

## 前提
1. 先把 `.gitignore`、`.gitattributes`、`Docs/README.md` 提交并推送，所有人执行 `git lfs install`。
2. 通知队友：把涉及下列目录的资产改动先推送上去，整理期间不要修改这些目录。
3. 用桌面快捷方式 “The_Awakening (UE 5.8 + MCP)” 打开编辑器（我会通过它在编辑器里执行重命名，生成 Redirector）。

## 本批内容（不涉及 Inventory 线程正在改的文件）

### A. `Levels/Persistant_Level` → `Levels/Persistent_Level`
- 代码和配置里都没有引用这个路径，只需要在编辑器里重命名，然后执行 Fix Up Redirectors。

### B. QTE 模板：`/Game/DeubgItem/QTE/*` → `/Game/Gameplay/QTE/*`
同时移动 `DeubgItem/TA_Debug_QTE_Tester` → `/Game/Gameplay/QTE/Debug/`。需要同步修改的文本引用：
| 文件 | 行 | 内容 |
|---|---|---|
| `Source/The_Awakening/Interaction/TAQTESequenceComponent.cpp` | 10 | 默认模板软路径 `DA_Debug_QTE_Pipe` |
| `Source/The_AwakeningEditor/TAQTESetupCommandlet.cpp` | 76, 120, 126 | 生成目录和日志 |
| `Source/The_Awakening/Tests/TAWorldInteractionTests.cpp` | 302, 606 | cook 目录断言、`IA_QTE_Q` 路径 |
| `Config/DefaultGame.ini` | 129 | `DirectoriesToAlwaysCook` |
| `Docs/QTEDebugTester.md`、`InteractableObjects.md`、`WorldInteractionSystem.md` | 多处 | 文档路径 |

### C. `/Game/Setting` → `/Game/Settings`，`/Game/UI/Setting` → `/Game/UI/Settings`
| 文件 | 内容 |
|---|---|
| `Config/DefaultGame.ini` 第 123、159 行 | AlwaysCook、`DefinitionAsset=/Game/Setting/DA_Settings` |
| `Source/The_Awakening/Tests/TASettingsTests.cpp` 第 293、865 行 | WBP 路径、cook 根目录 |
| `Source/The_Awakening/Tests/TASettingsEnglishLayoutTests.cpp` 第 41、84、99、168 行 | WBP 路径 |
| `Tools/Settings/*.py`（5 个脚本） | 资产路径 |
| `Docs/SettingsSystem.md`、`InputIcons.md`、`GamepadDeadZone.md`、`CommonDialogSystem.md` | 文档路径 |

## 推迟到 Inventory 合并之后
- `DeubgItem/` 中剩下的部分 → `_Dev/`。原因：`Inventory/TAItemSettings.cpp` 的默认物品图标 `/Game/DeubgItem/棍母` 是运行时要用的（不能放进 NeverCook），而这个文件正被 Inventory 线程修改；物品 DA（Herb、Keycard、Money）也属于背包系统。
- `UI/PickUp` → `UI/Pickup`。原因：`The_AwakeningCharacter.cpp` 第 872 行引用了它，这个文件也正在被修改。另外，在 Windows 上只改大小写的重命名，UE 和 Git（`ignorecase=true`）都容易出问题，需要分两步改名，单独做比较稳妥。

## 执行后的验证
- 编译编辑器；运行自动化测试 `TheAwakening.*`（重点是 Settings、WorldInteraction）。
- PIE：打开设置菜单，触发一次 QTE。
- 检查 `git status`：应该只看到重命名的资产、Redirector 被清理，以及上表中的文本改动。
