#pragma once

#include "CoreMinimal.h"
#include "TACursorSettings.generated.h"

USTRUCT(BlueprintType)
struct FTACursorPulseSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HOVER PULSE SETTINGS", meta=(ClampMin="0", ClampMax="1"))
    float Opacity = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HOVER PULSE SETTINGS", meta=(ClampMin="0.05", Units="s"))
    float Interval = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HOVER PULSE SETTINGS", meta=(ClampMin="0.05", Units="s"))
    float Duration = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HOVER PULSE SETTINGS", meta=(ClampMin="0"))
    float ExpansionDistance = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HOVER PULSE SETTINGS", meta=(ClampMin="0.01"))
    float StrokeWidth = .6f;
};

USTRUCT(BlueprintType)
struct FTACursorStyleSettings
{
    GENERATED_BODY()

    // Radii describe the INNER edge of rings, preserving the gap at minimum radius.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cursor", meta=(ClampMin="0.5"))
    float UnitSize = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cursor")
    FLinearColor Color = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cursor", meta=(ClampMin="0.01"))
    float MainStrokeWidth = .7f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cursor", meta=(ClampMin="0.01", Units="s"))
    float ContractionDuration = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cursor", meta=(ClampMin="0.01", Units="s"))
    float ExpansionDuration = .16f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cursor", meta=(ClampMin="0.001", Units="s"))
    float ReversalDuration = .05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cursor", meta=(ClampMin="0.01", Units="s"))
    float HoverDuration = .12f;
};
