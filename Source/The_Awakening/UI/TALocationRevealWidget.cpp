#include "UI/TALocationRevealWidget.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"
#include "Rendering/DrawElements.h"
#include "AkAudioEvent.h"
#include "AkGameplayStatics.h"
#include "Misc/PackageName.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

const FName UTALocationRevealWidget::InEventName(TEXT("Play_UI_LocationReveal_In"));
const FName UTALocationRevealWidget::OutEventName(TEXT("Play_UI_LocationReveal_Out"));

bool FTALocationRevealClock::Request(const FText& C, const FText& L, bool Force, float Opacity)
{
 // Pending is the effective destination during interruption; entering the old area again
 // must replace that destination too. Keep FText histories for live culture changes.
 const FText& CompareC = bPending ? PendingChapter : Chapter;
 const FText& CompareL = bPending ? PendingLocation : Location;
 if (!Force && C.IdenticalTo(CompareC) && L.IdenticalTo(CompareL)) return false;
 ++RequestVersion;
 PendingChapter = C; PendingLocation = L; bPending = true;
 if (Phase == ETALocationRevealPhase::Hidden) StartPending();
 else if (Phase != ETALocationRevealPhase::Interrupting)
 {
  Phase = ETALocationRevealPhase::Interrupting; Elapsed = 0;
  FadeStartOpacity = Opacity;
 }
 return true;
}

void FTALocationRevealClock::StartPending()
{
 Chapter = PendingChapter; Location = PendingLocation; bPending = false;
 PlayingVersion = RequestVersion; bInPosted = bOutPosted = false;
 Phase = ETALocationRevealPhase::Revealing; Elapsed = 0;
}

UTALocationRevealWidget::UTALocationRevealWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
 SetIsFocusable(false);
 SetVisibility(ESlateVisibility::HitTestInvisible);
 SetRenderOpacity(0);
 static ConstructorHelpers::FObjectFinder<UFont> GlobalFont(TEXT("/Game/Fonts/TA_GlobalFont.TA_GlobalFont"));
 Font = GlobalFont.Object;
 TextColor = FLinearColor(FColor(232,229,220));
 TraceColor = FLinearColor(FColor(215,224,219)); TraceColor.A = .62f;
}

bool UTALocationRevealWidget::Initialize()
{
 if (!Super::Initialize()) return false;
 if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this);
 auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget=Root;
 RegionCanvas=WidgetTree->ConstructWidget<UCanvasPanel>(); Root->AddChildToCanvas(RegionCanvas);
 RegionCanvas->SetRenderTransformPivot(FVector2D::ZeroVector);
 RegionCanvas->SetClipping(EWidgetClipping::ClipToBoundsAlways);
 TitleMask=WidgetTree->ConstructWidget<UCanvasPanel>(); RegionCanvas->AddChildToCanvas(TitleMask);
 TitleMask->SetClipping(EWidgetClipping::ClipToBoundsAlways);
 TitleCanvas=WidgetTree->ConstructWidget<UCanvasPanel>(); TitleMask->AddChildToCanvas(TitleCanvas);
 ChapterLabel=WidgetTree->ConstructWidget<UTextBlock>();
 LocationLabel=WidgetTree->ConstructWidget<UTextBlock>();
 TitleCanvas->AddChildToCanvas(ChapterLabel)->SetAutoSize(true);
 TitleCanvas->AddChildToCanvas(LocationLabel)->SetAutoSize(true);
 return true;
}

