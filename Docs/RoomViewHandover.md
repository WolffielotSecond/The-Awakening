# 房间镜头与黑场：交接说明

更新时间：2026-10-30。

本文说明房间视图系统已实现的部分、关卡中的配置方法、可调参数、实现细节和后续阶段，供接手的同学使用。完整的需求与长期规划见 [RoomVisibilityAndCameraPlan.md](RoomVisibilityAndCameraPlan.md)。

## 1. 当前进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| A | 按区域切换镜头距离与观察范围 | 已实现，已通过 PIE 验收 |
| B | 当前区域显式指定房间 / Actor 黑场（stencil 底层渲染） | 已实现；本轮验证见文末 |
| C | 墙体切除（世界空间区域 + 固定噪声半透明） | 未开始 |
| D | 楼道防探看、交互 / 拾取 / 扫描过滤、QTE 与过场镜头覆盖 | 未开始 |
| E | 离家卸载住宅、医院过门 | 未开始，需先把白盒拆成子关卡 |

## 2. 资产一览

关卡里一律放 BP，不要直接拖 C++ 类。

| 资产 | 路径 | 用途 |
|---|---|---|
| BP_RoomRegion | /Game/Blueprints/RoomView/ | 划定一个房间、走廊或室外区域，决定镜头参数和黑场 |
| BP_RoomPortal | /Game/Blueprints/RoomView/ | 旧资产兼容壳，已停用，不再放置；现有实例不会影响显示 |
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
3. **登记归属**：在姐姐房区域的 `Room > Included Assets > Included Assets` 数组里，用吸管选择房间包含的 Actor。登记归属本身不会触发黑场；共用物体应按实际需要分配。
4. **配置进入连廊后的黑场**：在 Hall 的 `Blackout Room Ids` 数组中添加 `Home.SisterBedroom`，这样进入 Hall 才会黑场姐姐房登记的物体。
5. **额外墙体**：如进入姐姐房后需要黑场某面墙，将它加入姐姐房 Region 的 `Blackout Actors`。不要求墙登记在任何房间中。
6. 不再放置 Portal。两个黑场数组都为空时，不对任何物体施加黑场。切换到另一个区域时只执行新区域自己的规则。

配置要点：

- 判定用的是角色脚下的位置（胶囊底部往上 5 cm），盒子要包住角色站立的地面。
- 不规则房间可以在同一个 BP_RoomRegion 上 Add Component 加多个 Box 拼接。Box 不参与碰撞。
- 区域重叠时 Priority 大的优先；同优先级时保持上一帧的区域，不会来回切换。
- RoomId 使用稳定名称，不要用 Actor 名；字段类型为 FName（比较不区分大小写），仍建议统一拼写且不带空格。没有集中 ID 表，直接填写并引用。RoomId 为 None 时不接受按 ID 黑场；多个已加载 Region 使用相同 ID 时，其 Included Assets 一并被引用。
- 玩家不在任何区域时，镜头使用角色蓝图上的默认参数，所有房间正常显示。没配区域的旧地图不受影响。
- 客厅和餐厅之间没有隔断，按同一个区域处理。
- 黑场关系单向，不自动对称，也不递归执行目标房间的规则；允许显式指定自己的 RoomId。未被当前规则选中的房间保持正常显示，数量不限。

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

### BP_RoomRegion 的归属与黑场参数

| 参数 | 含义 |
|---|---|
| RoomId | 区域稳定名称，保留原行为 |
| Priority | 重叠时数值高的优先，同级优先保留上一帧区域 |
| CameraProfile | 进入区域时使用的镜头 Profile，行为保持原样 |
| Included Assets | Actor 数组，登记本区域包含的物体，不自动黑场 |
| Blackout Room Ids | FName 数组，进入本区域后黑场这些 ID 对应区域的 Included Assets |
| Blackout Actors | Actor 数组，进入本区域后额外黑场的墙等物体 |

配置保存在关卡的 Region 实例中；镜头参数保存在引用的 Profile 数据资产中。
原 `DetailActors` 已重命名为 `IncludedAssets`，通过 `Config/DefaultEngine.ini` 中的 PropertyRedirect 保留已有序列化引用。旧地图不自动生成 `BlackoutRoomIds`：首次使用新规则时需要显式填写，空数组即无黑场。

