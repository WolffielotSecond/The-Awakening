# 背包 UI 阶段交付（2026-10-31）

**顶部 UI、衣服/口袋与衣架换衣布局阶段均已完成并收工，后续制作暂缓。**
衣服三张图片属性、背包/换衣自适应排版、装饰线、装备栏和按键提示定位见 [ClothingLayout.md](ClothingLayout.md)。

本轮完成顶部 Tab、细胞 / HP 显示、基础格子贴图及背景，不代表整个背包的最终美术交付。

## 最终样式

| 项目 | 设置 |
| --- | --- |
| 顺序 | 背包、技能、细胞、HP |
| 顶栏锚点 | `(0.088, 0.036)` 到 `(0.912, 0.102)`，左右各留 8.8% 屏幕宽度 |
| 背包 + 技能宽度 | 屏幕宽度的 53.2% |
| 两处空挡 | 技能到细胞、细胞到 HP，均为整排宽度的 2% |
| 细胞 : HP 宽度 | 剩余宽度按 **2:3** 分配 |
| 造型 | 同高、同斜率的平行四边形，文字正立 |
| 深色 / 浅色 | sRGB `#26343A` / `#B9D7D8` |
| 细胞、HP | 深色实心底、浅色文字，只显示数据，不参与翻页 |
| 细胞、HP 字号 | 顶栏高度 × 0.32 取整；1920×1080 预览为 23，长文本自动缩小 |
| 格子贴图 | `/Game/Textures/Inventory/Grid`，源图 `Content/Textures/Inventory/Grid.png` |
| 格子样式 | sRGB `#799FA7`，2 像素九宫格边距；UI 压缩、sRGB、无 mipmaps，保留原图透明度 |
| 背景遮罩 | `Border_Dim`，sRGB `#101820`，原透明度 0.5 |
| 背景模糊 | `BackgroundBlur_Inventory`，强度 **0.4**，遮罩下方 Z=-1，不拦截输入 |

## 切换与输入

`WBP_InventoryPanel.TabBar_Inventory` 使用原生 `UTAInventoryTabBar`。
选中底板以 0.32 秒 ease in/out 滑动，文字沿斜边换色；连续切换从当前位置继续，重复点击已选页不重播。

按视频位置，背包选中时为 `E → 技能`，技能选中时为 `背包 ← Q`。
Q/E 仅说明默认映射，运行时仍由 `UTAInputIconSubsystem::GetIconForActionForPlayer`
获取现有上一页 / 下一页动作和当前设备的图标，支持改绑与手柄；缺失图标隐藏，设备变化不重置动画。

`M_InventoryTabIcon` 仅在顶部显示层使用
`lerp(DarkColor, LightColor, 1 - texture.RGB)`，alpha 保持不变。
两张图标各用独立动态材质，蓝图直接引用材质以保留打包依赖；原始按键贴图与其他提示不改动。

`SelectInventoryPage` 管理页面和输入归属。鼠标、原按钮、新 Tab、IMC 键盘 / 手柄动作、可点击操作提示共用切页规则。
翻页在两端停止，不循环；边界向外按键不改变页面、动画或当前拖拽。重新打开背包无动画回到第一页。
原 `HorizontalBox_PanelSwitch` 隐藏，保留原按钮委托与本地化文字来源；技能页仍为占位。

## 细胞与生命数据

- 细胞读取 `ATAPlayerState::GetMoney()`，沿用 `UI_Cells`。
- HP 优先读取玩家 `UTAHealthComponent`；角色暂不可用时，读取同一 PlayerState 的 GAS Health 属性。
- 显示当前血量，不另建生命数据；不可用时为 `—`。英文、简体、繁体的 `UI_Inventory_HP` 均为 `HP: {0}`。
- 使用面板已有 Tick 检查变化，只在数值变化或语言刷新时格式化文字。
- 原 `Text_Money` 控件和原生绑定已移除，不保留第二处显示。

## 验证与清理

UE 5.8 Development Editor 编译通过。真实蓝图离屏渲染检查了布局、动画及缩小后的四边框。
`TheAwakening.Input.UIPlayerIngress` 覆盖输入遮盖、无效页码、实际页面 / Tab 同步、
IMC 改绑键盘与手柄边界翻页、细胞 / GAS 血量变化刷新、旧文字移除和已有物品拖拽。

本轮未向正式关卡写入测试物品；预览用的 128 细胞 / 75 HP 仅存在于临时测试环境，没有写入正式数据或存档。
交付清理已移除一次性 `TAInventoryTabs` 资产生成 / 预览命令行工具，已保存资产不依赖该工具。
保留运行时控件、蓝图、贴图、材质和正式自动化回归。调整样式直接编辑上述资产或原生控件。
本机最终预览和最近回归报告保留在 `Saved/`，其他成员应重新运行测试。

后续已继续制作单张衣服图与右侧口袋自适应排版，见 [ClothingLayout.md](ClothingLayout.md)。正式衣服美术、连线、角色展示和技能页内容尚未制作。

## 外套状态调试准备

外套装备栏单独使用 `/Game/Textures/Inventory/Grid_Solid`，源文件为同目录的 `Grid-Solid.png`。
`WBP_InventoryPanel` 默认值 `OuterEquipBackgroundTexture` 指定该贴图；原有 `#799FA7` 调色和九宫格边距保留。
普通口袋及剧情格继续使用原 `Grid`，不改共用格子蓝图。

玩家角色蓝图 `BP_ThirdPersonCharacter` 的 `InventoryComponent.DefaultOuterClothing` 清空，
`DefaultInnerClothing` 保持原配置；不删除外套定义或改写存档。新开局应只保留内衣容量，
外套图和外套口袋隐藏，外套装备栏保留空框。加载已有存档仍会恢复其中保存的外套。
衣服挂机制保留；衣服换装、口袋物品与新布局的自动化验证范围见 ClothingLayout.md。
