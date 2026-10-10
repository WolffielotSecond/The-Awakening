#pragma once

#include "UI/Cursor/TACursorSettings.h"

/** Pure animation model. Absolute UI time is independent of movement and world pause. */
class FTACursorAnimation
{
public:
    enum class EPhase { Idle, Contracting, Holding, Expanding, BrakingOutward };
    struct FPulse
    {
        double BirthTime;
        float StartRadius;
        FTACursorPulseSettings Settings;
        float RadiusAt(double Now) const;
        float OpacityAt(double Now) const;
    };

    void Update(double Now, bool bVisible, bool bHover, const FTACursorStyleSettings& Style, const FTACursorPulseSettings& Pulse);
    void AdvanceTo(double Now);
    void Press(double Now);
    void Release(double Now);
    void Hide();
    float GetRadius() const { return Radius; }
    float GetVelocity() const { return Velocity; }
    float GetDotRadius() const { return DotRadius; }
    bool IsVisible() const { return bVisible; }
    EPhase GetPhase() const { return Phase; }
    const TArray<FPulse>& GetPulses() const { return Pulses; }
    double GetTime() const { return Time; }

private:
    void EvaluateRing(double Now);
    void BeginPhase(EPhase InPhase, double Now);
    static float Ease(float U) { return U * U * (3.f - 2.f * U); }
    FTACursorStyleSettings StyleSettings;
    FTACursorPulseSettings PulseSettings;
    TArray<FPulse> Pulses;
    EPhase Phase = EPhase::Idle;
    bool bVisible = false, bHover = false, bPressed = false;
    double Time = 0, PhaseStart = 0, NextPulse = 0, DotStart = 0;
    float Radius = 5.f, Velocity = 0, DotRadius = 1.f;
    float StartRadius = 5.f, StartVelocity = 0, PhaseDuration = .1f;
    float DotFrom = 1.f, DotTarget = 1.f, DotDuration = .12f;
};
