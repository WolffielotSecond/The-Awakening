#include "Core/TATimeSubsystem.h"
#include "Core/TAFreezeSubsystem.h"
#include "Settings/TAAdvancedDataWidget.h"
#include "Tests/TAHealthTestListener.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TATimeTests
{
	// 时停来源只需要是一个存活的 UObject；UObject 本身是抽象类，借用测试监听类作为具体类型
	UObject* NewSource() { return NewObject<UTAHealthTestListener>(GetTransientPackage()); }

	struct FWorldFixture
	{
		UWorld* World = nullptr;
		UTATimeSubsystem* Time = nullptr;

		FWorldFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Time = World->GetSubsystem<UTATimeSubsystem>();
		}
		~FWorldFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTATimeQTEClockTest, "TheAwakening.Time.UnpausedRealDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTATimeQTEClockTest::RunTest(const FString&)
{
	TATimeTests::FWorldFixture F;
	F.World->DeltaRealTimeSeconds = .25f;
	F.World->DeltaTimeSeconds = .0025f;
	TestEqual(TEXT("Real delta ignores world dilation"), F.Time->GetUnpausedRealDeltaSeconds(), .25f);
	UObject* Freeze = TATimeTests::NewSource();
	F.Time->RequestFreeze(Freeze, .5f);
	TestEqual(TEXT("Real delta ignores partial Freeze"), F.Time->GetUnpausedRealDeltaSeconds(), .25f);
	F.Time->RequestFreeze(Freeze, 1.f);
	TestEqual(TEXT("Real delta ignores full Freeze"), F.Time->GetUnpausedRealDeltaSeconds(), .25f);
	UObject* Pause = TATimeTests::NewSource();
	F.Time->RequestPause(Pause);
	TestEqual(TEXT("Pause stops the QTE delta"), F.Time->GetUnpausedRealDeltaSeconds(), 0.f);
	F.Time->ReleasePause(Pause);
	TestEqual(TEXT("Resume has no accumulated pause catch-up"), F.Time->GetUnpausedRealDeltaSeconds(), .25f);
	F.Time->ReleaseFreeze(Freeze);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTATimePauseSourcesTest, "TheAwakening.Time.PauseSources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTATimePauseSourcesTest::RunTest(const FString&)
{
	TATimeTests::FWorldFixture F;
	if (!TestNotNull(TEXT("Time subsystem"), F.Time)) return false;
	// 测试世界没有 GameMode，引擎会拒绝 SetGamePaused；这里只验证来源计数
	AddExpectedMessage(TEXT("rejected by the game mode"), EAutomationExpectedErrorFlags::Contains, 0, false);

	UObject* Menu = TATimeTests::NewSource();
	UObject* Cutscene = TATimeTests::NewSource();
	F.Time->RequestPause(Menu);
	F.Time->RequestPause(Cutscene);
	F.Time->ReleasePause(Menu);
	TestTrue(TEXT("Releasing one source keeps the other source's pause"), F.Time->HasPauseRequest());
	F.Time->ReleasePause(Menu);
	TestTrue(TEXT("Repeated release of the same source is harmless"), F.Time->HasPauseRequest());
	F.Time->ReleasePause(Cutscene);
	TestFalse(TEXT("Last release ends the pause"), F.Time->HasPauseRequest());

	UObject* Expiring = TATimeTests::NewSource();
	F.Time->RequestPause(Expiring);
	Expiring->MarkAsGarbage();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("Destroyed pause source no longer holds the pause"), F.Time->HasPauseRequest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTATimeGameplayClockTest, "TheAwakening.Time.GameplayClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTATimeGameplayClockTest::RunTest(const FString&)
{
	TATimeTests::FWorldFixture F;
	if (!TestNotNull(TEXT("Time subsystem"), F.Time)) return false;
	UObject* Scan = TATimeTests::NewSource();
	UObject* Puzzle = TATimeTests::NewSource();

	F.Time->AdvanceGameplayTime(1.f);
	TestEqual(TEXT("Normal speed"), F.Time->GetGameplayTime(), 1.0, 0.0001);
	TestEqual(TEXT("Normal scale"), F.Time->GetGameplayTimeScale(), 1.f, 0.0001f);

	F.Time->RequestFreeze(Scan, 0.5f);
	F.Time->AdvanceGameplayTime(1.f);
	TestEqual(TEXT("Partial freeze slows gameplay time like actor dilation"), F.Time->GetGameplayTime(), 1.5, 0.0001);
	TestEqual(TEXT("Partial freeze scale"), F.Time->GetGameplayTimeScale(), 0.5f, 0.0001f);

	F.Time->RequestFreeze(Puzzle);
	F.Time->AdvanceGameplayTime(10.f);
	TestEqual(TEXT("Full freeze stops gameplay time"), F.Time->GetGameplayTime(), 1.5, 0.0001);
	TestEqual(TEXT("Full freeze scale is 0"), F.Time->GetGameplayTimeScale(), 0.f);

	F.Time->ReleaseFreeze(Puzzle);
	TestEqual(TEXT("Releasing puzzle keeps scan strength"), F.Time->GetFreezeStrength(), 0.5f, 0.0001f);
	F.Time->ReleaseFreeze(Scan);
	F.Time->AdvanceGameplayTime(1.f);
	TestEqual(TEXT("Released freeze resumes normal speed"), F.Time->GetGameplayTime(), 2.5, 0.0001);
	TestTrue(TEXT("Facade and execution layer agree"),
		FMath::IsNearlyEqual(F.Time->GetFreezeStrength(), F.World->GetSubsystem<UTAFreezeSubsystem>()->GetFreezeStrength()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTATimeGameplayTimerTest, "TheAwakening.Time.GameplayTimers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTATimeGameplayTimerTest::RunTest(const FString&)
{
	TATimeTests::FWorldFixture F;
	if (!TestNotNull(TEXT("Time subsystem"), F.Time)) return false;
	UObject* Puzzle = TATimeTests::NewSource();

	int32 OneShot = 0;
	FTAGameplayTimerHandle OneShotHandle = F.Time->SetGameplayTimer(FTimerDelegate::CreateLambda([&] { ++OneShot; }), 2.f);
	F.Time->AdvanceGameplayTime(1.f);
	TestEqual(TEXT("Not yet due"), OneShot, 0);
	TestEqual(TEXT("Remaining gameplay time"), F.Time->GetGameplayTimerRemaining(OneShotHandle), 1.f, 0.0001f);
	F.Time->RequestFreeze(Puzzle);
	F.Time->AdvanceGameplayTime(30.f);
	TestEqual(TEXT("Frozen gameplay time does not run timers"), OneShot, 0);
	F.Time->ReleaseFreeze(Puzzle);
	F.Time->AdvanceGameplayTime(1.f);
	TestEqual(TEXT("One-shot fires once when due"), OneShot, 1);
	TestFalse(TEXT("One-shot handle becomes inactive"), F.Time->IsGameplayTimerActive(OneShotHandle));

	int32 Loops = 0;
	FTAGameplayTimerHandle LoopHandle = F.Time->SetGameplayTimer(FTimerDelegate::CreateLambda([&] { ++Loops; }), 1.f, true);
	F.Time->AdvanceGameplayTime(3.5f);
	TestEqual(TEXT("Looping timer catches up within one long frame"), Loops, 3);
	F.Time->ClearGameplayTimer(LoopHandle);
	TestFalse(TEXT("Clear invalidates the handle"), LoopHandle.IsValid());
	F.Time->AdvanceGameplayTime(5.f);
	TestEqual(TEXT("Cleared timer no longer fires"), Loops, 3);

	// 回调里新增计时器：同一次推进内按到期顺序继续处理
	TArray<int32> Order;
	F.Time->SetGameplayTimer(FTimerDelegate::CreateLambda([&]
	{
		Order.Add(1);
		F.Time->SetGameplayTimer(FTimerDelegate::CreateLambda([&] { Order.Add(3); }), 0.f);
	}), 0.5f);
	F.Time->SetGameplayTimer(FTimerDelegate::CreateLambda([&] { Order.Add(2); }), 0.8f);
	F.Time->AdvanceGameplayTime(1.f);
	TestTrue(TEXT("Timers fire in due order, including ones added by callbacks"), Order == TArray<int32>({1, 2, 3}));

	AddExpectedMessage(TEXT("Looping gameplay timer needs a positive duration"), EAutomationExpectedErrorFlags::Contains, 1, false);
	TestFalse(TEXT("Zero-interval loop is rejected"),
		F.Time->SetGameplayTimer(FTimerDelegate::CreateLambda([] {}), 0.f, true).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTATimeAdvancedDataTest, "TheAwakening.Time.AdvancedDataFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTATimeAdvancedDataTest::RunTest(const FString&)
{
	const FText ChinesePattern = FText::FromString(TEXT("{Time}(倍率{Scale})"));
	TestEqual(TEXT("HH:MM:SS with fractional scale"),
		UTAAdvancedDataWidget::FormatGameTime(3725.9, 0.5f, ChinesePattern), FString(TEXT("01:02:05(倍率0.5)")));
	TestEqual(TEXT("Integer scale has no trailing zeros"),
		UTAAdvancedDataWidget::FormatGameTime(59.0, 1.f, ChinesePattern), FString(TEXT("00:00:59(倍率1)")));
	TestEqual(TEXT("Hours keep counting past 99"),
		UTAAdvancedDataWidget::FormatGameTime(360000.0, 0.f, ChinesePattern), FString(TEXT("100:00:00(倍率0)")));
	TestEqual(TEXT("This build's distribution platform comes from config"),
		UTAAdvancedDataWidget::GetPlatformText([](const FString& Id) { return Id; }), FString(TEXT("Steam")));
	return true;
}

#endif
