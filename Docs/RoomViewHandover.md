# 房间镜头与黑场：交接说明

更新时间：2026-10-30。

本文说明房间视图系统已实现的部分、关卡中的配置方法、可调参数、实现细节和后续阶段，供接手的同学使用。完整的需求与长期规划见 [RoomVisibilityAndCameraPlan.md](RoomVisibilityAndCameraPlan.md)。

## 1. 当前进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| A | 按区域切换镜头距离与观察范围 | 已实现，已通过 PIE 验收 |
| B | 房间黑场（stencil 底层渲染）+ 门口预览 | 已实现，待 PIE 验收 |
| C | 墙体切除（世界空间区域 + 固定噪声半透明） | 未开始 |
| D | 楼道防探看、交互 / 拾取 / 扫描过滤、QTE 与过场镜头覆盖 | 未开始 |
| E | 离家卸载住宅、医院过门 | 未开始，需先把白盒拆成子关卡 |

## 2. 资产一览

关卡里一律放 BP，不要直接拖 C++ 类。

| 资产 | 路径 | 用途 |
|---|---|---|
| BP_RoomRegion | /Game/Blueprints/RoomView/ | 划定一个房间、走廊或室外区域，决定镜头参数和黑场 |
| BP_RoomPortal | /Game/Blueprints/RoomView/ | 门口预览区域，站进去时两侧房间都正常显示 |
| DA_RoomCamera_Room | /Game/Camera/RoomView/Profiles/ | 小房间：450 cm，左右 40，上下 15，过渡 0.6 秒 |
| DA_RoomCamera_Corridor | /Game/Camera/RoomView/Profiles/ | 走廊：750 cm，左右 100，上下 25，过渡 0.6 秒（与角色蓝图一致） |
| DA_RoomCamera_Outdoor | /Game/Camera/RoomView/Profiles/ | 室外 / 追逐：1000 cm，左右 200，上下 50，过渡 0.8 秒 |
| M_RoomBlackoutPP / MI_RoomBlackoutPP | /Game/Camera/RoomView/Materials/ | 黑场后处理材质，代码自动挂到角色相机上 |
| M_RoomBlackout / MI_RoomBlackout | /Game/Camera/RoomView/Materials/ | 旧方案（材质替换），已停用，可删除 |

三个镜头 Profile 的高度都是 175 cm，与 BP_ThirdPersonCharacter 一致。

## 3. 关卡配置步骤

以“连廊 + 姐姐房”为例。

1. **连廊**：放一个 BP_RoomRegion，把蓝色盒子缩放到罩住连廊。RoomId 填 `Home.Hall`，CameraProfile 选 DA_RoomCamera_Corridor。
2. **姐姐房**：放一个 BP_RoomRegion 罩住房间。RoomId 填 `Home.SisterBedroom`，CameraProfile 保持默认的 DA_RoomCamera_Room。
3. **登记家具**：在姐姐房区域的 DetailActors 里点 +，用吸管逐个选中房间里的家具。与连廊共用的墙、地板、门框不要选。
4. **门口**：在门口放一个 BP_RoomPortal，把黄色盒子拉到盖住门洞，两侧各伸出一点。RoomA 填 `Home.Hall`，RoomB 填 `Home.SisterBedroom`。

配置要点：

- 判定用的是角色脚下的位置（胶囊底部往上 5 cm），盒子要包住角色站立的地面。
- 不规则房间可以在同一个 BP_RoomRegion 上 Add Component 加多个 Box 拼接。Box 不参与碰撞。
- 区域重叠时 Priority 大的优先；同优先级时保持上一帧的区域，不会来回切换。
- RoomId 必须完全一致（区分大小写，不要有空格）。不要用 Actor 名当 RoomId。
- 玩家不在任何区域时，镜头使用角色蓝图上的默认参数，所有房间正常显示。没配区域的旧地图不受影响。
- 客厅和餐厅之间没有隔断，按同一个区域处理。
- 室内门常开、不能交互关闭，所以 Portal 不判断门的开关。

## 4. 可调参数

### 镜头 Profile（DA_RoomCamera_*）

| 参数 | 含义 |
|---|---|
| ArmLengthCm | Camera Boom 长度 |
| HeightOffsetCm | 镜头高度，写入 Boom 的 SocketOffset.Z |
| LookLeftCm / LookRightCm | 鼠标或右摇杆往屏幕左 / 右最多移动多少；可以不对称，填 0 表示不能往那边看 |
| LookUpCm / LookDownCm | 往屏幕上 / 下最多移动多少 |
| BlendSeconds | 进入该区域时镜头过渡的秒数，0 为立即切换 |

几个房间可以共用一个 Profile，需要不同参数时再复制一份。

### BP_RoomPortal

| 参数 | 含义 |
|---|---|
| RoomA / RoomB | 门两侧的 RoomId |
| ExitMarginCm | 已经在预览时，离开的判定范围向外放大多少（默认 30），防止站在门槛上来回闪 |

### 黑场后处理（MI_RoomBlackoutPP）

| 参数 | 默认值 | 含义 |
|---|---|---|
| Tint | (0.04, 0.04, 0.045) | 黑场轮廓的底色 |
| EdgeColor | (0.12, 0.12, 0.13) | 描线颜色 |
| EdgeSensitivity | 4 | 描线强弱 |
| EdgeWidth | 1 | 描线采样间距（像素） |
| BackgroundDepth | 20000 cm | 场景深度超过这个值的像素才当作空白，黑场轮廓只画在空白处 |

