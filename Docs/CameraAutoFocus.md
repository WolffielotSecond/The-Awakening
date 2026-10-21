# 角色自动对焦

现有 `FollowCamera` 使用 `UTAAutoFocusCameraComponent`，沿用原来的 CameraBoom、相机名称和 Post Process 设置，没有增加相机或后处理组件。

在 `BP_ThirdPersonCharacter` 中：

- 选择 `FocusPoint`，调整相对位置到胸口。默认相对胶囊中心 Z +45 cm（胶囊中心不是脚底）。也可以将相机的 `Focus Point` 指向其他 Scene Component。
- 选择 `FollowCamera` → `Camera | Auto Focus`，调整 `Focus Interp Speed`（默认 8）。数值越大跟焦越快；0 表示立即对焦。
- `Auto Focus Enabled` 默认开启，可在蓝图中关闭，恢复手动 Focal Distance。
- 蓝图通过 `Get Follow Camera` 读取/设置自动对焦参数，读取 `Current Focal Distance` / `Target Focal Distance`（cm）；镜头切换时可调用 `Reset Auto Focus`，下一次视图更新立即初始化到目标距离。

每帧摄像机视图更新时，先由原 CameraComponent 生成实际世界位置和朝向（包含 Spring Arm 的 Camera Lag、碰撞收缩以及 Camera Additive Offset），再计算：

```text
TargetFocalDistance = max(1 cm, Dot(CameraForward, FocusPointWorld - CameraWorld))
CurrentFocalDistance = FInterpTo(CurrentFocalDistance, TargetFocalDistance, DeltaTime, FocusInterpSpeed)
```

第一次有效更新直接初始化，避免从 0 跳焦。后续平滑插值。对焦点在镜头后方时距离限制为 1 cm，避免负数或 Unreal 中用于关闭景深的 0 值。角色 Tick 因扫描时停关闭时，只要摄像机视图继续更新，对焦仍会更新。

仅覆盖输出视图的 `DepthOfFieldFocalDistance` 及其 override 标记，不修改相机保存的手动值。没有对焦点或关闭自动对焦时直接使用原设置。光圈、焦距、景深强度、Post Process Blend Weight 等均由蓝图手动配置。现有相机的 Post Process Blend Weight 需要大于 0（通常设为 1），自动对焦不会改变它；小于 1 时焦距会按现有后处理混合规则参与混合。

自动化验证：`TheAwakening.Camera.AutoFocus`，覆盖光轴投影、移动摄像机和目标、插值、附加位移、异常方向、手动设置保留，以及现有角色蓝图的组件兼容性。
