#include "UI/Cursor/TAGameCursor.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
    double CursorTime() { return FPlatformTime::Seconds(); }

    class STAGameCursor : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(STAGameCursor) {}
            SLATE_ARGUMENT(TSharedPtr<FTACursorAnimation>, Preview)
            SLATE_ARGUMENT(FTACursorStyleSettings, PreviewStyle)
        SLATE_END_ARGS()
        void Construct(const FArguments& Args, TSharedPtr<FTAGameCursor> InCursor = nullptr, AThe_AwakeningPlayerController* InOwner = nullptr)
        {
            Cursor = InCursor;
            Owner = InOwner;
            Preview = Args._Preview;
            PreviewStyle = Args._PreviewStyle;
            SetVisibility(EVisibility::HitTestInvisible);
            SetClipping(EWidgetClipping::Inherit);
        }
        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(64, 64); }
        virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
            FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle&, bool) const override
        {
            const auto C = Cursor.Pin();
            const auto* PC = Owner.Get();
            if (!Preview && (!C || !PC || !C->CanPaint())) return Layer;
            FTACursorAnimation& Model = Preview ? *Preview : C->Animation;
            if (!Model.IsVisible()) return Layer;
            // The software cursor is painted at Slate's current pointer position, independent of animation.
            if (!Preview) Model.AdvanceTo(CursorTime());
            const auto& Style = Preview ? PreviewStyle : PC->CursorStyle;
            const float Unit = FMath::Max(.5f, Style.UnitSize);
            const FVector2D Center = Geometry.GetLocalSize() * .5;
            const auto Circle = [&](float InnerRadius, float Width, FLinearColor Color, int32 DrawLayer)
            {
                // Keep the antialias fringe INSIDE the requested shape. MakeLines adds
                // coverage outside its stroke and can bridge the small dot/ring gap.
                const float Inner = InnerRadius * Unit, Outer = (InnerRadius + Width) * Unit;
                const float Feather = FMath::Min(.5f, (Outer - Inner) * .5f);
                const bool bDisk = InnerRadius == 0.f;
                const float Radii[] = {Inner, bDisk ? 0.f : Inner + Feather, Outer - Feather, Outer};
                TArray<FSlateVertex> Vertices;
                TArray<SlateIndex> Indices;
                Vertices.Reserve(4 * 65); Indices.Reserve(3 * 64 * 6);
                for (int32 Row = 0; Row < 4; ++Row)
                {
                    FLinearColor VertexColor = Color;
                    if (Row == 3 || (Row == 0 && !bDisk)) VertexColor.A = 0.f;
                    for (int32 I = 0; I <= 64; ++I)
                    {
                        const float Angle = 2.f * PI * I / 64.f;
                        const FVector2D Position = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radii[Row];
                        Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                            Geometry.GetAccumulatedRenderTransform(), FVector2f(Position), FVector2f(.5f, .5f), VertexColor.ToFColor(true)));
                    }
                }
                for (int32 Row = 0; Row < 3; ++Row)
                    for (int32 I = 0; I < 64; ++I)
                    {
                        const SlateIndex A = Row * 65 + I, B = A + 65;
                        Indices.Append({A, B, SlateIndex(A + 1), SlateIndex(A + 1), B, SlateIndex(B + 1)});
                    }
                const auto Resource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
                FSlateDrawElement::MakeCustomVerts(Elements, DrawLayer, Resource, Vertices, Indices, nullptr, 0, 0);
            };
            for (const auto& Pulse : Model.GetPulses())
            {
                FLinearColor Color = Style.Color;
                Color.A *= Pulse.OpacityAt(Model.GetTime());
                Circle(Pulse.RadiusAt(Model.GetTime()), Pulse.Settings.StrokeWidth, Color, Layer);
            }
            Circle(Model.GetRadius(), FMath::Max(.01f, Style.MainStrokeWidth), Style.Color, Layer + 1);
            // Inner radius zero produces a true filled disk using the same smooth geometry.
            Circle(0.f, Model.GetDotRadius(), Style.Color, Layer + 2);
            return Layer + 2;
        }
    private:
        TWeakPtr<FTAGameCursor> Cursor;
        TWeakObjectPtr<AThe_AwakeningPlayerController> Owner;
        TSharedPtr<FTACursorAnimation> Preview;
        FTACursorStyleSettings PreviewStyle;
    };
}

#if WITH_DEV_AUTOMATION_TESTS
TSharedRef<SWidget> FTAGameCursor::MakePreview(TSharedRef<FTACursorAnimation> Model, const FTACursorStyleSettings& Style)
{
    return SNew(STAGameCursor).Preview(Model).PreviewStyle(Style);
}
#endif

