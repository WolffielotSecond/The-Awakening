# 3D 日记本交付与清点（2026-10-22）

用户本轮反馈为「暂时没有问题」。功能约定见 [文本系统](DocumentSystem.md)，
后续交互验收见 [PIE 清单](DocumentSystemPIEChecklist.md)，输入接入见 [InputRouting](InputRouting.md)。

## 当前交付

- 书体、翻动纸、夹页、抽出阅读共用一个场景捕获和摄像机；读取、旋转、折叠归位不切换展示台。
- 静置只显示右侧可点击文本。翻页前预置目标文本，移动副本跟随书页正面；背面不显示纸片，落页不突然消失。反向翻页时随正面重新露出。
- Sample01/02 复用 6767676767 的模型、材质贴图、Presentation 和四种展示变换；ID、标题、正文与插图保留。纸面临时贴图相同，转译正文各自独立。
- Q/E、LB/RB（PlayStation L1/R1）短按逐页，首尾停止；长按 0.65 秒到首/末页，每次按住只触发一次。底部含两条长按提示，随设备/改键更新，0/1 份时隐藏翻页提示。
- 背包「查看内容」从剧情栏日记图标的位置和尺寸放大、移动至中央并翻开。快捷入口持有图标弱引用，布局就绪后读取实际几何；返回日记再关闭时缩回该图标。
- 首次快捷预览先跨帧发布开书终点骨骼，再显示并进入阅读，避免初始化帧缓存停留；展开/翻页/关闭期间维持输入状态互斥与清理。
- 日记/阅读键盘、手柄统一为 Enhanced Input Action 事件；Slate 只负责物理事件转交。该输入改动不迁移其他菜单的旧接收器。

## 验证与保留证据

- 最后运行的日记组 `TheAwakening.Document.Diary`：6/6 通过，包含真实 IMC 改键和释放、正反翻页遮挡、共享阅读、组件清理及页码。
- 共享输入组 `TheAwakening.Input`：23/23 通过。编译与 `git diff --check` 通过。
- 已检查翻页背面、落页前、长按提示布局、图标尺寸入场和快捷关闭的 GPU 离屏结果。离屏验证不能替代具体关卡的首次快捷入口、焦点和实际设备验收。
- 早前全量文档测试存在既有 `TheAwakening.Document.Reader.TranslationPresentation` 水平居中断言失败；本轮未修改该布局，不能据上述局部通过宣称全量无失败。

保留的本地证据（均在 Git 忽略的 `Saved/`）：

| 类型 | 路径 |
|---|---|
| 最终日记报告/日志 | `Automation/DiaryPageOcclusion/`、`Logs/DiaryPageOcclusionTests.log` |
| 输入回归报告/日志 | `Automation/DiaryIMCInputRegression/`、`Logs/DiaryIMCInputRegression.log` |
| 已知全量文档失败基线 | `Automation/UnifiedPaper/`、`Logs/UnifiedPaperTests.log` |
| 最终视觉截图 | `DiaryPageOcclusion0.7.png`、`DiaryPageOcclusion0.999.png`、`DiaryQuickReturnFinal.png` |
| 原始导入验证 | `DiaryImport/ue_validation.json` |

## 工具与资产清点

可重复使用的导入工具保留在 [Tools/Diary](../Tools/Diary/README.md)。
UI/输入/默认文本的专用命令行入口见文本系统文档；重建布局、迁移默认外观和调整 IMC 均有本地资产备份。

当前未接入的源包资产：`SK_Journal`、`SK_Collectible_A4`、`SK_Collectible_A4_Skeleton`、
`Document_Extract`、`Document_Insert`、`M_Document_Front`、`M_Document_Back`。
四个书本动画之外的动作没有被播放；这些资产先保留完整导入包，未将“未接入”误当成“已确认没有引用”删除。

临时清理仅针对本轮 `Saved/` 生成物，清单位于 `Saved/DiaryCleanup/20261022/manifest.json`。
清除旧截图/HDR、重复渲染和测试日志、过时日记自动化报告、一次性重导入脚本，共 90 个文件、约 17.73 MiB。
保留最终证据、所有资产回滚备份、SaveGames、其他任务文件以及项目源码/资源/构建缓存；本轮没有删除运行时 uasset。
