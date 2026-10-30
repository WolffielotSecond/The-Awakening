# 背包改动实现结果（2026-10-27）

历史实施记录，反映 2026-10-27 当时的范围与验证结果。当前 UI 状态见
[背包 UI 阶段交付](../InventoryTabs.md)；本阶段已完成，后续背包制作暂缓。
当时按计划跳过了技能面板和卫衣帽子。

## 编译与测试
- `Build.bat The_AwakeningEditor`：成功，项目代码没有警告。
- Inventory 测试 10/10（新增 5 个 + 原有的剧情格测试 5 个）；Pickup 测试 3/3；Interaction 测试 6/6。
- 新增的测试覆盖：口袋形状（包括旧资产回退到 SlotCount）、换外套时物品跟着外套走、交换后的回滚、变动格子的回报、背包和衣服挂之间移动物品。
- 启动日志里有一条引擎的 GameFeatureData 加载 ensure，和这次改动无关。
- **还没有在 PIE 里跑过。**

## 改动的文件
- **修改**：TAClothingTypes.h、TAClothingDefinition.h、TAItemDefinition.h、TAInventoryComponent.h/.cpp、TAWorldItem.cpp、TAClothingPanelWidget.h/.cpp、TAInventoryPanelWidget.cpp、TAInventorySlotWidget.h/.cpp、The_AwakeningCharacter.h/.cpp，以及 en / zh-CN / zh-TW 三个本地化 json。新增 4 个 Key：`Inventory_Full`、`Interact_ClothingRack`、`UI_ClothingRack_Rack`、`UI_ClothingRack_Worn`。
- **新增**：Inventory/TAClothingRack.h/.cpp、UI/Inventory/TAClothingSwapWidget.h/.cpp、UI/Inventory/TAMiniInventoryWidget.h/.cpp、Tests/TAInventoryClothingTests.cpp。

## 主要改动
- 口袋改为 `Columns` × `Rows`；任意一项为 0 时回退到 `SlotCount`，所以现有的 DA 不用改也能用。背包里每个口袋按自己的形状排格子。
- 新增 `EquipOuterInstance` / `SwapOuter`：外套口袋里的物品跟着外套走，两者都会记录回滚。原来的 `EquipOuter(Def)` 仍然可用。`TryAddItem` 新增一个能返回变动格子的重载。
- 衣服定义新增 `GrantedEffects`（GE）和 `ExclusiveAbilityTags`。穿上时通过 ASC 生效，脱下时移除；角色的 ASC 就绪后也会补一次。
- 外套可以选择覆盖内衬口袋的显示位置。
- `Value` 改为 float。货币拾取时对总额取整，钱数仍然是整数。
- 新增背包与外部容器（衣服挂）之间的物品移动。
- 死亡或读档时，外套更换界面会跟背包一起关闭。
- 第 11 条（旋转速度）跳过：WBP_PickupBanner 里已经设成 120，按“以 WBP 为准”的规则保留。

## PIE 检查清单
1. 背包：两件现有衣服仍然是 2 格 + 4 格；外套装备格显示外套图标，但不能拖动。
2. 拾取：角色旁边出现迷你背包，刚变动的格子闪两下；4 秒（游戏内时间）后消失，再次拾取会重新计时。
3. 背包满时拾取：迷你背包上方显示“背包已满”，不闪烁。
4. 衣服挂：在关卡里放一个 `ATAClothingRack`，把 `InitialClothing` 设为 `DA_Clothing_Outer`。交互后打开更换界面。
   - 两个外套格之间拖动可以交换外套。
   - 口袋格之间拖动可以在衣服挂和玩家之间转移物品。
   - 界面打开期间时停并且无敌，ESC 关闭。

## 需要你批准后在编辑器里做（我没有动任何资产）
- **WBP_ClothingSwap**：控件命名为 NamedSlot_RackCoat、NamedSlot_RackClothing、NamedSlot_PlayerCoat、NamedSlot_InnerClothing、NamedSlot_OuterClothing、Text_RackTitle、Text_PlayerTitle。
- **WBP_MiniInventory**：控件命名为 HorizontalBox_Clothing、Text_Full。颜色、圆角、时长、位置偏移都可以在 WBP 里调。
- **衣服挂蓝图**，设置 CoatMesh / EmptyMesh。
- 可选：把两件现有衣服的 DA 改成用 Columns/Rows 配置。

## 已知的不足
- 在 WBP 做好之前，更换界面和迷你背包用的是 C++ 搭的临时布局，比较简陋。
- 迷你背包的排列是近似的：内衬在前、外套在后，从左往右排。如果要和背包完全一致，需要在 WBP 里调整。
- 更换界面里悬停物品还没有信息卡。
- 衣服挂存档只留了 TODO，读档或重新进入关卡后衣服挂会恢复成 InitialClothing。

## 编辑器资产（已通过 MCP 完成，均已保存，未提交）
- **新建** `/Game/UI/Inventory/WBP_ClothingSwap`：半透明暗色背景，左列是衣服挂、右列是身上的衣服；标题用 TA_GlobalFont 24 号。
- **新建** `/Game/UI/Inventory/WBP_MiniInventory`：上方是“背包已满”文字（14 号），下方是格子；颜色、圆角、显示时长和位置偏移在 Class Defaults 里调。
- **新建** `/Game/Inventory/BP_ClothingRack`：占位网格体（有外套时是 Cone，空的时候是 Cylinder），`InitialClothing` 默认是 DA_Clothing_Outer。
- **修改** DA_Clothing_Inner_Default 为 2x1；DA_Clothing_Outer 为 Main 2x1 + Main2 2x2。格子数量和排版都和之前一样，`SlotCount` 保留未改。
- 角色和蓝图的默认值都没改：C++ 会按路径自动加载这两个 WBP。也没有改关卡。

## PIE 检查（更新）
1. 把 BP_ClothingRack 拖进关卡，交互后打开更换界面，标题应该是“衣架 / 身上”。
2. 在外套格之间拖动：外套交换，口袋里的物品跟着外套走。
3. 在口袋格之间拖动：物品在衣服挂和玩家之间转移。
4. 界面打开期间时停、不受伤害，ESC 关闭。
5. 拾取：出现迷你背包，变动的格子闪两下，4 秒后消失；背包满时显示“背包已满”。
6. 背包里口袋的排版和之前一样。