bool FTAGameCursor::IsActionablePath(const FWidgetPath& Path)
{
    bool bActionable = false;
    for (int32 I = 0; I < Path.Widgets.Num(); ++I)
    {
        if (!Path.Widgets[I].Widget->IsEnabled()) return false;
        bActionable |= Path.Widgets[I].Widget->IsInteractable();
    }
    return bActionable;
}

FTAGameCursor::FTAGameCursor(AThe_AwakeningPlayerController* InOwner) : Owner(InOwner) {}

void FTAGameCursor::Install(UGameViewportClient* Client)
{
    if (!Client || CursorWidget) return;
    Viewport = Client;
    CursorWidget = SNew(STAGameCursor, AsShared(), Owner.Get());
    bPreviousSoftwareEnabled = Client->GetIsUsingSoftwareCursorWidgets();
    PreviousCursors.SetNum(EMouseCursor::TotalCursorCount);
    for (int32 Type = EMouseCursor::Default; Type < EMouseCursor::TotalCursorCount; ++Type)
    {
        const auto CursorType = static_cast<EMouseCursor::Type>(Type);
        PreviousCursors[Type] = Client->GetSoftwareCursorWidget(CursorType);
        Client->SetSoftwareCursorWidget(CursorType, CursorWidget);
    }
    Client->SetUseSoftwareCursorWidgets(true);
}

void FTAGameCursor::Shutdown()
{
    if (TestWidget) ToggleTest();
    if (auto* Client = Viewport.Get())
    {
        bool bReplaced = false;
        for (int32 Type = EMouseCursor::Default; Type < PreviousCursors.Num(); ++Type)
        {
            const auto CursorType = static_cast<EMouseCursor::Type>(Type);
            if (Client->GetSoftwareCursorWidget(CursorType) == CursorWidget)
                Client->SetSoftwareCursorWidget(CursorType, PreviousCursors[Type]);
            else bReplaced = true;
        }
        if (!bReplaced) Client->SetUseSoftwareCursorWidgets(bPreviousSoftwareEnabled);
    }
    Animation.Hide();
    CursorWidget.Reset();
    PreviousCursors.Reset();
    Viewport.Reset();
}

bool FTAGameCursor::CanPaint() const
{
    const auto* PC = Owner.Get();
    return PC && PC->bShowMouseCursor && PC->OwnsPlayerInput() && Animation.IsVisible();
}

void FTAGameCursor::Update()
{
    auto* PC = Owner.Get();
    if (!PC || !FSlateApplication::IsInitialized()) { Animation.Hide(); return; }
    if (!CursorWidget) Install(PC->GetWorld() ? PC->GetWorld()->GetGameViewport() : nullptr);
    auto& App = FSlateApplication::Get();
    const FVector2D Position = App.GetCursorPos();
    const int32 User = PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetControllerId() : 0;
    const auto Path = App.LocateWindowUnderMouse(Position, App.GetInteractiveTopLevelWindows(), false, User);
    const auto Surface = Viewport.IsValid() ? Viewport->GetGameViewportWidget() : nullptr;
    bool bVisible = PC->bShowMouseCursor && PC->OwnsPlayerInput(&Position) && Surface.IsValid()
        && Path.IsValid() && Path.ContainsWidget(Surface.Get());
    bool bActionable = false;
    if (bVisible)
    {
        const FPointerEvent Query(User, 0, Position, Position, App.GetPressedMouseButtons(), EKeys::Invalid, 0.f, App.GetModifierKeys());
        bool bCursorResolved = false;
        bActionable = IsActionablePath(Path);
        for (int32 I = Path.Widgets.Num() - 1; I >= 0; --I)
        {
            const auto& Arranged = Path.Widgets[I];
            if (!bCursorResolved)
            {
                const auto Reply = Arranged.Widget->OnCursorQuery(Arranged.Geometry, Query);
                if (Reply.IsEventHandled())
                {
                    bCursorResolved = true;
                    bVisible &= Reply.GetCursorType() != EMouseCursor::None;
                }
            }
        }
    }
    Animation.Update(CursorTime(), bVisible, bActionable, PC->CursorStyle, PC->HoverPulseSettings);
}

void FTAGameCursor::Button(bool bDown)
{
    Update();
    if (bDown) Animation.Press(CursorTime());
    else Animation.Release(CursorTime());
}

