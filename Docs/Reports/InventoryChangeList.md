# 背包系统改动清单（对齐新版设计文档）

依据：[InventoryVsDesignDoc.md](InventoryVsDesignDoc.md)。按建议顺序排列，前面的是后面的基础。

## P0 数据层（`Inventory/`）

1. **口袋改为按形状定义**
   - `FTAPocketDef`：新增 `Columns` / `Rows`（例如 1x2、2x4、2x2），`SlotCount` 改为由它们计算，或者保留 `SlotCount` 作为兼容字段。
   - 更新 `DA_Clothing_Inner_Default`、`DA_Clothing_Outer`，并新建白大褂（3 个 1x2）和卫衣（一个 2x4 口袋 + 一个 2x2 帽子）的资产。
   - 需要读档兼容：`RestoreSaveData` 本来就按口袋/格子位置恢复，格子数变化时多出的格子会被丢弃，确认这个行为可以接受。

2. **装备外套时带上口袋里的物品**
   - 新增 `EquipOuterInstance(const FTAClothingInstance&)`，或者把 `EquipOuter` 改成接收实例，保留衣服挂上那件外套原有的物品。
   - 新增 `SwapOuter(FTAClothingInstance& InOut)`：已穿外套时直接交换，被换下的实例（连同物品）交还给衣服挂。现在的 `EquipOuter` 在已有外套时会直接返回 false。
   - 交换和卸下时要处理回滚（Rollback）记录，参考 `TryAddStoryItem` 的写法。

3. **外套增益 / 技能的数据和钩子**
   - `UTAClothingDefinition`：新增增益字段，建议用 GAS 的 `TSubclassOf<UGameplayEffect>` 列表，因为已经有 `TAAttributeSet`；另外加一个专属技能字段（可以先只放 Tag 或 Id）。
   - 在装备、卸下、交换以及读档恢复时应用和移除增益。

4. **穿外套后默认口袋排版调整**：在衣服定义里允许覆盖内衬口袋的显示位置，只改 UI 排版，不改数据。

5. **`Value` 改成 float**（按文档为准）：`UTAItemDefinition::Value` 从 int32 改成 float，同时检查用到它的地方，比如 `ATAWorldItem::TryPickupAsCurrency` 里的 `Value * Count`。

## P1 外套更换（新功能）

6. **衣服挂 Actor `ATAClothingRack`**
   - 持有一个 `FTAClothingInstance`，可以为空；有外套和空衣服挂两种外观，实例变化时刷新模型。
   - 交互后打开外套更换界面；需要扫描高亮时挂上 `UTAScannableComponent`。
   - 存档：衣服挂上的外套和口袋物品要存进关卡状态，目前 `TASaveSubsystem` 没有这部分。

7. **外套更换界面 `UTAClothingSwapWidget`**
   - 左边是衣服挂上的外套格加它的口袋，右边是玩家的外套格加内衬和外套口袋，可以复用 `UTAClothingPanelWidget`。
   - 拖动外套格可以穿上或交换；口袋物品可以在两边拖动（需要支持跨背包组件的移动，新增一个接口，同时接收衣服挂和玩家背包）。
   - 打开期间时停并且无敌（参考 `TAInventoryPanelWidget` 的 `RequestFreeze`），按 ESC 退出。
   - 背包里不能脱外套，现在本来就没有入口，保持即可。

## P1 背包界面（`UI/Inventory/`）

8. **`TAClothingPanelWidget` 按 `Columns` × `Rows` 排格子**，替换现在固定的两列/两行。
9. **外套装备格显示当前外套图标**，替换 `OuterEquipSlot->SetEmpty()`；只能看，不能拖出。

## P1 迷你背包 HUD（新功能）

10. **`UTAMiniInventoryWidget`**，放在 Q 版小人右侧：
    - 排版和背包一致，复用第 8 条的排版逻辑。
    - 空格是空心圆角方块；满格或不可堆叠的物品是白色实心；未满的堆叠按 `Count / MaxStack` 在黑白之间插值。
    - 拾取成功时，被新占用或被补充的格子闪两下；显示 4 秒（游戏内计时），再次拾取刷新计时。
    - 背包满时只显示“背包已满”，不闪烁。
    - 触发：在 `ATAWorldItem::TryPickupAsInventory` 或 `TANearbyPickupComponent` 里拿到本次拾取改动了哪些格子（`TryAddItem` 需要能返回受影响的格子下标），通知 HUD。
11. **“背包已满”文字**：迷你背包上方显示，新增本地化 Key；拾取列表里的 `Pickup_Failed` 可以保留，也可以改成这条。

## P2

12. **技能 / 护符面板**：替换现在的技能页占位，按 5.2 节的规则只允许重新分配。工作量较大，建议单独做。
13. **卫衣帽子**：帽子口袋为空时可以戴在头上伪装成小怪，需要专门的技能键和伪装状态。文档也写了“暂无想法”，可以先等设计确定。
14. **小调整**：拾取提示的 `ModelRotationSpeed`：如果 WBP_PickupBanner 里已经改过这个值，就以 WBP 为准，不动；没改过的话，C++ 默认值从 180 改成 360。

## 不需要改

剧情格、拖拽/堆叠/交换、丢弃落点、拆分、部分丢弃、部分拾取、拾取提示时序、快速查看、背包时停、扫描中打开背包，这些都已经和文档一致。

## 已确定的原则（2026-10-27）

- 代码里多出来、文档没写的东西（武器/耐久度、消耗品、货币等物品类型，`SortInventory`）**先保留**。
- 已经在 WBP 里调过的数值和排版**以 WBP 为准**，C++ 不去覆盖。
- 其余代码和文档冲突的地方**以文档为准**：口袋形状、外套连同物品一起装备、直接交换外套、`Value` 用 float、拾取提示旋转速度用 360°/s。

## 仍需确认

- 衣服挂上的外套和物品是否需要存档（离开关卡后再回来是否还在）？