旧 Portal 类和资产仅为加载已有地图保留，没有注册、点检测或预览逻辑；无需为了启用新规则先修改地图。后续整理关卡时可删除旧实例。

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
- `Source/The_Awakening/Camera/TARoomRegion.h/.cpp`：`ATARoomRegion` 和轻量登记表 `UTARoomRegistrySubsystem`（BeginPlay / EndPlay 成对注册注销）；`ATARoomPortal` 仅为旧资产的停用兼容壳。
- `Source/The_Awakening/Camera/TARoomViewComponent.h/.cpp`：挂在角色上的 `RoomViewComponent`。
- `The_AwakeningCharacter.cpp`：BeginPlay 里初始化 RoomViewComponent；`DoLook` 只提交观察输入；`ResetCameraForRespawn` 调用 `SnapToCurrentLocation`。

### 动态镜头

- RoomViewComponent 是唯一写入 CameraBoom 的 `TargetArmLength`、`SocketOffset`、`TargetOffset` 的地方。它在 Boom 之前 Tick（Tick 前置依赖），带 FreezeExempt 标签，使用世界时间：扫描冻结时不卡住，暂停时不推进。
- 原 DoLook 的软限位手感原样移到了组件里。偏移仍写世界 Y / Z；左右限位根据相机右向量判断 +Y 在屏幕哪一侧，所以 LookLeft / LookRight 始终对应屏幕方向。
- 从大范围切到小范围时，偏移会跟着收缩的限位平滑收回。
- 初始化和复活后的下一帧会按当前位置直接到位，不从默认镜头慢慢拉过去。
- 没有区域或区域没指定 Profile 时，使用角色蓝图的 CameraDistance、CameraHeightOffset、CameraOffsetLimitX / Z 和 CameraDefaultBlendSeconds。

### 房间黑场

- 黑场 Actor 集合 = 当前区域 BlackoutRoomIds 对应所有已加载区域的 Included Assets ∪ 当前区域 Blackout Actors。只执行当前区域的规则，不递归、不推断邻接关系，不限制正常显示房间的数量。
- 无区域时清除黑场；缺失 ID、None、空引用和已销毁对象被忽略。加载 / 卸载 Region 后重算，当前区域卸载、视图组件 EndPlay、世界子系统 Deinitialize 时清除相应黑场。
- 黑场时，目标 Actor 上可见的 Primitive 组件关闭 `bRenderInMainPass` 和 `bRenderInDepthPass`，打开 `bRenderCustomDepth`，Stencil 设为 3（`ATARoomRegion::BlackoutStencilValue`）。不隐藏 Actor，不改碰撞、Tick、交互和声音；用于墙时碰撞依然存在。
- 原渲染状态由登记表按组件统一保存、去重；跨区域仍被引用的组件保持黑场，不重复覆盖快照。不再被引用的组件还原原有主渲染、深度、自定义深度和 Stencil 设置。
- `IsBlackedOut` 表示该 Region 的 ID 被当前规则指定，不代表其中每个 Actor 的最终状态；共享 Actor 或额外 Blackout Actors 仍按最终组件并集处理。
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

## 9. 2026-10-30 规则修订与验证

本轮按需求将隐式“当前房间 + Portal”规则替换为区域显式黑场列表，镜头 Profile 行为保留。未改写现有地图或 Blueprint 二进制资产；原有 Included Assets 引用通过字段重定向迁移。已有地图的黑场列表初始为空，需要按第 3 节填写，原来的自动邻房黑场不再发生。

验证结果：

- UE 5.8 Development Editor 编译通过。
- `TheAwakening.Camera.RoomVisibility` 自动化 2/2 通过（0 警告、0 失败），覆盖显式房间 / 额外 Actor 黑场、重复与共享引用、跨区域快照还原、无区域恢复、目标 / 当前区域卸载、Priority 与区域 fallback。
- 无界面加载现有 `Persistent_Level` 和两个 BP 成功；姐姐房原来的 9 个家具 / 物体引用全部读入 Included Assets，两个新数组默认空，旧 Portal 实例可加载。未保存地图或蓝图。
- 检查报告：`Saved/Automation/RoomVisibility/index.json`、`Saved/RoomVisibilityValidation/assets.json`。日志：`Saved/Logs/RoomVisibilityTests.log`、`Saved/Logs/RoomVisibilityAssets.log`。
- 人工 PIE 画面验收尚未执行；上述无界面检查不验证最终后处理画面。
