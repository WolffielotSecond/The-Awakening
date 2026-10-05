#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TAGameViewportClient.h"
#include "Engine/Engine.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/NavigationConfig.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"

namespace
{
// A deterministic candidate avoids relying on headless hit-test-grid layout.
class SNavigationCandidateButton : public SButton
{
public:
	TWeakPtr<SWidget> Candidate;
	virtual FNavigationReply OnNavigation(const FGeometry&, const FNavigationEvent&) override
	{
		return FNavigationReply::Explicit(Candidate.Pin());
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAScopedNavigationTest, "TheAwakening.Input.ScopedNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTAScopedNavigationTest::RunTest(const FString&)
{
	if (!TestTrue(TEXT("Slate available"), FSlateApplication::IsInitialized())) return false;
	auto& Slate = FSlateApplication::Get();
	const auto Config = Slate.GetNavigationConfig();
	const bool Tab = Config->bTabNavigation, Key = Config->bKeyNavigation, Analog = Config->bAnalogNavigation;
	const auto PreviousFocus = Slate.GetUserFocusedWidget(0);
	TestEqual(TEXT("Engine config selects scoped viewport class"), GEngine->GameViewportClientClass.Get(), UTAGameViewportClient::StaticClass());

	const auto Host = SNew(SWindow);
	const auto Surface = SNew(SViewport);
	const auto Source = SNew(SNavigationCandidateButton);
	const auto Target = SNew(SButton);
	const auto EditorSibling = SNew(SNavigationCandidateButton);
	Source->Candidate = Target;
	EditorSibling->Candidate = Target;
	Surface->SetContent(SNew(SVerticalBox)
		+ SVerticalBox::Slot()[Source]
		+ SVerticalBox::Slot()[Target]);
	Host->SetContent(SNew(SVerticalBox)
		+ SVerticalBox::Slot()[Surface]
		+ SVerticalBox::Slot()[EditorSibling]);
	Slate.RegisterVirtualWindow(Host);
	auto* Client = NewObject<UTAGameViewportClient>(GEngine);
	const auto Scene = MakeShared<FSceneViewport>(Client, Surface);
	Surface->SetViewportInterface(Scene);
	Host->SetViewport(Scene);

	auto Focus = [&](const TSharedRef<SWidget>& Widget)
	{
		TestTrue(TEXT("Focus test surface"), Slate.SetUserFocus(0, Widget, EFocusCause::SetDirectly));
	};
	auto Navigate = [&](EUINavigation Direction, ENavigationGenesis Genesis)
	{
		FWidgetPath Path;
		Slate.GeneratePathToWidgetUnchecked(Slate.GetUserFocusedWidget(0).ToSharedRef(), Path);
		Slate.ProcessReply(Path, FReply::Handled().SetNavigation(Direction, Genesis), nullptr, nullptr, 0);
	};
	Focus(Source);
	// Exercise the engine's raw-key -> navigation reply conversion as well.
	for (const FKey Input : {EKeys::Tab, EKeys::Left, EKeys::Gamepad_DPad_Down})
	{
		const FKeyEvent Event(Input, FModifierKeysState(), 0, false, 0, 0);
		const FReply Reply = Source->OnKeyDown(FGeometry(), Event);
		TestTrue(TEXT("Default key produces a navigation request"), Reply.GetNavigationType() != EUINavigation::Invalid);
		FWidgetPath Path;
		Slate.GeneratePathToWidgetUnchecked(Source, Path);
		Slate.ProcessReply(Path, Reply, nullptr, nullptr, 0);
		TestTrue(TEXT("Raw navigation key cannot move game focus"), Slate.GetUserFocusedWidget(0) == Source);
	}
	for (const EUINavigation Direction : {EUINavigation::Next, EUINavigation::Previous,
		EUINavigation::Left, EUINavigation::Right, EUINavigation::Up, EUINavigation::Down})
	{
		Navigate(Direction, ENavigationGenesis::Keyboard);
		TestTrue(TEXT("Game default navigation cannot move focus"), Slate.GetUserFocusedWidget(0) == Source);
	}
	Navigate(EUINavigation::Down, ENavigationGenesis::Controller);
	TestTrue(TEXT("Controller navigation cannot move game focus"), Slate.GetUserFocusedWidget(0) == Source);
	TestTrue(TEXT("Second local user's attempts use the same viewport policy"), Scene->HandleNavigation(1, Target));

	// Same host window, but a source outside the viewport must bypass the policy.
	Focus(EditorSibling);
	Navigate(EUINavigation::Next, ENavigationGenesis::Keyboard);
	TestTrue(TEXT("Sibling/editor source is still allowed to navigate"), Slate.GetUserFocusedWidget(0) == Target);

	int32 Clicks = 0;
	Target->SetOnClicked(FOnClicked::CreateLambda([&Clicks]() { ++Clicks; return FReply::Handled(); }));
	for (const FKey Input : {EKeys::Enter, EKeys::SpaceBar, EKeys::Virtual_Accept})
	{
		const FKeyEvent Accept(Input, FModifierKeysState(), 0, false, 0, 0);
		Target->OnKeyDown(FGeometry(), Accept);
		Target->OnKeyUp(FGeometry(), Accept);
	}
	TestEqual(TEXT("Enter, Space and virtual Accept still click"), Clicks, 3);
	for (const FKey Input : {EKeys::Escape, EKeys::Virtual_Back})
		TestTrue(TEXT("Back action classification unchanged"), Slate.GetNavigationActionFromKey(
			FKeyEvent(Input, FModifierKeysState(), 0, false, 0, 0)) == EUINavigationAction::Back);
	TestTrue(TEXT("Direct receiver/presentation focus is not navigation"), Slate.SetUserFocus(0, Source, EFocusCause::SetDirectly));
	TestTrue(TEXT("Direct focus remains available"), Slate.GetUserFocusedWidget(0) == Source);
	TestTrue(TEXT("Global config object unchanged"), Slate.GetNavigationConfig() == Config);
	TestEqual(TEXT("Tab flag unchanged"), Config->bTabNavigation, Tab);
	TestEqual(TEXT("Key flag unchanged"), Config->bKeyNavigation, Key);
	TestEqual(TEXT("Analog flag unchanged"), Config->bAnalogNavigation, Analog);

	Slate.ClearUserFocus(0);
	Slate.UnregisterVirtualWindow(Host);
	if (PreviousFocus.IsValid()) Slate.SetUserFocus(0, PreviousFocus, EFocusCause::SetDirectly);
	return true;
}
#endif