## 5. 实现说明

### 代码位置

- `Source/The_Awakening/Camera/TARoomCameraProfile.h`：`FTARoomCameraSettings` 与 `UTARoomCameraProfile`（DataAsset）。
- `Source/The_Awakening/Camera/TARoomRegion.h/.cpp`：`ATARoomRegion`、`ATARoomPortal` 和轻量登记表 `UTARoomRegistrySubsystem`（BeginPlay / EndPlay 成对注册注销，子关卡卸载后自动清除）。
- `Source/The_Awakening/Camera/TARoomViewComponent.h/.cpp`：挂在角色上的 `RoomViewComponent`。
- `The_AwakeningCharacter.cpp`：BeginPlay 里初始化 RoomViewComponent；`DoLook` 只提交观察输入；`ResetCameraForRespawn` 调用 `SnapToCurrentLocation`。

### 动态镜头

- RoomViewComponent 是唯一写入 CameraBoom 的 `TargetArmLength`、`SocketOffset`、`TargetOffset` 的地方。它在 Boom 之前 Tick（Tick 前置依赖），带 FreezeExempt 标签，使用世界时间：扫描冻结时不卡住，暂停时不推进。
- 原 DoLook 的软限位手感原样移到了组件里。偏移仍写世界 Y / Z；左右限位根据相机右向量判断 +Y 在屏幕哪一侧，所以 LookLeft / LookRight 始终对应屏幕方向。
- 从大范围切到小范围时，偏移会跟着收缩的限位平滑收回。
- 初始化和复活后的下一帧会按当前位置直接到位，不从默认镜头慢慢拉过去。
- 没有区域或区域没指定 Profile 时，使用角色蓝图的 CameraDistance、CameraHeightOffset、CameraOffsetLimitX / Z 和 CameraDefaultBlendSeconds。

### 房间黑场

- 可见房间 = 玩家当前所在房间 + 玩家正站在其门口（Portal 一侧是当前房间）的另一侧房间。只看脚下位置，镜头朝向和偏移不会让房间显示出来。
- 黑场时，DetailActors 上可见的 Primitive 组件关闭 `bRenderInMainPass` 和 `bRenderInDepthPass`，打开 `bRenderCustomDepth`，Stencil 设为 3（`ATARoomRegion::BlackoutStencilValue`）。退出黑场或区域 EndPlay 时还原原来的设置。不隐藏 Actor，不改碰撞、Tick、交互和声音。
- Stencil 0、1、2 被扫描特效占用（角色 1，WorldItem 和扫描 2），黑场只用 3。
- RoomViewComponent 初始化时把 MI_RoomBlackoutPP 加到角色相机的后处理 Blendables（`BlackoutPostProcessMaterial`，可在组件上替换）。
- M_RoomBlackoutPP 是 Post Process 材质（After Tonemapping）：像素 Stencil = 3 且 SceneDepth > BackgroundDepth 时，输出 Lerp(Tint, EdgeColor, Edge)，Edge 由 CustomDepth 四邻域拉普拉斯 × EdgeSensitivity 得到；其余像素输出原画面。所以黑场物品始终在所有可见物体之下，只从空白处露出暗色轮廓和描边。
- 项目设置 `r.CustomDepth=3`（带 Stencil）已开启，不需要改。

## 6. 已知限制

- 黑场切换是瞬时的，没有渐变。
- 黑场房间里的灯光、粒子、Decal 不会跟着变化。
- 黑场物品被可见物体完全挡住时就完全看不见，这是预期效果。
- 扫描在黑场期间也可能改写这些组件的 Stencil（序章不解锁扫描，暂未处理）。
- 站在走廊仍可以与黑场房间里的物体交互 / 拾取，计划在阶段 D 处理。
- MCP 的材质工具连不上 PixelDepthOffset 输出，需要时请在材质编辑器里手动连接。

## 7. 后续阶段要点（已与需求方确认）

- **阶段 C 墙体切除**：白盒墙由多块 Level Prototyping mesh 拼成，以后可能换成整块模型，所以切除在材质里按世界空间的“可切除区域”盒子计算，不依赖模型怎么拆分。离遮挡物远时完全切除（Opacity Mask = 0）；靠近时下半截用固定噪声图案半透明、上半截仍全透明，不用 DitherTemporalAA（会拖影）。断面和背面输出纯黑。需要切除的位置：厕所的墙、厨房门等。
- **阶段 D**：楼道往住宅方向的偏移用不对称限位（填 0），再在最终视图上做一层安全约束；交互、拾取、扫描候选按房间可见性过滤，扫描不能看到黑场房间内容；QTE、过场、追逐镜头通过带 Owner 的临时覆盖接入，不要直接写 Boom。
- **阶段 E**：住宅目前都在 Persistent_Level 里，卸载前要先拆成子关卡；医院先在门后放一个占位平台。

## 8. 在编辑器里使用 MCP

编辑器内置的 Unreal MCP 插件已启用（仅 Editor）。本机 8000 端口被 Windows HTTP.sys 占用，启动编辑器时需要指定其他端口：

```bash
UnrealEditor.exe "C:\UnrealProjects\The-Awakening\The_Awakening.uproject" -ModelContextProtocolStartServer -ModelContextProtocolPort=8765
```

接口地址为 `http://127.0.0.1:8765/mcp`。编辑器开着时不要直接运行 Build.bat 编译 C++，否则会触发热重载；先关闭编辑器再编译。
