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

namespace TAStorySlotTests
{
	/** 一个带 1 格内衬口袋的背包 */
	struct FFixture
	{
		UWorld* World = nullptr;
		UTAInventoryComponent* Inventory = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			AActor* Owner = World->SpawnActor<AActor>();
			Inventory = NewObject<UTAInventoryComponent>(Owner);
			Owner->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			auto* Clothing = NewObject<UTAClothingDefinition>();
			FTAPocketDef Pocket;
			Pocket.SlotCount = 1;
			Clothing->Pockets.Add(Pocket);
			Inventory->DefaultInnerClothing = Clothing;
			Inventory->BeginPlay();
		}

		~FFixture()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		static UTAItemDefinition* MakeStoryItem(int32 SlotIndex, UTAItemDefinition* UpgradesFrom = nullptr)
		{
			auto* Def = NewObject<UTAItemDefinition>();
			Def->ItemType = ETAItemType::QuestItem;
			Def->StorySlotIndex = SlotIndex;
			Def->UpgradesFrom = UpgradesFrom;
			return Def;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAStorySlotBindingTest, "TheAwakening.Inventory.StorySlots.Binding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAStorySlotBindingTest::RunTest(const FString&)
{
	TAStorySlotTests::FFixture F;
	UTAInventoryComponent* Inv = F.Inventory;
	UTAItemDefinition* Diary = F.MakeStoryItem(0);
	UTAItemDefinition* Cracker = F.MakeStoryItem(1);

	TestEqual(TEXT("Story item goes through TryAddItem"), Inv->TryAddItem(Cracker, 3), 1);
	TestEqual(TEXT("Diary added"), Inv->TryAddStoryItem(Diary), 1);
	TestTrue(TEXT("Diary in slot 0"), Inv->GetStorySlots()[0].ItemDef == Diary);
	TestTrue(TEXT("Cracker in slot 1, count 1"), Inv->GetStorySlots()[1].ItemDef == Cracker && Inv->GetStorySlots()[1].Count == 1);
	TestTrue(TEXT("Story items never land in pockets"), Inv->IsSlotEmpty(0));
	TestTrue(TEXT("HasStoryItem"), Inv->HasStoryItem(Diary));
	TestEqual(TEXT("Picking the same story item again adds nothing"), Inv->TryAddItem(Diary), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAStorySlotUpgradeTest, "TheAwakening.Inventory.StorySlots.Upgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAStorySlotUpgradeTest::RunTest(const FString&)
{
	TAStorySlotTests::FFixture F;
	UTAInventoryComponent* Inv = F.Inventory;
	UTAItemDefinition* CrackerV1 = F.MakeStoryItem(1);
	UTAItemDefinition* CrackerV2 = F.MakeStoryItem(1, CrackerV1);

	Inv->TryAddItem(CrackerV1);
	TestEqual(TEXT("Upgrade accepted"), Inv->TryAddItem(CrackerV2), 1);
	TestTrue(TEXT("Upgrade replaces the original in place"), Inv->GetStorySlots()[1].ItemDef == CrackerV2);
	TestTrue(TEXT("Holding the upgrade counts as holding the original"), Inv->HasStoryItem(CrackerV1));
	TestEqual(TEXT("Original cannot downgrade the slot"), Inv->TryAddItem(CrackerV1), 0);
	TestTrue(TEXT("Slot still holds the upgrade"), Inv->GetStorySlots()[1].ItemDef == CrackerV2);

	// 没有原版时直接拿到升级版也能入槽
	TAStorySlotTests::FFixture G;
	TestEqual(TEXT("Upgrade into an empty slot"), G.Inventory->TryAddItem(CrackerV2), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAStorySlotLockedTest, "TheAwakening.Inventory.StorySlots.Locked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAStorySlotLockedTest::RunTest(const FString&)
{
	TAStorySlotTests::FFixture F;
	UTAInventoryComponent* Inv = F.Inventory;
	UTAItemDefinition* Diary = F.MakeStoryItem(0);
	UTAItemDefinition* Cracker = F.MakeStoryItem(1);
	auto* Coin = NewObject<UTAItemDefinition>();
	Inv->TryAddItem(Diary);
	Inv->TryAddItem(Cracker);
	Inv->TryAddItem(Coin);

	const int32 Story0 = UTAInventoryComponent::ToStorySlotUIIndex(0);
	const int32 Story1 = UTAInventoryComponent::ToStorySlotUIIndex(1);
	const int32 Story2 = UTAInventoryComponent::ToStorySlotUIIndex(2);
	TestFalse(TEXT("Story slots cannot swap"), Inv->MoveItemBetweenSlots(Story0, Story1));
	TestFalse(TEXT("Story item cannot move to an empty story slot"), Inv->MoveItemBetweenSlots(Story0, Story2));
	TestFalse(TEXT("Story item cannot move into a pocket"), Inv->MoveItemBetweenSlots(Story0, 0));
	TestFalse(TEXT("Pocket item cannot move into a story slot"), Inv->MoveItemBetweenSlots(0, Story2));
	FTAInventorySlot Dropped;
	TestFalse(TEXT("Story item cannot be dropped"), Inv->DropFromSlot(Story0, 1, Dropped));
	TestEqual(TEXT("Story item is not removed by RemoveFromSlot"), Inv->RemoveFromSlot(Story0, 1), 0);
	TestTrue(TEXT("Slots unchanged"), Inv->GetStorySlots()[0].ItemDef == Diary && Inv->GetStorySlots()[1].ItemDef == Cracker);
	TestTrue(TEXT("Pocket unchanged"), Inv->GetSlots()[0].ItemDef == Coin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAStorySlotRejectTest, "TheAwakening.Inventory.StorySlots.Reject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAStorySlotRejectTest::RunTest(const FString&)
{
	TAStorySlotTests::FFixture F;
	UTAInventoryComponent* Inv = F.Inventory;
	AddExpectedMessage(TEXT("is not bound to a story slot"), EAutomationExpectedErrorFlags::Contains, 2, false);
	AddExpectedMessage(TEXT("is occupied by"), EAutomationExpectedErrorFlags::Contains, 1, false);

	TestEqual(TEXT("Unbound story item rejected"), Inv->TryAddItem(F.MakeStoryItem(-1)), 0);
	TestEqual(TEXT("Out of range slot rejected"), Inv->TryAddItem(F.MakeStoryItem(4)), 0);
	TestTrue(TEXT("Rejected story item does not fall back to pockets"), Inv->IsSlotEmpty(0));

	Inv->TryAddItem(F.MakeStoryItem(2));
	TestEqual(TEXT("Different item for an occupied slot rejected"), Inv->TryAddItem(F.MakeStoryItem(2)), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAStorySlotRollbackTest, "TheAwakening.Inventory.StorySlots.Rollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAStorySlotRollbackTest::RunTest(const FString&)
{
	TAStorySlotTests::FFixture F;
	UTAInventoryComponent* Inv = F.Inventory;
	UTARollbackSubsystem* Rollback = F.World->GetSubsystem<UTARollbackSubsystem>();
	if (!TestNotNull(TEXT("Rollback subsystem"), Rollback)) return false;
	UTAItemDefinition* CrackerV1 = F.MakeStoryItem(1);
	UTAItemDefinition* CrackerV2 = F.MakeStoryItem(1, CrackerV1);
	UTAItemDefinition* Key = F.MakeStoryItem(3);
	Inv->TryAddItem(CrackerV1);

	// 小游戏奖励升级版和新剧情物品，期间死亡：恢复原版、清空新槽
	UObject* Owner = NewObject<UTAHealthTestListener>();
	Rollback->BeginScope(Owner, TEXT("Puzzle"));
	Inv->TryAddItem(CrackerV2);
	Inv->TryAddItem(Key);
	Rollback->RollbackAll();
	TestTrue(TEXT("Upgrade undone"), Inv->GetStorySlots()[1].ItemDef == CrackerV1);
	TestTrue(TEXT("New story item undone"), Inv->GetStorySlots()[3].IsEmpty());
	return true;
}

#endif
