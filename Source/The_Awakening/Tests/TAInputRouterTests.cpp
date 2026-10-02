#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TAInputRouter.h"
#include "InputAction.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInputRouterTest, "TheAwakening.Input.Router",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInputRouterTest::RunTest(const FString&)
{
	FTAInputRouter Router;
	UObject* Owner = NewObject<UInputAction>();
	TestTrue(TEXT("Base gameplay"), Router.Allows(ETAInputCapability::Gameplay));
	auto Scan = Router.Acquire(Owner,100,{ETAInputCapability::Scan,ETAInputCapability::Look,ETAInputCapability::Cursor});
	TestFalse(TEXT("Scan blocks movement"),Router.Allows(ETAInputCapability::Gameplay));
	TestTrue(TEXT("Scan allows camera and cursor together"),Router.Allows(ETAInputCapability::Look) && Router.Allows(ETAInputCapability::Cursor));
	auto Menu = Router.Acquire(Owner,200,{ETAInputCapability::Menu});
	auto Puzzle = Router.Acquire(Owner,300,{ETAInputCapability::Puzzle});
	Router.Release(Menu);
	TestTrue(TEXT("Out of order release preserves puzzle"),Router.Allows(ETAInputCapability::Puzzle));
	TestFalse(TEXT("Denied capability does not fall through"),Router.Allows(ETAInputCapability::Look));
	auto Peer = Router.Acquire(Owner,300,{ETAInputCapability::Menu});
	TestTrue(TEXT("Latest equal priority wins"),Router.Allows(ETAInputCapability::Menu));
	Router.Release(Peer); Router.Release(Peer);
	TestTrue(TEXT("Release is idempotent and restores previous owner"),Router.Allows(ETAInputCapability::Puzzle));
	Router.Release(Puzzle);
	TestTrue(TEXT("Scan restored"),Router.Allows(ETAInputCapability::Scan));
	Router.Release(Scan);
	TestTrue(TEXT("Gameplay restored"),Router.Allows(ETAInputCapability::Gameplay));
	Router.Acquire(nullptr,999,{});
	TestTrue(TEXT("Invalid owner never blocks"),Router.Allows(ETAInputCapability::Gameplay));
	return true;
}
#endif
