# 日记模型导入工具

`import_diary.py` 用于从 Awakening Journal 原始交付重新建立 `/Game/Models/Diary`。
它不是日常运行或修复脚本；目标目录只要已有资产就拒绝执行，避免覆盖手工调整。

## 导入方式

在文件中将 `SOURCE` 改成原始交付目录（包含 `FBX/`），然后使用项目对应的 UnrealEditor-Cmd：

```powershell
& '<UE_5.6>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' '<project>/The_Awakening.uproject' -run=pythonscript '-script=<project>/Tools/Diary/import_diary.py' -unattended -nop4
```

使用传统 FBX 导入器，保留源法线/切线，不自动建立物理资产。书体与翻动纸共享
`SK_Journal_Skeleton`；交付自带的 A4 收集纸使用独立骨架，避免被错误关联到书本。
验证结果写入 `Saved/DiaryImport/ue_validation.json`。

## 当前使用范围

- 日记运行时：`SK_Journal_Body`、`SK_TurningPage`、书本骨架、相关书体材质。
- 书本动画仅四个：`Book_Open`、`Book_Close`、`Page_Turn_Forward`、`Page_Turn_Backward`。
- 文本模型/折叠动画继续复用 `/Game/Models/Document` 的 6767676767 纸张配置。
- 导入脚本保留完整源包：集成网格 `SK_Journal`、`SK_Collectible_A4` 及其骨架、
  `Document_Extract`、`Document_Insert`、`M_Document_Front/Back` 当前没有接入运行时。
  这是保留资产清单，不是已证明可直接删除的孤立资产：骨架编辑器预览和源包内部仍可能引用它们。
  需要裁剪时先用编辑器 Reference Viewer 检查，再处理原脚本的导入清单；不要直接删磁盘 uasset。

功能说明、验证和临时文件清理记录见 [日记交付记录](../../Docs/Diary3DWrapUp.md)。
