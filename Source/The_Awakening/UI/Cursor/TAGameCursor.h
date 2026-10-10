#pragma once

#include "CoreMinimal.h"
#include "UI/Cursor/TACursorAnimation.h"

class AThe_AwakeningPlayerController;
class UGameViewportClient;
class SWidget;
class FWidgetPath;

/** Viewport-scoped software cursor registration; restores previous mappings on teardown. */
class FTAGameCursor : public TSharedFromThis<FTAGameCursor>
{
public:
    explicit FTAGameCursor(AThe_AwakeningPlayerController* InOwner);
    void Install(UGameViewportClient* Client);
    void Shutdown();
    void Update();
    void Button(bool bDown);
    void ToggleTest();
    bool CanPaint() const;
    static bool IsActionablePath(const FWidgetPath& Path);
#if WITH_DEV_AUTOMATION_TESTS
    // Frozen model, same drawing code as the live cursor, for offscreen visual verification.
    static TSharedRef<SWidget> MakePreview(TSharedRef<FTACursorAnimation> Model, const FTACursorStyleSettings& Style);
#endif
    FTACursorAnimation Animation;
private:
    TWeakObjectPtr<AThe_AwakeningPlayerController> Owner;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    TSharedPtr<SWidget> CursorWidget;
    TArray<TSharedPtr<SWidget>> PreviousCursors;
    bool bPreviousSoftwareEnabled = true;
    TSharedPtr<SWidget> TestWidget;
    uint64 TestRequest = 0;
};
