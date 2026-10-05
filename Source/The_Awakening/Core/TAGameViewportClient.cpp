#include "Core/TAGameViewportClient.h"

bool UTAGameViewportClient::HandleNavigation(const uint32 UserIndex, TSharedPtr<SWidget> Destination)
{
	// Slate calls this before SetUserFocus, only for a source path inside this
	// window's viewport. Accept/Back button actions and pointer events do not use it.
	// Consume the navigation attempt without changing focus or application config.
	return true;
}
