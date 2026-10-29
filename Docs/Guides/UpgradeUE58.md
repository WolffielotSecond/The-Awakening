# Molt：The Awakening 升级到 UE 5.8 步骤

项目现状（2026-10-27 检查）：`EngineAssociation` 为 5.6；Target.cs 使用 `BuildSettingsVersion.V5` 和 `EngineIncludeOrderVersion.Unreal5_6`；依赖 GAS、StateTree、PaperZD、Wwise（AkAudio）、Steamworks。Wwise 被 `.gitignore` 忽略，每个人本地通过 Wwise Launcher 安装。

## 0. 准备（先做，不改任何东西）

1. 提交或暂存所有本地改动，确认 `git status` 干净。
2. 打一个标签作为回退点：`git tag pre-molt`
3. 新建分支：`git checkout -b upgrade/molt`
4. 升级完成、全队确认之前，其他人继续在 5.6 的主分支上工作。

## 1. 安装引擎和第三方插件

1. Epic Launcher → 虚幻引擎 → 库 → 安装 **5.8**（勾选需要的平台和调试符号）。
2. Launcher 库里找到 **PaperZD**，点“安装到引擎”，选 5.8。**如果没有 5.8 选项，先停下**，等作者更新，或去 PaperZD 的 GitHub releases 找源码版。
3. **Wwise Launcher**：
   - 先在 Unreal 页签里确认 2025.1.x 是否提供 UE 5.8 的集成包。没有的话，升级到提供 5.8 支持的 Wwise 版本（这会同时升级 Wwise 工程 `GDIM-61-Unreal-Project_WwiseProject`，需要声音同学一起做）。
   - 对本项目执行“修改 Wwise 集成”，部署平台选 UE 5.8，让它重新写入 `Plugins/Wwise*`。
   - Wwise 不在 git 里，**每个队员**之后都要在自己电脑上重做这一步。

## 2. 修改项目文件

- `The_Awakening.uproject`：`"EngineAssociation": "5.8"`（也可以右键 .uproject → Switch Unreal Engine version → 5.8，它会改这个字段并重新生成 .sln）。
- `Source/The_Awakening.Target.cs` 和 `Source/The_AwakeningEditor.Target.cs`：
  - `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;`
  - `DefaultBuildSettings`：如果 5.8 新增了更高的 `BuildSettingsVersion`（编译时 UBT 会有警告提示），改成最新值；否则保持 V5。
- 删除 `Binaries/`、`Intermediate/`、`Plugins/*/Binaries`、`Plugins/*/Intermediate`、`DerivedDataCache/`（都不在 git 里），再重新生成 VS 工程文件。

## 3. 编译

1. 用 VS 打开新的 .sln，编译 **Development Editor | Win64**。
2. 按报错修：
   - 废弃 API 的警告先记下，错误必须修。重点关注 GAS（GameplayAbilities）、StateTree、UMG/Slate、Enhanced Input、Steamworks/SteamShared。
   - `AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks")` 和 `SteamShared` 在新版本里可能有改动，编译不过时优先看这里。
3. 编译通过后再开编辑器（不要在编译失败时让编辑器自己重建）。

## 4. 打开编辑器并验证

1. 第一次打开会重编译着色器、重建 DDC，可能很久。
2. 看 Output Log，有没有加载失败的资产或插件。
3. 打开主要关卡，检查 World Partition 是否正常。
4. 跑全部自动化测试（Session Frontend → Automation，或命令行 `-ExecCmds="Automation RunTests The_Awakening"`），和 CHANGELOG 里记录的已知失败对比，只看新增的失败。
5. 按 `Docs/DocumentSystemPIEChecklist.md` 等 PIE 清单手动过一遍：跑酷、扫描、背包、拾取提示、文本阅读、QTE、存档读档、死亡复活、Wwise 声音、PaperZD 动画。
6. 打一个 Windows 包跑一下，确认 Steam 和 Wwise 在打包版里也正常。

## 5. 全队切换

1. 在分支上提交 .uproject、Target.cs 和代码修复（以及升级后被重新保存的资产），发 PR。
2. 合并前通知全队：安装 UE 5.8、PaperZD 5.8、Wwise 5.8 集成包。
3. 合并后大家拉代码，删掉本地 `Binaries/`、`Intermediate/`，重新编译。

## 6.（可选）接上 Claude 的 Unreal MCP

1. `.uproject` 的 Plugins 里加 `ModelContextProtocol` 和 `AllToolsets`（`"Enabled": true`）。
2. 编辑器控制台：`ModelContextProtocol.StartServer`，然后 `ModelContextProtocol.GenerateClientConfig ClaudeCode` 生成 `.mcp.json`。
3. 本机 Claude Code：`/plugin install unreal-engine-skills-for-claude-code@claude-plugins-official`
4. Windows 需要 Git Bash，启动钩子才会生效。
