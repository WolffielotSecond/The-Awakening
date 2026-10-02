#include "Core/TAInputOwnershipAdapter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"

bool TAInputOwnershipAdapter::OwnsInput(const APlayerController* Player, const FVector2D* PointerPosition)
{
	if (!Player || !Player->IsLocalController() || !Player->GetWorld()) return false;
	const auto* Client = Player->GetWorld()->GetGameViewport();
	// Headless automation has no Slate surface. It may exercise policy without UI.
	if (!Client || !Client->Viewport)
	{
#if WITH_DEV_AUTOMATION_TESTS
		return GIsAutomationTesting || IsRunningCommandlet();
#else
		return false;
#endif
	}
#if WITH_EDITOR
	if (Player->GetWorld()->WorldType == EWorldType::PIE)
		if (const auto* Scene = Client->GetGameViewport(); Scene && Scene->GetPlayInEditorIsSimulate()) return false;
#endif
	if (!FSlateApplication::IsInitialized()) return false;
	auto& Slate = FSlateApplication::Get();
	if (!Slate.IsActive()) return false;
	const auto Viewport = Client->GetGameViewportWidget();
	if (!Viewport.IsValid()) return false;
	const int32 User = Player->GetLocalPlayer() ? Player->GetLocalPlayer()->GetControllerId() : 0;
	if (PointerPosition)
	{
		const auto Path = Slate.LocateWindowUnderMouse(*PointerPosition, Slate.GetInteractiveTopLevelWindows(), false, User);
		return Path.ContainsWidget(Viewport.Get());
	}
	return Viewport->HasUserFocus(User) || Viewport->HasUserFocusedDescendants(User);
}
