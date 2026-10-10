#include "UI/Cursor/TACursorAnimation.h"

float FTACursorAnimation::FPulse::RadiusAt(double Now) const
{
    const float U = FMath::Clamp(float((Now - BirthTime) / Settings.Duration), 0.f, 1.f);
    return StartRadius + Settings.ExpansionDistance * Ease(U);
}

float FTACursorAnimation::FPulse::OpacityAt(double Now) const
{
    const float U = FMath::Clamp(float((Now - BirthTime) / Settings.Duration), 0.f, 1.f);
    return Settings.Opacity * (1.f - Ease(U));
}

void FTACursorAnimation::Hide()
{
    bVisible = bHover = bPressed = false;
    Phase = EPhase::Idle;
    Radius = 5.f;
    Velocity = 0;
    DotRadius = DotFrom = DotTarget = 1.f;
    Pulses.Reset();
}

void FTACursorAnimation::Update(double Now, bool InVisible, bool InHover,
    const FTACursorStyleSettings& Style, const FTACursorPulseSettings& Pulse)
{
    if (!InVisible) { Hide(); Time = Now; return; }
    if (!bVisible) { Time = Now; bVisible = true; }
    AdvanceTo(Now);
    StyleSettings = Style;
    FTACursorPulseSettings Safe = Pulse;
    Safe.Interval = FMath::Max(.05f, Safe.Interval);
    Safe.Duration = FMath::Max(.05f, Safe.Duration);
    Safe.Opacity = FMath::Clamp(Safe.Opacity, 0.f, 1.f);
    Safe.ExpansionDistance = FMath::Max(0.f, Safe.ExpansionDistance);
    Safe.StrokeWidth = FMath::Max(.01f, Safe.StrokeWidth);
    if (!FMath::IsNearlyEqual(Safe.Interval, PulseSettings.Interval) && bHover)
    {
        // Preserve the last generation/hover-entry anchor when tuning the interval.
        NextPulse = FMath::Max(Now, NextPulse - PulseSettings.Interval + Safe.Interval);
    }
    PulseSettings = Safe;
    if (bHover != InHover)
    {
        bHover = InHover;
        DotFrom = DotRadius;
        DotTarget = bHover ? 3.f : 1.f;
        DotStart = Now;
        DotDuration = FMath::Max(.01f, StyleSettings.HoverDuration);
        if (bHover) NextPulse = Now + PulseSettings.Interval;
    }
}

void FTACursorAnimation::BeginPhase(EPhase InPhase, double Now)
{
    Phase = InPhase;
    PhaseStart = Now;
    StartRadius = Radius;
    StartVelocity = Velocity;
    if (Phase == EPhase::Contracting) PhaseDuration = FMath::Max(.01f, StyleSettings.ContractionDuration);
    else if (Phase == EPhase::Expanding) PhaseDuration = FMath::Max(.01f, StyleSettings.ExpansionDuration);
    else if (Phase == EPhase::BrakingOutward)
    {
        // Analytical displacement v*T/3 guarantees no outward overshoot; no hard clamp.
        PhaseDuration = FMath::Min(FMath::Max(.001f, StyleSettings.ReversalDuration),
            3.f * FMath::Max(0.f, 5.f - Radius) / StartVelocity);
    }
    else Velocity = 0;
}

void FTACursorAnimation::EvaluateRing(double Now)
{
    // Carry elapsed time across transitions, including frames spanning an entire short click.
    for (int32 Transition = 0; Transition < 4; ++Transition)
    {
        if (Phase == EPhase::Idle || Phase == EPhase::Holding) return;
        const float U = PhaseDuration > 0 ? FMath::Clamp(float((Now - PhaseStart) / PhaseDuration), 0.f, 1.f) : 1.f;
        if (Phase == EPhase::BrakingOutward)
        {
            Radius = StartRadius + StartVelocity * PhaseDuration * (U - U*U + U*U*U/3.f);
            Velocity = StartVelocity * FMath::Square(1.f - U);
        }
        else
        {
            const float Target = Phase == EPhase::Contracting ? 3.4f : 5.f;
            Radius = FMath::Lerp(StartRadius, Target, Ease(U));
            Velocity = (Target - StartRadius) * 6.f * U * (1.f - U) / PhaseDuration;
        }
        if (U < 1.f) return;
        const double End = PhaseStart + PhaseDuration;
        if (Phase == EPhase::BrakingOutward) BeginPhase(EPhase::Contracting, End);
        else if (Phase == EPhase::Contracting) BeginPhase(bPressed ? EPhase::Holding : EPhase::Expanding, End);
        else BeginPhase(EPhase::Idle, End);
    }
}

void FTACursorAnimation::AdvanceTo(double Now)
{
    if (!bVisible || Now < Time) return;
    if (bHover && NextPulse <= Now)
    {
        // After a stall, emit only the latest due pulse; retain the fixed timer cadence.
        const double Count = FMath::FloorToDouble((Now - NextPulse) / PulseSettings.Interval);
        const double LastDue = NextPulse + Count * PulseSettings.Interval;
        // Capture the actual current visual radius at creation, not a backdated
        // scheduled radius. The scheduling anchor itself remains independent.
        EvaluateRing(Now);
        Pulses.Add({Now, Radius, PulseSettings});
        NextPulse = LastDue + PulseSettings.Interval;
    }
    EvaluateRing(Now);
    DotRadius = FMath::Lerp(DotFrom, DotTarget, Ease(FMath::Clamp(float((Now - DotStart) / DotDuration), 0.f, 1.f)));
    Pulses.RemoveAll([Now](const FPulse& P) { return Now - P.BirthTime >= P.Settings.Duration; });
    Time = Now;
}

void FTACursorAnimation::Press(double Now)
{
    if (!bVisible) return;
    AdvanceTo(Now);
    bPressed = true;
    if (Phase == EPhase::Idle) BeginPhase(EPhase::Contracting, Now);
    else if (Phase == EPhase::Expanding)
        BeginPhase(Velocity > 0.f ? EPhase::BrakingOutward : EPhase::Contracting, Now);
}

void FTACursorAnimation::Release(double Now)
{
    if (!bVisible) return;
    AdvanceTo(Now);
    bPressed = false;
    if (Phase == EPhase::Holding) BeginPhase(EPhase::Expanding, Now);
}
