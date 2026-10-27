[Game Overview](https://docs.google.com/document/d/1J-uc2OdxDg83_7OZP3RBQze8JxMpgs-_r1e0_SXkI3s/edit?usp=sharing)

[Game Draft](https://docs.google.com/document/d/1H36EjSZLaVtGO1SlZpaHMbM3CGkAK166BxVVpmNv-YU/edit?usp=sharing)

[Google Drive Files](https://drive.google.com/drive/folders/1RaTMevEchUWVm5dZgQtvUQu9nj4xkPAM?usp=sharing)





## 你说的对，但是我只是想放图罢了
<img width="191" height="159" alt="屏幕截图 2026-02-19 145546" src="https://github.com/user-attachments/assets/ae98ef53-3899-4d5b-ab60-f54fc943a7d4" />


## 开发环境

项目已升级到 UE 5.8（Molt），每位成员需要自行安装：

- **Unreal Engine 5.8.4**：在 Epic 启动器中安装，并确认显示为“已安装”（必要时点“验证”）
- **PaperZD 2.2.4**：通过 Fab / Epic 启动器安装到 UE 5.8
- **Wwise 2025.1.11.9262**：体积较大，不进仓库。用 Audiokinetic Launcher 集成到本地项目，部署平台选择 UE58

升级前的 UE 5.6 版本保留在 `pre-molt` 分支。

> 提交前请检查 `The_Awakening.uproject` 中的 `EngineAssociation` 仍为 `"5.8"`。引擎未在启动器中登记时，编辑器会把它改成本机专用的 GUID，其他成员将无法打开项目。

## 开发文档

- [跑酷：严格 Upper/Lower 按键、朝向与旧数据](Docs/DirectionalParkour.md)
- [交互物体与 QTE：Blueprint 操作指南](Docs/InteractableObjects.md)
- [通用交互、世界剧情事件与特殊动作架构](Docs/WorldInteractionSystem.md)
- [QTE 调试组件、参数与事件](Docs/QTEDebugTester.md)
