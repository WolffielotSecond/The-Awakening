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

- 物品拾取/普通预览：`SM_Diary_Closed`，由 `SK_Journal_Body` 的 `Book_Open` 起始合书姿态生成，保留原材质与实际尺寸。
- 日记运行时：`SK_Journal_Body`、`SK_TurningPage`、书本骨架、相关书体材质。
- 书本动画仅四个：`Book_Open`、`Book_Close`、`Page_Turn_Forward`、`Page_Turn_Backward`。
- 文本模型/折叠动画继续复用 `/Game/Models/Document` 的 6767676767 纸张配置。
- 导入脚本保留完整源包：集成网格 `SK_Journal`、`SK_Collectible_A4` 及其骨架、
  `Document_Extract`、`Document_Insert`、Diary 目录内的 `M_Document_Front/Back` 当前没有接入运行时。
  文本默认正反材质来自 `/Game/Models/Document` 下的同名资产，已接入运行时；两组资产不要混淆。
  这是保留资产清单，不是已证明可直接删除的孤立资产：骨架编辑器预览和源包内部仍可能引用它们。
  需要裁剪时先用编辑器 Reference Viewer 检查，再处理原脚本的导入清单；不要直接删磁盘 uasset。

功能说明、验证和临时文件清理记录见 [日记交付记录](../../Docs/Diary3DWrapUp.md)。

## 默认物品模型

`-run=TADiaryItem -AllowCommandletRendering` 生成缺失的 `SM_Diary_Closed`，设置
`DA_Item_Diary.PreviewMesh` 和 `BP_WorldItem_Diary` 的模型，并清除该拾取蓝图的材质覆盖。
世界拾取物平放，普通预览通过 `PreviewTransform` 朝向预览相机。阅读界面仍使用原有动画模型。
修改前备份到 `Saved/DiaryItemBackups/<时间>/`；已有静态模型不重新烘焙。
`-Verify` 只读加载并生成实际拾取 Actor，验证模型、物品定义、材质和合书尺寸；
附加 `-Render` 导出 `Saved/DiaryItemPreview.png`，用于核对普通物品预览外观。
