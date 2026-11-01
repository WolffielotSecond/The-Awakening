#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/Inventory/TAPocketConnectionLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPocketConnectionTest, "TheAwakening.Inventory.Clothing.PocketConnections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTAPocketConnectionTest::RunTest(const FString&)
{
	const FVector2D ImageMin(100, 100), ImageMax(500, 500);
	const auto OneRow = TAPocketConnectionLayout::Calculate(ImageMin, ImageMax, {600, 200}, {800, 300}, {.25, .75}, false);
	TestEqual(TEXT("One row endpoint at near edge and row center"), OneRow[0], FVector2D(600, 250));
	TestEqual(TEXT("Horizontal segment reaches image edge"), OneRow[1], FVector2D(500, 250));
	TestEqual(TEXT("Bottom-left UV converted to image UI space"), OneRow[2], FVector2D(200, 200));
	const auto TwoRows = TAPocketConnectionLayout::Calculate(ImageMin, ImageMax, {600, 200}, {800, 400}, {0, 0}, false);
	TestEqual(TEXT("Two row endpoint at row divider"), TwoRows[0], FVector2D(600, 300));
	TestEqual(TEXT("Zero UV is valid image bottom-left"), TwoRows[2], FVector2D(100, 500));
	const auto Rack = TAPocketConnectionLayout::Calculate(ImageMin, ImageMax, {0, 200}, {80, 400}, {1, 1}, true);
	TestEqual(TEXT("Rack grid endpoint mirrored to right edge"), Rack[0], FVector2D(80, 300));
	TestEqual(TEXT("Rack bend on image left edge"), Rack[1], FVector2D(100, 300));
	TestEqual(TEXT("One UV is image top-right"), Rack[2], FVector2D(500, 100));
	return true;
}
#endif