void UTALocationRevealWidget::UpdateLayout(FVector2D Screen)
{
 if (!RegionCanvas || Screen.X<=0) return;
 const float Scale=Screen.X/1920.f;
 auto* RegionSlot=CastChecked<UCanvasPanelSlot>(RegionCanvas->Slot);
 RegionSlot->SetPosition(FVector2D(Screen.X*ScreenMargin.X,Screen.Y*(1-ScreenMargin.Y)-DesignSize.Y*Scale));
 RegionSlot->SetSize(DesignSize); RegionCanvas->SetRenderScale(FVector2D(Scale));
 const bool Interrupt=Clock.Phase==ETALocationRevealPhase::Interrupting;
 const auto Phase=Interrupt ? FrozenPhase : Clock.Phase;
 const float P=Interrupt ? FrozenProgress : Progress();
 const float Left=Phase==ETALocationRevealPhase::Erasing ? P*DesignSize.X : 0;
 const float Right=Phase==ETALocationRevealPhase::Revealing ? P*DesignSize.X : DesignSize.X;
 TitleMask->SetVisibility(Phase==ETALocationRevealPhase::Revealing || Phase==ETALocationRevealPhase::Holding || Phase==ETALocationRevealPhase::Erasing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
 auto* MaskSlot=CastChecked<UCanvasPanelSlot>(TitleMask->Slot);
 MaskSlot->SetPosition(FVector2D(Left,0)); MaskSlot->SetSize(FVector2D(FMath::Max(0.f,Right-Left),DesignSize.Y));
 auto* TitleSlot=CastChecked<UCanvasPanelSlot>(TitleCanvas->Slot);
 TitleSlot->SetPosition(FVector2D(-Left,0)); TitleSlot->SetSize(DesignSize);
 auto Label=[&](UTextBlock* Block,const FText& Text,FVector2D Offset,int32 Size,int32 Spacing,float Alpha)
 {
  FSlateFontInfo Info(Font,Size,Typeface); Info.LetterSpacing=Spacing;
  Block->SetFont(Info); Block->SetText(bUppercase ? Text.ToUpper() : Text);
  auto Color=TextColor; Color.A*=Alpha; Block->SetColorAndOpacity(Color);
  CastChecked<UCanvasPanelSlot>(Block->Slot)->SetPosition(Offset);
 };
 Label(ChapterLabel,Clock.Chapter,ChapterOffset,ChapterFontSize,ChapterLetterSpacing,ChapterOpacity);
 Label(LocationLabel,Clock.Location,LocationOffset,LocationFontSize,LocationLetterSpacing,1);
}

void UTALocationRevealWidget::ShowLocationReveal(FText ChapterText, FText LocationText, bool Force)
{
 const bool WasHidden = Clock.Phase == ETALocationRevealPhase::Hidden;
 if (Clock.Phase!=ETALocationRevealPhase::Interrupting)
 { FrozenPhase=Clock.Phase; FrozenProgress=Progress(); }
 if (!Clock.Request(ChapterText,LocationText,Force,GetRenderOpacity())) return;
 if (WasHidden) SetRenderOpacity(1); // First frame is empty: zero reveal, no pulse.
 auto Resolve=[](const TSoftObjectPtr<UAkAudioEvent>& Ref) -> UAkAudioEvent*
 {
  if (Ref.IsNull()) return nullptr;
  if (auto* Loaded=Ref.Get()) return Loaded;
  return FPackageName::DoesPackageExist(Ref.ToSoftObjectPath().GetLongPackageName()) ? Ref.LoadSynchronous() : nullptr;
 };
 LoadedInEvent=Resolve(InEvent); LoadedOutEvent=Resolve(OutEvent);
 UpdateLayout(GetCachedGeometry().GetLocalSize());
 InvalidateLayoutAndVolatility();
}

float UTALocationRevealWidget::PhaseDuration() const
{
 switch (Clock.Phase)
 {
 case ETALocationRevealPhase::Revealing: return FMath::Max(.001f,RevealDuration);
 case ETALocationRevealPhase::Holding: return FMath::Max(0.f,HoldDuration);
 case ETALocationRevealPhase::Erasing: return FMath::Max(.001f,EraseDuration);
 case ETALocationRevealPhase::FadingLine: return FMath::Max(.001f,LineFadeDuration);
 case ETALocationRevealPhase::Interrupting: return FMath::Max(.001f,InterruptFadeDuration);
 default: return 0;
 }
}

float UTALocationRevealWidget::Progress() const
{
 return FMath::Clamp(Clock.Elapsed / FMath::Max(.001f,PhaseDuration()),0.f,1.f);
}

void UTALocationRevealWidget::PostCue(bool Entrance, uint64 Version)
{
 if (Version != Clock.RequestVersion || Version != Clock.PlayingVersion) return;
 if (UAkAudioEvent* Event = Entrance ? LoadedInEvent.Get() : LoadedOutEvent.Get())
  UAkGameplayStatics::PostEvent(Event,nullptr,0,FOnAkPostEventCallback(),true);
}

void UTALocationRevealWidget::AdvancePresentation(float Delta)
{
 if (Clock.Phase==ETALocationRevealPhase::Hidden) return;
 float Remaining = FMath::Max(0.f,Delta);
 for (int32 Guard=0; Guard<8 && Clock.Phase!=ETALocationRevealPhase::Hidden; ++Guard)
 {
  const auto Phase = Clock.Phase;
  const float Duration = PhaseDuration();
  const float Step = FMath::Min(Remaining,FMath::Max(0.f,Duration-Clock.Elapsed));
  Clock.Elapsed += Step; Remaining -= Step;
  const uint64 Version = Clock.PlayingVersion;
  if (Phase==ETALocationRevealPhase::Revealing && !Clock.bInPosted && Clock.Elapsed>=FMath::Max(0.f,InEventOffset))
  { Clock.bInPosted=true; PostCue(true,Version); }
  if (Phase==ETALocationRevealPhase::Erasing && !Clock.bOutPosted && Clock.Elapsed>=FMath::Max(0.f,OutEventOffset))
  { Clock.bOutPosted=true; PostCue(false,Version); }
  if (Phase==ETALocationRevealPhase::Interrupting)
   SetRenderOpacity(FMath::Lerp(Clock.FadeStartOpacity,0.f,Progress()));
  else if (Phase==ETALocationRevealPhase::FadingLine) SetRenderOpacity(1-Progress());
  if (Clock.Elapsed < Duration) break;
  Clock.Elapsed = 0;
  switch (Phase)
  {
  case ETALocationRevealPhase::Revealing: Clock.Phase=ETALocationRevealPhase::Holding; break;
  case ETALocationRevealPhase::Holding: Clock.Phase=ETALocationRevealPhase::Erasing; break;
  case ETALocationRevealPhase::Erasing: Clock.Phase=ETALocationRevealPhase::FadingLine; break;
  case ETALocationRevealPhase::Interrupting:
   Clock.StartPending(); SetRenderOpacity(1); break;
  default: Clock.Phase=ETALocationRevealPhase::Hidden; SetRenderOpacity(0); break;
  }
  if (Remaining<=0) break;
 }
 InvalidateLayoutAndVolatility();
}

void UTALocationRevealWidget::NativeTick(const FGeometry& Geometry, float Delta)
{
 Super::NativeTick(Geometry,Delta);
 const bool Active=Clock.Phase!=ETALocationRevealPhase::Hidden;
 AdvancePresentation(Delta);
 if (Active) UpdateLayout(Geometry.GetLocalSize());
}

void UTALocationRevealWidget::NativeDestruct()
{
 ++Clock.RequestVersion; Clock.bPending=false; Clock.Phase=ETALocationRevealPhase::Hidden;
 SetRenderOpacity(0);
 Super::NativeDestruct();
}

int32 UTALocationRevealWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Cull,
 FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool Enabled) const
{
 Layer=Super::NativePaint(Args,G,Cull,Out,Layer,Style,Enabled);
 if (Clock.Phase==ETALocationRevealPhase::Hidden || !Font) return Layer;
 const FVector2D Screen=G.GetLocalSize();
 const float Scale=Screen.X/1920.f;
 const FVector2D Origin(Screen.X*ScreenMargin.X,Screen.Y*(1-ScreenMargin.Y)-DesignSize.Y*Scale);
 const FGeometry Region=G.MakeChild(DesignSize,FSlateLayoutTransform(Scale,Origin));
 const bool Interrupt=Clock.Phase==ETALocationRevealPhase::Interrupting;
 // During interruption preserve the exact visual snapshot; see FrozenPhase/FrozenProgress.
 const auto VisualPhase=Interrupt ? FrozenPhase : Clock.Phase;
 const float P=Interrupt ? FrozenProgress : Progress();
 const bool Reveal=VisualPhase==ETALocationRevealPhase::Revealing;
 const bool Erase=VisualPhase==ETALocationRevealPhase::Erasing;
 const float Sweep=(Reveal || Erase) ? P*DesignSize.X : DesignSize.X;
 Out.PushClip(FSlateClippingZone(Region));
 const float Y=DesignSize.Y-8;
 const FLinearColor Ink=TraceColor*Style.GetColorAndOpacityTint();
 auto Line=[&](TArray<FVector2D> Points)
 {
  if (TraceGlowRadius>0 && TraceGlowOpacity>0)
   for (int32 Ring=3;Ring>=1;--Ring)
   {
    auto Glow=Ink; Glow.A*=TraceGlowOpacity/(Ring*Ring*2.f);
    FSlateDrawElement::MakeLines(Out,Layer+1,Region.ToPaintGeometry(),Points,ESlateDrawEffect::None,Glow,true,LineThickness+TraceGlowRadius*Ring*2.f/3.f);
   }
  FSlateDrawElement::MakeLines(Out,Layer+2,Region.ToPaintGeometry(),Points,ESlateDrawEffect::None,Ink,true,LineThickness);
 };
 const bool Pulse=(Reveal && P>0 && P<.996f) || Erase;
 if (Pulse)
 {
  const float Start=Sweep-PulseWidth*.5f, End=Sweep+PulseWidth*.5f;
  if (Start>0) Line({FVector2D(0,Y),FVector2D(Start,Y)});
  // Exact exported SVG: M0 64 H17 L24 61 L31 69 L40 5 L49 69 L57 64 H80.
  static const FVector2D Points[]={{0,64},{17,64},{24,61},{31,69},{40,5},{49,69},{57,64},{80,64}};
  TArray<FVector2D> Trace;
  for (const auto& V:Points) Trace.Add(FVector2D(Start+V.X*PulseWidth/80,Y+(V.Y-64)*PulseHeight/72));
  Line(Trace);
  if (Erase && End<DesignSize.X) Line({FVector2D(End,Y),FVector2D(DesignSize.X,Y)});
 }
 else if (!Reveal || P>0) Line({FVector2D(0,Y),FVector2D(Sweep,Y)});
 Out.PopClip();
 return Layer+2;
}