void FTAGameCursor::ToggleTest()
{
    auto* PC = Owner.Get();
    auto* Client = Viewport.Get();
    if (!PC || !Client) return;
    if (TestWidget)
    {
        Client->RemoveViewportWidgetContent(TestWidget.ToSharedRef());
        TestWidget.Reset();
        PC->ReleaseInputRequest(TestRequest);
        TestRequest = 0;
        return;
    }
    TSharedRef<SVerticalBox> Controls = SNew(SVerticalBox);
    Controls->AddSlot().AutoHeight().Padding(0, 0, 0, 16)
        [SNew(STextBlock).Text(FText::FromString(TEXT("HOVER PULSE SETTINGS")))];
    const TWeakObjectPtr<AThe_AwakeningPlayerController> WeakOwner = PC;
    const auto AddSlider = [&](const TCHAR* Label, float FTACursorPulseSettings::*Field, float Min, float Max)
    {
        Controls->AddSlot().AutoHeight().Padding(0, 6)
        [SNew(STextBlock).Text_Lambda([WeakOwner, Label, Field]()
        {
            const auto* OwnerPC = WeakOwner.Get();
            return FText::FromString(FString::Printf(TEXT("%s: %.2f"), Label, OwnerPC ? OwnerPC->HoverPulseSettings.*Field : 0.f));
        })];
        Controls->AddSlot().AutoHeight()
        [SNew(SSlider).MinValue(Min).MaxValue(Max)
            .Value_Lambda([WeakOwner, Field]() { const auto* P = WeakOwner.Get(); return P ? P->HoverPulseSettings.*Field : 0.f; })
            .OnValueChanged_Lambda([WeakOwner, Field](float Value) { if (auto* P = WeakOwner.Get()) P->HoverPulseSettings.*Field = Value; })];
    };
    AddSlider(TEXT("Opacity"), &FTACursorPulseSettings::Opacity, 0, 1);
    AddSlider(TEXT("Interval (s)"), &FTACursorPulseSettings::Interval, .05f, 2.f);
    AddSlider(TEXT("Duration (s)"), &FTACursorPulseSettings::Duration, .05f, 3.f);
    AddSlider(TEXT("Expansion (units)"), &FTACursorPulseSettings::ExpansionDistance, 0, 5.f);
    AddSlider(TEXT("Stroke (units)"), &FTACursorPulseSettings::StrokeWidth, .05f, 1.5f);
    Controls->AddSlot().AutoHeight().Padding(0, 20, 0, 0)
        [SNew(SButton).Text(FText::FromString(TEXT("Reset pulse settings"))).OnClicked_Lambda([WeakOwner]()
        { if (auto* P = WeakOwner.Get()) P->HoverPulseSettings = FTACursorPulseSettings(); return FReply::Handled(); })];
    Controls->AddSlot().AutoHeight().Padding(0, 12)
        [SNew(SButton).Text(FText::FromString(TEXT("Close test"))).OnClicked_Lambda([WeakOwner]()
        { if (auto* P = WeakOwner.Get()) P->TACursorTest(); return FReply::Handled(); })];

    TSharedRef<SVerticalBox> Area = SNew(SVerticalBox);
    Area->AddSlot().AutoHeight().Padding(0, 0, 0, 24)
        [SNew(STextBlock).Text(FText::FromString(TEXT("CURSOR TEST\nMove while clicking / holding. Hover for pulses.\nEmpty area below is non-interactive. Disabled button gives no hover feedback.")))];
    for (int32 I = 0; I < 3; ++I)
        Area->AddSlot().AutoHeight().Padding(0, 8)
        [SNew(SButton).ContentPadding(FMargin(20)).Text(FText::FromString(FString::Printf(TEXT("Interactive target %d"), I + 1)))];
    Area->AddSlot().AutoHeight().Padding(0, 8)
        [SNew(SButton).IsEnabled(false).Text(FText::FromString(TEXT("Disabled target")))];
    Area->AddSlot().FillHeight(1)
        [SNew(SBox).MinDesiredHeight(200)[SNew(STextBlock).Text(FText::FromString(TEXT("Empty test area")))]];
    TestWidget = SNew(SBorder).Padding(40).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.025f, .035f, .05f, 1.f))
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 60, 0)[Area]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(340)[Controls]]];
    Client->AddViewportWidgetContent(TestWidget.ToSharedRef(), 10000);
    FTAInputRequest Request;
    Request.Owner = PC;
    Request.Priority = 10000;
    Request.Allowed = {ETAInputCapability::Cursor, ETAInputCapability::Confirm};
    Request.Presentation.bShowCursor = true;
    Request.Presentation.InputMode = ETAInputModeRequirement::UIOnly;
    TestRequest = PC->AcquireInputRequest(Request);
}
