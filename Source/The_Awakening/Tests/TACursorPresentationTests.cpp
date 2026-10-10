#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/Cursor/TAGameCursor.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACursorHoverTest, "TheAwakening.Cursor.HoverPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACursorHoverTest::RunTest(const FString&)
{
    const auto Window = SNew(SWindow);
    const auto Button = SNew(SButton);
    const auto Parent = SNew(SBorder);
    const auto Text = SNew(STextBlock).Text(FText::FromString(TEXT("Information")));
    const auto MakePath = [&](TSharedRef<SWidget> Leaf)
    {
        FArrangedChildren Children(EVisibility::All);
        Children.AddWidget(FArrangedWidget(Parent, FGeometry()));
        Children.AddWidget(FArrangedWidget(Leaf, FGeometry()));
        return FWidgetPath(Window, Children);
    };
    TestTrue(TEXT("Enabled button is actionable"), FTAGameCursor::IsActionablePath(MakePath(Button)));
    Button->SetEnabled(false);
    TestFalse(TEXT("Disabled button is not actionable"), FTAGameCursor::IsActionablePath(MakePath(Button)));
    Button->SetEnabled(true); Parent->SetEnabled(false);
    TestFalse(TEXT("Disabled ancestor blocks actionable descendants"), FTAGameCursor::IsActionablePath(MakePath(Button)));
    Parent->SetEnabled(true);
    TestFalse(TEXT("Informational text and containers are not actionable"), FTAGameCursor::IsActionablePath(MakePath(Text)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACursorVisualTest, "TheAwakening.Cursor.Visual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACursorVisualTest::RunTest(const FString&)
{
    if (GUsingNullRHI) { AddInfo(TEXT("Visual rendering requires an RHI; run this test without -NullRHI.")); return true; }
    FTACursorStyleSettings Style;
    FTACursorPulseSettings Pulse;
    Pulse.Interval = .2f; Pulse.Duration = 1.f;
    const auto Grid = SNew(SGridPanel);
    const TCHAR* Labels[] = {TEXT("Default"), TEXT("Hover"), TEXT("Hover + hold"), TEXT("Pulse + hold"), TEXT("Overlap"), TEXT("Hidden")};
    for (int32 I = 0; I < 6; ++I)
    {
        auto Model = MakeShared<FTACursorAnimation>();
        Model->Update(0, true, I != 0, Style, Pulse);
        if (I == 2 || I == 3) Model->Press(0);
        Model->AdvanceTo(.15);
        if (I == 3) Model->AdvanceTo(.32);
        if (I == 4) { Model->AdvanceTo(.21); Model->AdvanceTo(.41); Model->AdvanceTo(.61); Model->AdvanceTo(.7); }
        if (I == 5) Model->Hide();
        Grid->AddSlot(I % 3, I / 3).Padding(12)
        [SNew(SBox).WidthOverride(200).HeightOverride(140)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [SNew(STextBlock).Text(FText::FromString(Labels[I]))]
                + SVerticalBox::Slot().FillHeight(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [FTAGameCursor::MakePreview(Model, Style)]]];
    }
    const auto Sheet = SNew(SBorder).Padding(8).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.025f, .035f, .05f, 1.f))[Grid];
    FWidgetRenderer* Renderer = new FWidgetRenderer(true);
    UTextureRenderTarget2D* Target = Renderer->DrawWidget(Sheet, FVector2D(688, 344));
    if (!TestNotNull(TEXT("Cursor preview render target"), Target)) { BeginCleanup(Renderer); return false; }
    FlushRenderingCommands();
    TArray<FColor> Pixels;
    FReadSurfaceDataFlags ReadFlags;
    ReadFlags.SetLinearToGamma(false);
    TestTrue(TEXT("Preview pixels readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags));
    if (Pixels.Num() == 688 * 344)
    {
        const FColor Background = Pixels[0];
        const FColor Gap = Pixels[97 * 688 + 577];
        TestTrue(TEXT("Held hovered cursor keeps a clear gap at its native display size"),
            FMath::Abs(int32(Gap.R) - Background.R) <= 4 && FMath::Abs(int32(Gap.G) - Background.G) <= 4
            && FMath::Abs(int32(Gap.B) - Background.B) <= 4);
        TestTrue(TEXT("Center dot is solid"), Pixels[97 * 688 + 568].R > 245 && Pixels[97 * 688 + 574].R > 245);
        TestTrue(TEXT("Main ring stays opaque while held"), Pixels[97 * 688 + 579].R > 245);
        TestEqual(TEXT("Hidden cursor paints nothing"), Pixels[261 * 688 + 568], Background);
    }
    TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(688, 344, Pixels, PNG);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Cursor");
    IFileManager::Get().MakeDirectory(*Directory, true);
    TestTrue(TEXT("Cursor preview saved"), FFileHelper::SaveArrayToFile(PNG, *(Directory / TEXT("CursorPreview.png"))));
    BeginCleanup(Renderer);
    return true;
}
#endif
