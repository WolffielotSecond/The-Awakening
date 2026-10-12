# 版本号管理

项目版本的唯一来源是 `Config/DefaultGame.ini`：

```ini
[/Script/EngineSettings.GeneralProjectSettings]
ProjectVersion=0.1.0-alpha.1

[TheAwakening.Build]
BuildId=local
SaveFormatVersion=1
```

## 编号约定

- 开发阶段使用 `0.x.y`。功能里程碑升级中间位，问题修复升级末位；升级中间位时末位归零。
- 内部测试使用 `-alpha.1`，外部测试使用 `-beta.1`，发布候选使用 `-rc.1`；正式首发使用 `1.0.0`。
- 游戏编号借用 SemVer 的格式，含义按游戏发布里程碑约定；不自动承诺 API 或存档兼容性。
- BuildId 为 UTC 日期、本机当日序号、Git 短提交号，例如 `20261012.001-a1b2c3d4-dirty`。UTC 日期可能与本地日期不同。
- 有未提交改动（含未跟踪文件）时追加 `-dirty`。没有项目 Git 仓库时使用 `nogit`。序号记录在 `Saved/Versioning/counter.json`，失败构建也消耗一个序号；这是本机编号，不是跨机器的全局流水号。
- `local` 表示编辑器日常开发，没有为发布包生成身份。

## 命令

在项目根目录运行，支持 Windows PowerShell 5.1 和 PowerShell 7。脚本支持项目路径和引擎路径中的空格。

```powershell
# 查看当前配置
./Tools/Versioning/Manage-Version.ps1 -Action Show

# 手动发布一个新批次；重置 BuildId 为 local
./Tools/Versioning/Manage-Version.ps1 -Action Set -Version '0.2.0-alpha.1'

# 推荐：自动生成身份，构建、Cook、Stage、Archive，结束后恢复配置原始字节
./Tools/Versioning/Manage-Version.ps1 -Action Package `
  -EngineRoot 'C:/Program Files/Epic Games/UE_5.6' `
  -Configuration Development

# 正式包可选择 Shipping，自定义输出目录
./Tools/Versioning/Manage-Version.ps1 -Action Package `
  -EngineRoot 'C:/Program Files/Epic Games/UE_5.6' `
  -Configuration Shipping -OutputRoot 'D:/Releases'

# 仅写入构建身份，供手动打包使用（会保留配置修改）
./Tools/Versioning/Manage-Version.ps1 -Action Prepare
```

`Package` 当前目标为 Win64，默认输出到 `Saved/Releases/The_Awakening-版本-构建ID-Win64-配置/`。该目录附带 `build-info.json` 和 `CHANGELOG.md`。构建记录包含版本、存档格式、UTC 时间、提交号、工作区改动标记、构建配置和状态。失败输出目录也会保留，其状态是 `Failed`，不可作为发布包。

## 编辑器原生打包入口

现已接入 UE 5.6 编辑器的 Platforms → Windows → Package Project 入口，无须先运行 Prepare。首次接入后建议重启编辑器，让内存中的 Packaging 配置与磁盘一致。

`DefaultGame.ini` 的 Packaging 配置中设置了 `CustomStageCopyHandler=TAVersioning`。UAT 通过 `-ScriptsForProject` 自动发现 `Build/Automation/TAVersioning.Automation.csproj`，在 Stage 阶段调用扩展。扩展执行 `Manage-Version.ps1 -Action Stage`，只修改即将装入包的 DefaultGame.ini 副本。源码配置和编辑器日常显示仍为 `local`，最终游戏包显示真实构建 ID；即使这次打包跳过代码编译，仍会生成身份。输出目录沿用编辑器所选路径。

编辑器原生流程的元数据记录在 `Saved/Versioning/build-info.json`，状态为 `Staged`，表示配置已经写入包的准备阶段，并不代表整个打包成功；构建配置和最终结果以 UAT 日志为准。日志应包含 `TA Versioning: staged BuildId=...`。原生流程不会附加脚本 Package 入口所提供的外部 JSON／CHANGELOG 副本或带版本号的归档目录。

如果使用 Prepare 或 Package 提前生成了非 local 的 ID，Stage 扩展保留该 ID，同一配置在多个分块中也共用一个 ID。保持源码 `BuildId=local` 即可让每次原生打包自动生成新 ID。Prepare 之后可用 `Set -Version 当前版本` 恢复 `BuildId=local`。

当前接入验证范围为 Windows／Win64 的 Pak + IoStore 打包，使用 Windows 自带 PowerShell 执行。请保持 `bMakeBinaryConfig=False`；开启 Binary Config 后，配置合并发生在此扩展之前，当前方案不支持。日常编译不增加游戏版本。不要在脚本 Package 打包时修改或保存项目设置，也不要同时使用编辑器打包。

脚本不自动提交、创建标签或上传发布包。发布前更新 `CHANGELOG.md`，提交代码和资产，确认构建来源。发布标签建议为 `v0.2.0-alpha.1`；同一标签对应的已发布内容不要覆盖，修复后使用新版本。

## C++ 与蓝图

`UTAVersionLibrary` 提供四个 Blueprint Pure 节点，分类为 `The Awakening | Version`：

| 方法 | 返回值 |
| --- | --- |
| `GetProjectVersion` | 游戏版本，缺失时返回 `unknown` |
| `GetBuildId` | 构建身份，缺失时返回 `local` |
| `GetFullVersion` | `版本+构建ID`，用于日志和反馈 |
| `GetSaveFormatVersion` | 存档格式正整数，缺失或无效时返回 `1` |

运行时只读取打包配置，无须安装 Git。现有设置的 Full 高级数据显示已接入统一接口，游戏模块启动时也会输出完整版本和存档格式版本。主菜单可通过 `GetFullVersion` 节点接入；本次没有改动主菜单蓝图资产。

存档版本目前是独立元数据和接入接口，并未实现存档迁移。将来正式存档头应保存 `SaveFormatVersion`；结构发生变化才升级这个字段，并实现逐级迁移。更高格式的存档应提示版本不支持，避免直接覆盖。普通界面修复或内容更新不必提升存档版本。

## 验证

```powershell
./Tools/Versioning/Test-Versioning.ps1
```

脚本测试在系统临时目录创建隔离项目，验证编号校验、字段保留、序号递增、无 Git 回退、Stage 只修改配置副本及模拟打包成功/失败时的配置恢复。

原生入口已通过真实 UAT Stage／Pak／IoStore／Archive 验证，测试复用既有编译和 Cook 数据。发布时仍需完成正常 Cook，并检查打包日志与包内 Full 信息是否和 `build-info.json` 一致。
