#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/Cursor/TACursorAnimation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACursorClickTest, "TheAwakening.Cursor.Click",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACursorClickTest::RunTest(const FString&)
{
    FTACursorStyleSettings Style;
    FTACursorPulseSettings Pulse;
    FTACursorAnimation A;
    A.Update(0, true, false, Style, Pulse);
    A.Press(0);
    A.Release(.001);
    A.AdvanceTo(.05);
    TestTrue(TEXT("Early release does not cancel contraction"), A.GetRadius() < 5.f && A.GetVelocity() < 0.f);
    A.AdvanceTo(.1);
    TestEqual(TEXT("Short click reaches minimum"), A.GetRadius(), 3.4f, .0001f);
    A.AdvanceTo(.27);
    TestEqual(TEXT("Short click finishes"), A.GetRadius(), 5.f, .0001f);
    A.Press(.3);
    A.AdvanceTo(1);
    TestEqual(TEXT("Long press holds minimum"), A.GetRadius(), 3.4f, .0001f);
    A.Release(1);
    A.AdvanceTo(1.2);
    TestEqual(TEXT("Long press expands on release"), A.GetRadius(), 5.f, .0001f);

    FTACursorAnimation Hover, Empty;
    Hover.Update(0, true, true, Style, Pulse);
    Empty.Update(0, true, false, Style, Pulse);
    Hover.Press(.2); Empty.Press(.2);
    Hover.Release(.201); Empty.Release(.201);
    for (int32 I = 0; I <= 30; ++I)
    {
        const double Now = .201 + I * .01;
        Hover.AdvanceTo(Now); Empty.AdvanceTo(Now);
        TestEqual(TEXT("Hover does not change ring animation"), Hover.GetRadius(), Empty.GetRadius(), .0001f);
    }
    TestEqual(TEXT("Dot remains enlarged while clicking"), Hover.GetDotRadius(), 3.f, .0001f);
    FTACursorAnimation Coarse;
    Coarse.Update(0, true, false, Style, Pulse);
    Coarse.Press(0); Coarse.Release(.001); Coarse.AdvanceTo(1);
    TestEqual(TEXT("Entire click can finish across a stalled frame"), Coarse.GetRadius(), 5.f, .0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACursorReversalTest, "TheAwakening.Cursor.Reversal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACursorReversalTest::RunTest(const FString&)
{
    const FTACursorStyleSettings Style;
    const FTACursorPulseSettings Pulse;
    for (const double Offset : {.001, .02, .08, .14, .159})
    {
        FTACursorAnimation A;
        A.Update(0, true, false, Style, Pulse);
        A.Press(0); A.Release(.001); A.AdvanceTo(.1 + Offset);
        const float Radius = A.GetRadius(), Velocity = A.GetVelocity();
        A.Press(.1 + Offset);
        TestEqual(TEXT("Repress preserves radius"), A.GetRadius(), Radius, .00001f);
        TestEqual(TEXT("Repress preserves outward velocity"), A.GetVelocity(), Velocity, .00001f);
        A.Release(.101 + Offset);
        bool bTurnedInward = false;
        for (int32 I = 1; I <= 400; ++I)
        {
            A.AdvanceTo(.101 + Offset + I * .001);
            TestTrue(TEXT("Reversal stays inside radius bounds"), A.GetRadius() <= 5.00001f && A.GetRadius() >= 3.39999f);
            bTurnedInward |= A.GetVelocity() < 0;
        }
        TestTrue(TEXT("Repress completes inward turn despite early release"), bTurnedInward);
        TestEqual(TEXT("Repress eventually finishes"), A.GetRadius(), 5.f, .0001f);
    }
    FTACursorAnimation A;
    A.Update(0, true, false, Style, Pulse);
    A.Press(0); A.Release(.001); A.AdvanceTo(.04);
    const float Before = A.GetRadius();
    A.Press(.04); A.Release(.041); A.AdvanceTo(.1);
    TestEqual(TEXT("Repress during contraction does not restart it"), A.GetRadius(), 3.4f, .0001f);
    TestTrue(TEXT("Contracting repress starts at current radius"), Before > 3.4f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTACursorPulseTest, "TheAwakening.Cursor.Pulse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTACursorPulseTest::RunTest(const FString&)
{
    FTACursorStyleSettings Style;
    FTACursorPulseSettings Pulse;
    Pulse.Interval = .2f; Pulse.Duration = .8f;
    FTACursorAnimation A;
    A.Update(0, true, true, Style, Pulse);
    A.Press(.15); A.Release(.151); A.AdvanceTo(.201);
    TestEqual(TEXT("Click does not delay pulse"), A.GetPulses().Num(), 1);
    if (A.GetPulses().Num())
    {
        const auto P = A.GetPulses()[0];
        TestEqual(TEXT("Pulse captures the actual generation frame radius"), P.StartRadius, A.GetRadius(), .0001f);
        TestEqual(TEXT("Late frame does not use a backdated scheduled radius"), P.StartRadius, 4.176003f, .0001f);
        TestEqual(TEXT("Expansion is relative to captured radius"), P.RadiusAt(1.1), P.StartRadius + 2.f, .0001f);
        TestEqual(TEXT("Pulse opacity starts at configured value"), P.OpacityAt(P.BirthTime), .35f, .0001f);
        TestEqual(TEXT("Pulse fades fully"), P.OpacityAt(1.1), 0.f, .0001f);
        Pulse.Duration = 2.f; Pulse.Opacity = .8f; Pulse.ExpansionDistance = 4.f; Pulse.StrokeWidth = 1.f;
        A.Update(.22, true, true, Style, Pulse);
        TestEqual(TEXT("Existing pulse keeps duration snapshot"), A.GetPulses()[0].Settings.Duration, .8f, .0001f);
        TestEqual(TEXT("Existing pulse keeps opacity snapshot"), A.GetPulses()[0].Settings.Opacity, .35f, .0001f);
    }
    A.AdvanceTo(.401);
    TestEqual(TEXT("Overlapping pulses supported"), A.GetPulses().Num(), 2);
    if (A.GetPulses().Num() >= 2)
        TestTrue(TEXT("Pulse during expansion captures actual radius"), A.GetPulses()[1].StartRadius > 3.4f && A.GetPulses()[1].StartRadius < 5.f);
    A.Update(.45, true, true, Style, Pulse); // Switching between actionable targets has the same bool.
    A.AdvanceTo(.601);
    TestEqual(TEXT("Actionable target switch preserves cadence"), A.GetPulses().Num(), 3);
    A.Update(.65, true, false, Style, Pulse);
    A.AdvanceTo(.9);
    TestEqual(TEXT("Leaving hover stops new pulses but retains living ones"), A.GetPulses().Num(), 3);
    A.Update(.91, false, false, Style, Pulse);
    TestFalse(TEXT("Hidden cursor is invisible"), A.IsVisible());
    TestEqual(TEXT("Hiding clears every pulse"), A.GetPulses().Num(), 0);
    A.Press(.92);
    A.Update(.93, true, false, Style, Pulse);
    TestEqual(TEXT("Showing starts clean after hidden press"), A.GetRadius(), 5.f, .0001f);
    TestEqual(TEXT("Showing restores default dot"), A.GetDotRadius(), 1.f, .0001f);
    A.Update(1, true, true, Style, Pulse);
    A.AdvanceTo(100);
    TestTrue(TEXT("Stalled frame does not flood pulse allocations"), A.GetPulses().Num() <= 1);
    return true;
}
#endif
