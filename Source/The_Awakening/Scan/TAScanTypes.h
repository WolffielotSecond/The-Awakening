#pragma once

#include "CoreMinimal.h"
#include "TAScanTypes.generated.h"

UENUM(BlueprintType)
enum class ETAScanTargetType : uint8
{
	Generic,
	Item,
	Terminal,
	Enemy
};

UENUM(BlueprintType)
enum class ETAScanEndReason : uint8
{
	Released,
	TimedOut,
	Canceled,
	UIInterrupted,
	OwnerInvalid,
	ComponentDestroyed
};

USTRUCT(BlueprintType)
struct FTAScanTargetInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
	ETAScanTargetType TargetType = ETAScanTargetType::Generic;
};
