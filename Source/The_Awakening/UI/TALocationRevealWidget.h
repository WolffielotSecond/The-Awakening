#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TALocationRevealWidget.generated.h"

class UFont;
class UAkAudioEvent;
class UCanvasPanel;
class UTextBlock;

UENUM(BlueprintType)
enum class ETALocationRevealPhase : uint8 { Hidden, Revealing, Holding, Erasing, FadingLine, Interrupting };

/** Single synchronous clock: no delayed callbacks can survive a request replacement. */
struct THE_AWAKENING_API FTALocationRevealClock
{
 ETALocationRevealPhase Phase = ETALocationRevealPhase::Hidden;
 float Elapsed = 0, FadeStartOpacity = 0;
 uint64 RequestVersion = 0, PlayingVersion = 0;
 bool bPending = false, bInPosted = false, bOutPosted = false;
 FText Chapter, Location, PendingChapter, PendingLocation;
 bool Request(const FText& InChapter, const FText& InLocation, bool bForce, float Opacity);
 void StartPending();
};

/** Passive HUD. Slate paint is used only for the continuously connected ECG and clipping. */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTALocationRevealWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UTALocationRevealWidget(const FObjectInitializer& Initializer);
 UFUNCTION(BlueprintCallable, Category="UI|Location Reveal")
 void ShowLocationReveal(FText ChapterText, FText LocationText, bool bForceReplay = false);
 UFUNCTION(BlueprintPure, Category="UI|Location Reveal")
 ETALocationRevealPhase GetRevealPhase() const { return Clock.Phase; }

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Timing", meta=(ClampMin="0.001")) float RevealDuration = 1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Timing", meta=(ClampMin="0")) float HoldDuration = 1.75f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Timing", meta=(ClampMin="0.001")) float EraseDuration = 1;
 /** Legacy saved setting, unused: normal exit now continues offscreen at EraseDuration's speed. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Timing", meta=(ClampMin="0.001")) float LineFadeDuration = .25f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Timing", meta=(ClampMin="0.001")) float InterruptFadeDuration = .2f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Layout") FVector2D ScreenMargin = FVector2D(.06, .14);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Layout") FVector2D DesignSize = FVector2D(720,124);
 // Compensate Slate's font metrics for CSS line-height:1 / .92 using this supplied face.
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Layout") FVector2D ChapterOffset = FVector2D(0,12.4);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Layout") FVector2D LocationOffset = FVector2D(0,22.4);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Font") TObjectPtr<UFont> Font;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Font") FName Typeface = TEXT("GoogleSansMedium");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Font") int32 ChapterFontSize = 10;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Font") int32 LocationFontSize = 58;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Font") int32 ChapterLetterSpacing = 380;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Font") int32 LocationLetterSpacing = 130;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Font") bool bUppercase = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Appearance") FLinearColor TextColor;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Appearance") FLinearColor TraceColor;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Appearance") float ChapterOpacity = .72f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Appearance") float LineThickness = 2.5f;
 /** Multi-stroke approximation of the exported 3.2px drop-shadow; no UI material needed. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Appearance") float TraceGlowRadius = 3.2f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Appearance") float TraceGlowOpacity = .22f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Appearance") float PulseWidth = 80;
 /** Total QRS R-peak to S-trough height; approximately 68% above / 32% below baseline. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Appearance") float PulseHeight = 72;
 /** Seconds from phase start. Null soft references are deliberately silent. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Audio") TSoftObjectPtr<UAkAudioEvent> InEvent;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Audio") TSoftObjectPtr<UAkAudioEvent> OutEvent;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Audio", meta=(ClampMin="0")) float InEventOffset = .02f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Location Reveal|Audio", meta=(ClampMin="0")) float OutEventOffset = 0;
 static const FName InEventName;
 static const FName OutEventName;

 // Public deterministic step also used by automation; widget tick supplies presentation delta.
 void AdvancePresentation(float DeltaSeconds);
protected:
 virtual bool Initialize() override;
 virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
 virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
  FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
 virtual void NativeDestruct() override;
private:
#if WITH_DEV_AUTOMATION_TESTS
 friend class FTALocationRevealTest;
#endif
 FTALocationRevealClock Clock;
 ETALocationRevealPhase FrozenPhase = ETALocationRevealPhase::Hidden;
 float FrozenProgress = 0;
 UPROPERTY(Transient) TObjectPtr<UAkAudioEvent> LoadedInEvent;
 UPROPERTY(Transient) TObjectPtr<UAkAudioEvent> LoadedOutEvent;
 UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RegionCanvas;
 UPROPERTY(Transient) TObjectPtr<UCanvasPanel> TitleMask;
 UPROPERTY(Transient) TObjectPtr<UCanvasPanel> TitleCanvas;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> ChapterLabel;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> LocationLabel;
 void UpdateLayout(FVector2D Screen);
 float Progress() const;
 float PhaseDuration() const;
 void PostCue(bool bEntrance, uint64 Version);
};
