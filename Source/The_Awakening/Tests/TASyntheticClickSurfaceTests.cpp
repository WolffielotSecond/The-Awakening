#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"

namespace
{
class FClickObserver : public IInputProcessor
{
public:
	int32 Downs = 0, Ups = 0;
	virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override { ++Downs; return true; }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent&) override { ++Ups; return true; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASyntheticClickSurfaceTest, "TheAwakening.Input.SyntheticClickSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTASyntheticClickSurfaceTest::RunTest(const FString&)
{
	// A shared host window is insufficient: sibling controls and another player's
	// viewport must be rejected even when they have the same absolute geometry.
	const auto Host = SNew(SWindow);
	const TSharedPtr<SViewport> Surface = SNew(SViewport);
	const TSharedPtr<SViewport> OtherSurface = SNew(SViewport);
	const auto Target = SNew(SButton);
	auto Path = [&](TSharedPtr<SViewport> Viewport)
	{
		FArrangedChildren Widgets(EVisibility::All);
		Widgets.AddWidget(FArrangedWidget(Host, FGeometry()));
		if (Viewport) Widgets.AddWidget(FArrangedWidget(Viewport.ToSharedRef(), FGeometry()));
		Widgets.AddWidget(FArrangedWidget(Target, FGeometry()));
		return FWidgetPath(Host, Widgets);
	};
	TestTrue(TEXT("Game UI descendant is a legal pointer target"),
		AThe_AwakeningPlayerController::IsSyntheticClickPathWithinPlayerSurface(Path(Surface), Surface));
	TestFalse(TEXT("Sibling control in the same host window is rejected"),
		AThe_AwakeningPlayerController::IsSyntheticClickPathWithinPlayerSurface(Path(nullptr), Surface));
	TestFalse(TEXT("Another player surface in the same window is rejected"),
		AThe_AwakeningPlayerController::IsSyntheticClickPathWithinPlayerSurface(Path(OtherSurface), Surface));
	TestFalse(TEXT("Missing hit path is rejected"),
		AThe_AwakeningPlayerController::IsSyntheticClickPathWithinPlayerSurface(FWidgetPath(), Surface));
	TestFalse(TEXT("Missing player surface is rejected"),
		AThe_AwakeningPlayerController::IsSyntheticClickPathWithinPlayerSurface(Path(Surface), nullptr));
	// The same production predicate also guards the captor override route.
	TestFalse(TEXT("A truncated capture path without the surface is rejected"),
		AThe_AwakeningPlayerController::IsSyntheticClickPathWithinPlayerSurface(Path(nullptr), Surface));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller"), PC) || !TestTrue(TEXT("Slate available"), FSlateApplication::IsInitialized()))
	{
		World->DestroyWorld(false); return false;
	}
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	FTAInputRequest Request; Request.Owner = PC; Request.Allowed = {ETAInputCapability::Confirm};
	const auto Handle = PC->AcquireInputRequest(Request);
	auto Observer = MakeShared<FClickObserver>();
	FSlateApplication::Get().RegisterInputPreProcessor(Observer, 0);
	PC->SimulateSyntheticLeftMouseClick();
	TestEqual(TEXT("Confirm permission without a game surface cannot inject Down into the host"), Observer->Downs, 0);
	TestEqual(TEXT("Rejected click cannot inject Up into the host"), Observer->Ups, 0);
	TestFalse(TEXT("Rejected click leaves recursive guard inactive"), PC->IsSimulatingSyntheticLeftMouseClick());
	PC->NotifyApplicationActivationChanged(false);
	PC->SimulateSyntheticLeftMouseClick();
	TestEqual(TEXT("External ownership loss cannot inject a click"), Observer->Downs, 0);
	PC->NotifyApplicationActivationChanged(true);
	PC->bSimulatingSyntheticLeftMouseClick = true;
	PC->SimulateSyntheticLeftMouseClick();
	TestTrue(TEXT("Nested rejection preserves the outer synthesis guard"), PC->IsSimulatingSyntheticLeftMouseClick());
	PC->bSimulatingSyntheticLeftMouseClick = false;
	FSlateApplication::Get().UnregisterInputPreProcessor(Observer);
	PC->ReleaseInputRequest(Handle);
	World->DestroyWorld(false);
	return true;
}
#endif
