#include "Core/TAInputOwnershipAdapter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"

TAInputOwnershipAdapter::EState TAInputOwnershipAdapter::GetState(const APlayerController* Player, const FVector2D* PointerPosition)
{
	if (!Player || !Player->IsLocalController() || !Player->GetWorld()) return EState::Lost;
	const auto* Client = Player->GetWorld()->GetGameViewport();
	// Headless automation has no Slate surface. It may exercise policy without UI.
	if (!Client || !Client->Viewport)
	{
#if WITH_DEV_AUTOMATION_TESTS
		return GIsAutomationTesting || IsRunningCommandlet() ? EState::Owned : EState::Lost;
#else
		return EState::Lost;
#endif
	}
#if WITH_EDITOR
	if (Player->GetWorld()->WorldType == EWorldType::PIE)
		if (const auto* Scene = Client->GetGameViewport(); Scene && Scene->GetPlayInEditorIsSimulate()) return EState::Lost;
#endif
	if (!FSlateApplication::IsInitialized()) return EState::Lost;
	auto& Slate = FSlateApplication::Get();
	if (!Slate.IsActive()) return EState::Lost;
	const auto Viewport = Client->GetGameViewportWidget();
	if (!Viewport.IsValid()) return EState::Lost;
	const auto Window = Slate.FindWidgetWindow(Viewport.ToSharedRef());
	if (!Window.IsValid() || Window != Slate.GetActiveTopLevelWindow()) return EState::Lost;
	const int32 User = Player->GetLocalPlayer() ? Player->GetLocalPlayer()->GetControllerId() : 0;
	// Pointer hit-testing is an event-delivery query, never the lifecycle observation.
	if (PointerPosition)
	{
		const auto Path = Slate.LocateWindowUnderMouse(*PointerPosition, Slate.GetInteractiveTopLevelWindows(), false, User);
		return Path.ContainsWidget(Viewport.Get()) ? EState::Owned : EState::Lost;
	}
	if (Viewport->HasUserFocus(User) || Viewport->HasUserFocusedDescendants(User)) return EState::Owned;
	// No focus does not prove an external transfer. Do not invent a loss or a timeout.
	// A concrete focus elsewhere in this same window (e.g. editor controls) is Lost.
	return Slate.GetUserFocusedWidget(User).IsValid() ? EState::Lost : EState::Transition;
}

bool TAInputOwnershipAdapter::OwnsInput(const APlayerController* Player, const FVector2D* PointerPosition)
{
	return GetState(Player, PointerPosition) == EState::Owned;
}

bool TAInputOwnershipAdapter::CanApplyPresentation(const APlayerController* Player)
{
	return GetState(Player) != EState::Lost;
}
