#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"
#include "Inventory/TAClothingDefinition.h"
#include "Health/TARollbackSubsystem.h"
#include "Tests/TAHealthTestListener.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TAClothingTests
{
	FTAPocketDef MakePocket(int32 Columns, int32 Rows)
	{
		FTAPocketDef Pocket;
		Pocket.Columns = Columns;
		Pocket.Rows = Rows;
		return Pocket;
	}

	UTAClothingDefinition* MakeClothing(ETAClothingLayer Layer, TArray<FTAPocketDef> Pockets)
	{
		auto* Def = NewObject<UTAClothingDefinition>();
		Def->Layer = Layer;
		Def->Pockets = MoveTemp(Pockets);
		return Def;
	}

	UTAItemDefinition* MakeItem(bool bStack, int32 MaxStack = 1)
	{
		auto* Def = NewObject<UTAItemDefinition>();
		Def->bCanStack = bStack;
		Def->MaxStack = MaxStack;
		return Def;
	}

	/** 背包：内衬 InnerPockets，可选开局外套 */
	struct FFixture
	{
		UWorld* World = nullptr;
		UTAInventoryComponent* Inventory = nullptr;

		explicit FFixture(TArray<FTAPocketDef> InnerPockets, UTAClothingDefinition* Outer = nullptr)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			AActor* Owner = World->SpawnActor<AActor>();
			Inventory = NewObject<UTAInventoryComponent>(Owner);
			Owner->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			Inventory->DefaultInnerClothing = MakeClothing(ETAClothingLayer::Inner, MoveTemp(InnerPockets));
			Inventory->DefaultOuterClothing = Outer;
			Inventory->BeginPlay();
		}

		~FFixture()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPocketShapeTest, "TheAwakening.Inventory.Clothing.PocketShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPocketShapeTest::RunTest(const FString&)
{
	using namespace TAClothingTests;
	// 1x2、2x4、2x2 三个口袋
	FFixture F({ MakePocket(1, 2), MakePocket(2, 4), MakePocket(2, 2) });
	TestEqual(TEXT("Total slots = sum of Columns * Rows"), F.Inventory->GetMaxSlots(), 2 + 8 + 4);
	TestEqual(TEXT("GetTotalSlotCount matches"), F.Inventory->DefaultInnerClothing->GetTotalSlotCount(), 14);
	const TArray<FTAPocketRuntime>& Pockets = F.Inventory->GetInnerClothing().Pockets;
	TestTrue(TEXT("Per-pocket slot counts"), Pockets.Num() == 3 && Pockets[0].Slots.Num() == 2 && Pockets[1].Slots.Num() == 8 && Pockets[2].Slots.Num() == 4);

	// 2x4：行优先，第 5 个格子（下标 5）在第 2 行第 1 列
	const FTAPocketDef Grid = MakePocket(2, 4);
	int32 Columns = 0, Rows = 0, Row = 0, Column = 0;
	Grid.GetGridSize(Columns, Rows);
	Grid.GetCellPosition(5, Row, Column);
	TestTrue(TEXT("2x4 grid size"), Columns == 2 && Rows == 4);
	TestTrue(TEXT("2x4 row-major cell"), Row == 2 && Column == 1);

	// 旧资产：没有 Columns / Rows 时回退 SlotCount，摆法沿用两列 / 两行
	FTAPocketDef Legacy;
	Legacy.SlotCount = 3;
	Legacy.SlotSide = ETAPocketSlotSide::Bottom;
	TestFalse(TEXT("Legacy pocket has no grid shape"), Legacy.HasGridShape());
	TestEqual(TEXT("Legacy falls back to SlotCount"), Legacy.GetSlotCount(), 3);
	Legacy.GetGridSize(Columns, Rows);
	TestTrue(TEXT("Legacy Top/Bottom keeps two rows"), Columns == 2 && Rows == 2);
	Legacy.GetCellPosition(2, Row, Column);
	TestTrue(TEXT("Legacy Top/Bottom stays column-major"), Row == 0 && Column == 1);

	FTAPocketDef OnlyColumns;
	OnlyColumns.Columns = 2;
	OnlyColumns.SlotCount = 5;
	TestEqual(TEXT("Half-set shape still falls back to SlotCount"), OnlyColumns.GetSlotCount(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAClothingSwapOuterTest, "TheAwakening.Inventory.Clothing.SwapOuterKeepsItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAClothingSwapOuterTest::RunTest(const FString&)
{
	using namespace TAClothingTests;
	UTAClothingDefinition* CoatA = MakeClothing(ETAClothingLayer::Outer, { MakePocket(1, 2) });
	UTAClothingDefinition* CoatB = MakeClothing(ETAClothingLayer::Outer, { MakePocket(2, 2) });
	UTAItemDefinition* Herb = MakeItem(true, 10);
	UTAItemDefinition* Knife = MakeItem(false);
	FFixture F({ MakePocket(1, 1) }, CoatA);
	UTAInventoryComponent* Inv = F.Inventory;

	// 内衬 1 格 + 外套 A 2 格：草药进内衬，刀进外套
	Inv->TryAddItem(Herb, 3);
	Inv->TryAddItem(Knife);
	TestEqual(TEXT("Coat A worn with 3 slots in total"), Inv->GetMaxSlots(), 3);
	TestTrue(TEXT("Knife is in coat A's pocket"), Inv->GetOuterClothing().Pockets[0].Slots[0].ItemDef == Knife);

	// 衣架上的外套 B 口袋里有 5 个草药
	FTAClothingInstance Rack;
	TestTrue(TEXT("Build coat B"), UTAInventoryComponent::BuildClothingInstance(CoatB, Rack));
	Rack.Pockets[0].Slots[3].ItemDef = Herb;
	Rack.Pockets[0].Slots[3].Count = 5;

	TestFalse(TEXT("EquipOuterInstance refuses while a coat is worn"), Inv->EquipOuterInstance(Rack));
	TestTrue(TEXT("SwapOuter"), Inv->SwapOuter(Rack));
	TestTrue(TEXT("Coat B is worn"), Inv->GetOuterClothing().Definition == CoatB);
	TestEqual(TEXT("Coat B's pocket items came along"), Inv->GetItemCount(Herb), 3 + 5);
	TestEqual(TEXT("Knife left with coat A"), Inv->GetItemCount(Knife), 0);
	TestTrue(TEXT("Rack now holds coat A with the knife"), Rack.Definition == CoatA && Rack.Pockets[0].Slots[0].ItemDef == Knife);
	TestEqual(TEXT("Slots = inner 1 + coat B 4"), Inv->GetMaxSlots(), 5);

	// 衣架为空：脱下外套挂上去；再从衣架穿回
	FTAClothingInstance Empty;
	TestFalse(TEXT("Swap with nothing on either side fails"), [&] { FTAClothingInstance None; FFixture G({ MakePocket(1, 1) }); return G.Inventory->SwapOuter(None); }());
	TestTrue(TEXT("Swap into an empty rack takes the coat off"), Inv->SwapOuter(Empty));
	TestFalse(TEXT("No coat worn"), Inv->HasOuterClothing());
	TestTrue(TEXT("Empty rack got coat B with its herbs"), Empty.Definition == CoatB && Empty.Pockets[0].Slots[3].Count == 5);
	TestTrue(TEXT("EquipOuterInstance keeps pocket items"), Inv->EquipOuterInstance(Empty));
	TestEqual(TEXT("Herbs are back"), Inv->GetItemCount(Herb), 8);

	// 内衬不能当外套换上
	FTAClothingInstance InnerAsCoat;
	UTAInventoryComponent::BuildClothingInstance(MakeClothing(ETAClothingLayer::Inner, { MakePocket(1, 1) }), InnerAsCoat);
	TestFalse(TEXT("Inner layer rejected by SwapOuter"), Inv->SwapOuter(InnerAsCoat));

	// EquipOuter(Def) 仍可用：空口袋
	FFixture H({ MakePocket(1, 1) });
	TestTrue(TEXT("EquipOuter wrapper"), H.Inventory->EquipOuter(CoatA));
	TestEqual(TEXT("Wrapper coat adds empty slots"), H.Inventory->GetMaxSlots(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAClothingSwapRollbackTest, "TheAwakening.Inventory.Clothing.SwapRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAClothingSwapRollbackTest::RunTest(const FString&)
{
	using namespace TAClothingTests;
	UTAClothingDefinition* CoatA = MakeClothing(ETAClothingLayer::Outer, { MakePocket(1, 2) });
	UTAClothingDefinition* CoatB = MakeClothing(ETAClothingLayer::Outer, { MakePocket(2, 2) });
	FFixture F({ MakePocket(1, 1) }, CoatA);
	UTARollbackSubsystem* Rollback = F.World->GetSubsystem<UTARollbackSubsystem>();
	if (!TestNotNull(TEXT("Rollback subsystem"), Rollback)) return false;

	UObject* Owner = NewObject<UTAHealthTestListener>();
	Rollback->BeginScope(Owner, TEXT("Puzzle"));
	FTAClothingInstance Rack;
	UTAInventoryComponent::BuildClothingInstance(CoatB, Rack);
	F.Inventory->SwapOuter(Rack);
	TestTrue(TEXT("Swapped during scope"), F.Inventory->GetOuterClothing().Definition == CoatB);
	Rollback->RollbackAll();
	TestTrue(TEXT("Rollback restores the original coat"), F.Inventory->GetOuterClothing().Definition == CoatA);
	TestEqual(TEXT("Slot layout restored"), F.Inventory->GetMaxSlots(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInventoryChangedSlotsTest, "TheAwakening.Inventory.Clothing.ChangedSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInventoryChangedSlotsTest::RunTest(const FString&)
{
	using namespace TAClothingTests;
	FFixture F({ MakePocket(1, 3) });
	UTAInventoryComponent* Inv = F.Inventory;
	UTAItemDefinition* Herb = MakeItem(true, 5);
	UTAItemDefinition* Knife = MakeItem(false);

	TArray<int32> Changed;
	TestEqual(TEXT("Seven herbs fit"), Inv->TryAddItem(Herb, 7, Changed), 7);
	TestTrue(TEXT("Two new stacks reported"), Changed == TArray<int32>({ 0, 1 }));

	// 补满第 2 叠（2 -> 5），再新开第 3 格
	TestEqual(TEXT("Four more herbs"), Inv->TryAddItem(Herb, 4, Changed), 4);
	TestTrue(TEXT("Topped-up stack and new stack reported"), Changed == TArray<int32>({ 1, 2 }));

	// 背包已满：什么都不改
	TestEqual(TEXT("Knife does not fit"), Inv->TryAddItem(Knife, 1, Changed), 0);
	TestTrue(TEXT("Nothing reported when full"), Changed.IsEmpty());

	// 剧情物品不占口袋格
	auto* Story = NewObject<UTAItemDefinition>();
	Story->ItemType = ETAItemType::QuestItem;
	Story->StorySlotIndex = 0;
	TestEqual(TEXT("Story item added"), Inv->TryAddItem(Story, 1, Changed), 1);
	TestTrue(TEXT("Story item reports no pocket slots"), Changed.IsEmpty());

	// 旧签名仍可用
	FFixture G({ MakePocket(1, 1) });
	TestEqual(TEXT("Old TryAddItem signature"), G.Inventory->TryAddItem(Knife), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInventoryExternalMoveTest, "TheAwakening.Inventory.Clothing.ExternalMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInventoryExternalMoveTest::RunTest(const FString&)
{
	using namespace TAClothingTests;
	FFixture F({ MakePocket(1, 2) });
	UTAInventoryComponent* Inv = F.Inventory;
	UTAItemDefinition* Herb = MakeItem(true, 5);
	UTAItemDefinition* Knife = MakeItem(false);
	Inv->TryAddItem(Herb, 4);
	Inv->TryAddItem(Knife);

	FTAClothingInstance Rack;
	UTAInventoryComponent::BuildClothingInstance(MakeClothing(ETAClothingLayer::Outer, { MakePocket(2, 1) }), Rack);
	Rack.Pockets[0].Slots[0].ItemDef = Herb;
	Rack.Pockets[0].Slots[0].Count = 3;

	TestTrue(TEXT("Herbs merge into the rack stack"), Inv->MoveItemWithExternal(0, Rack, 0, 0, true));
	TestTrue(TEXT("Rack stack capped at MaxStack"), Rack.Pockets[0].Slots[0].Count == 5 && Inv->GetSlots()[0].Count == 2);
	TestTrue(TEXT("Knife moves to the empty rack slot"), Inv->MoveItemWithExternal(1, Rack, 0, 1, true));
	TestTrue(TEXT("Pocket emptied"), Inv->IsSlotEmpty(1) && Rack.Pockets[0].Slots[1].ItemDef == Knife);
	TestTrue(TEXT("Knife swaps back with the herbs"), Inv->MoveItemWithExternal(0, Rack, 0, 1, false));
	TestTrue(TEXT("Swap result"), Inv->GetSlots()[0].ItemDef == Knife && Rack.Pockets[0].Slots[1].ItemDef == Herb);
	TestFalse(TEXT("Invalid rack slot rejected"), Inv->MoveItemWithExternal(0, Rack, 0, 5, true));
	return true;
}

#endif
