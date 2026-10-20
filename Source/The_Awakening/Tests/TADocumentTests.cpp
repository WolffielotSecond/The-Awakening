#include "Document/TADocumentSubsystem.h"
#include "Health/TARollbackSubsystem.h"
#include "Tests/TADocumentTestGameInstance.h"
#include "Tests/TAHealthTestListener.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TADocumentTests
{
	/** 测试世界 + 指向它的 GameInstance + 挂在其下的文本子系统 */
	struct FFixture
	{
		UWorld* World = nullptr;
		UTADocumentTestGameInstance* GI = nullptr;
		UTADocumentSubsystem* Documents = nullptr;
		UTARollbackSubsystem* Rollback = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GI = NewObject<UTADocumentTestGameInstance>();
			GI->SetTestWorld(World);
			Documents = NewObject<UTADocumentSubsystem>(GI);
			Rollback = World->GetSubsystem<UTARollbackSubsystem>();
		}

		~FFixture()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADocumentRecordsTest, "TheAwakening.Document.Records",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADocumentRecordsTest::RunTest(const FString&)
{
	TADocumentTests::FFixture F;
	UTADocumentSubsystem* D = F.Documents;
	D->OnDocumentCollected.AddDynamic(F.GI, &UTADocumentTestGameInstance::HandleDocumentCollected);

	AddExpectedMessage(TEXT("empty DocumentId"), EAutomationExpectedErrorFlags::Contains, 2, false);
	TestFalse(TEXT("Empty id is not collected"), D->Collect(NAME_None));
	TestFalse(TEXT("Empty id is not marked read"), D->MarkRead(NAME_None));

	// 未获得日记本时读过：只标记已读，不进日记本
	TestTrue(TEXT("MarkRead a new document"), D->MarkRead(TEXT("Letter_B")));
	TestFalse(TEXT("MarkRead twice returns false"), D->MarkRead(TEXT("Letter_B")));
	TestTrue(TEXT("Read flag set"), D->IsRead(TEXT("Letter_B")));
	TestFalse(TEXT("Reading does not collect"), D->IsCollected(TEXT("Letter_B")));
	TestEqual(TEXT("No pages yet"), D->GetPageCount(), 0);

	TestTrue(TEXT("Collect A"), D->Collect(TEXT("Letter_A")));
	TestTrue(TEXT("Collect B after reading it earlier"), D->Collect(TEXT("Letter_B")));
	TestFalse(TEXT("Collect A twice returns false"), D->Collect(TEXT("Letter_A")));
	TestTrue(TEXT("Collecting marks read"), D->IsRead(TEXT("Letter_A")));

	TestTrue(TEXT("Pages follow collection order"), D->GetCollectedDocuments() == TArray<FName>({ TEXT("Letter_A"), TEXT("Letter_B") }));
	TestEqual(TEXT("Page of B"), D->GetPageOfDocument(TEXT("Letter_B")), 1);
	TestEqual(TEXT("Page of uncollected"), D->GetPageOfDocument(TEXT("Letter_X")), (int32)INDEX_NONE);
	TestEqual(TEXT("Document at page 0"), D->GetDocumentAtPage(0), FName(TEXT("Letter_A")));
	TestEqual(TEXT("Out of range page"), D->GetDocumentAtPage(5), FName(NAME_None));

	TestEqual(TEXT("Collected event count"), F.GI->CollectedEvents.Num(), 2);
	if (F.GI->CollectedEvents.Num() == 2)
	{
		TestEqual(TEXT("Event carries the new page index"), F.GI->CollectedEvents[1].PageIndex, 1);
		TestEqual(TEXT("Event carries the id"), F.GI->CollectedEvents[1].DocumentId, FName(TEXT("Letter_B")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADocumentDiaryPageTest, "TheAwakening.Document.DiaryPage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADocumentDiaryPageTest::RunTest(const FString&)
{
	TADocumentTests::FFixture F;
	UTADocumentSubsystem* D = F.Documents;

	D->SetDiaryPage(3);
	TestEqual(TEXT("Empty diary stays on page 0"), D->GetDiaryPageIndex(), 0);

	D->Collect(TEXT("A"));
	D->Collect(TEXT("B"));
	D->Collect(TEXT("C"));
	D->SetDiaryPage(2);
	TestEqual(TEXT("Valid page kept"), D->GetDiaryPageIndex(), 2);
	D->SetDiaryPage(10);
	TestEqual(TEXT("Clamped to last page"), D->GetDiaryPageIndex(), 2);
	D->SetDiaryPage(-1);
	TestEqual(TEXT("Clamped to first page"), D->GetDiaryPageIndex(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADocumentRollbackTest, "TheAwakening.Document.Rollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADocumentRollbackTest::RunTest(const FString&)
{
	TADocumentTests::FFixture F;
	UTADocumentSubsystem* D = F.Documents;
	if (!TestNotNull(TEXT("Rollback subsystem"), F.Rollback)) return false;

	D->Collect(TEXT("A"));
	D->Collect(TEXT("B"));
	D->SetDiaryPage(1);
	D->MarkRead(TEXT("Seen"));

	// 对话 / 阅读中死亡：期间的收集、已读、翻页全部撤销，之前的记录保留
	UObject* Owner = NewObject<UTAHealthTestListener>();
	F.Rollback->BeginScope(Owner, TEXT("Reading"));
	D->MarkRead(TEXT("Seen"));          // 已读过：不产生记录
	D->Collect(TEXT("Seen"));           // 之前读过的文本被收集
	D->Collect(TEXT("C"));
	D->MarkRead(TEXT("OnlyRead"));
	D->SetDiaryPage(3);
	TestEqual(TEXT("Four undo records"), F.Rollback->GetRecordCount(), 4);
	F.Rollback->RollbackAll();

	TestTrue(TEXT("Collection restored"), D->GetCollectedDocuments() == TArray<FName>({ TEXT("A"), TEXT("B") }));
	TestTrue(TEXT("Previously read document stays read"), D->IsRead(TEXT("Seen")));
	TestFalse(TEXT("Collect undo removes read flag it added"), D->IsRead(TEXT("C")));
	TestFalse(TEXT("MarkRead undone"), D->IsRead(TEXT("OnlyRead")));
	TestEqual(TEXT("Diary page restored"), D->GetDiaryPageIndex(), 1);

	// 正常结束：提交后不撤销
	FTARollbackScopeHandle Handle = F.Rollback->BeginScope(Owner, TEXT("Reading"));
	D->Collect(TEXT("D"));
	F.Rollback->CommitScope(Handle);
	F.Rollback->RollbackAll();
	TestTrue(TEXT("Committed collection kept"), D->IsCollected(TEXT("D")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADocumentSaveDataTest, "TheAwakening.Document.SaveData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADocumentSaveDataTest::RunTest(const FString&)
{
	TADocumentTests::FFixture F;
	UTADocumentSubsystem* D = F.Documents;
	D->Collect(TEXT("A"));
	D->Collect(TEXT("B"));
	D->MarkRead(TEXT("OnlyRead"));
	D->SetDiaryPage(1);

	const FTADocumentSaveData Saved = D->GetSaveData();
	UTADocumentSubsystem* Loaded = NewObject<UTADocumentSubsystem>(F.GI);
	Loaded->RestoreSaveData(Saved);
	TestTrue(TEXT("Order restored"), Loaded->GetCollectedDocuments() == D->GetCollectedDocuments());
	TestTrue(TEXT("Read-only document restored"), Loaded->IsRead(TEXT("OnlyRead")) && !Loaded->IsCollected(TEXT("OnlyRead")));
	TestEqual(TEXT("Page restored"), Loaded->GetDiaryPageIndex(), 1);

	// 脏数据：空 ID、重复项、越界页码；收集的文本一定算已读
	FTADocumentSaveData Dirty;
	Dirty.CollectedDocuments = { TEXT("X"), NAME_None, TEXT("Y"), TEXT("X") };
	Dirty.ReadDocuments = { NAME_None };
	Dirty.DiaryPageIndex = 9;
	Loaded->RestoreSaveData(Dirty);
	TestTrue(TEXT("Empty and duplicate ids dropped"), Loaded->GetCollectedDocuments() == TArray<FName>({ TEXT("X"), TEXT("Y") }));
	TestTrue(TEXT("Collected implies read"), Loaded->IsRead(TEXT("Y")));
	TestFalse(TEXT("Restore replaces old records"), Loaded->IsRead(TEXT("OnlyRead")));
	TestEqual(TEXT("Page clamped"), Loaded->GetDiaryPageIndex(), 1);
	return true;
}

#endif
