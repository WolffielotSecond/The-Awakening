# 2026-10-31 阶段收尾与本机清理

背包 UI 本阶段已完成，后续制作暂缓。最终样式、数据来源和未制作范围见
[InventoryTabs.md](InventoryTabs.md)。历史设计差异及实施计划保留，但已标记为历史记录。

## 清理结果

| 指标 | 结果 |
| --- | --- |
| 清理前 Saved | 2459.92 MiB（约 2.40 GiB） |
| 清理后 Saved | 353.21 MiB |
| 释放空间 | 2106.71 MiB（约 2.06 GiB） |
| 删除文件 | 2632 个 |
| 存档校验 | `TA_Slot0.sav` 清理前后 SHA-256 一致 |

大小是本次收尾时的快照，后续启动编辑器、测试或打包会重新生成日志及缓存。
逐项清单位于本机 `Saved/Cleanup/20261031/manifest.json`。

删除范围：

- 旧 `StagedBuilds`（约 1163 MiB）及 `Cooked` 打包产物。
- 房间层试验的重复 PNG / HDR、临时分析脚本、旧调试快照；房间原型保留小型 zip、README 和结果 JSON。
- 崩溃转储、旧 profiling、shader 调试产物、字体试验依赖和临时脚本。
- 自动化报告中的可再生成媒体、HTML 和大型资产副本；保留 JSON 摘要。
- 历史备份日志、旧引擎版本选择器日志、本轮过时的生成／检查日志。
- 过时 UI 预览，只保留最终顶栏裁图和完整背包预览。
- 一次性 `TAInventoryTabsCommandlet` 源文件及预览样例数据入口；不再作为长期编辑器功能维护。

保留范围：

- `Saved/SaveGames`、`Autosaves`、`Config`、`Collections`、`Versioning` 及资产回滚备份。
- 本机 `LocalCaches` 中约 318 MiB 的可用构建／Zen 缓存，不为腾空间强制停止服务。
- 各系统最新日志、自动化 JSON 摘要；本轮最终输入回归日志与预览。
- `Content` 中正式蓝图、物品、贴图、材质、模型、关卡和本轮背包 UI 资产。
- 正式自动化测试源码和 `Tools/` 下可重复使用的导入、验证、版本管理工具。

本轮物品与 128 细胞 / 75 HP 样例只在临时测试 World 中存在，没有写入正式关卡或存档。
清理移除了临时预览／生成入口和自动化资产副本，没有按文件名中的 Test / Debug 批量删除已有游戏内容。

## 验证

- 移除一次性工具后，UE 5.8 Development Editor 重新编译成功。
- `TheAwakening.Input.UIPlayerIngress` 再次通过：真实背包蓝图可加载，细胞 / HP 更新、格子拖拽、
  输入归属、改绑键盘和手柄边界翻页均正常。
- `git diff --check` 与本地化 JSON 格式检查通过。
- 清理使用已验证的 Saved 绝对路径和原生 PowerShell 文件操作；未处理工作区以外的文件。

`Saved` 是本机生成目录，不作为唯一交付位置。最终规格与结论放在 `Docs`，
原始截图、旧日志及测试导出物可以在后续清理中移除；需要再次打包时重新生成 StagedBuilds。
