#pragma once

#include "CoreMinimal.h"
#include "TAPromptTypes.generated.h"

class UInputAction;
class UTAInteractionPromptAnchor;
class AActor;

UENUM(BlueprintType)
enum class ETAPromptType : uint8
{
	Interact     UMETA(DisplayName = "交互"),
	ParkourJump  UMETA(DisplayName = "跨越"),
	ParkourDrop  UMETA(DisplayName = "跳下")
};

/** 某一类型本帧选中的提示目标。 */
USTRUCT(BlueprintType)
struct FTAPromptCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ETAPromptType Type = ETAPromptType::Interact;

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Source;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UInputAction> Action = nullptr;
	UPROPERTY(BlueprintReadOnly)
	FString TextOverride;

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UTAInteractionPromptAnchor> Anchor;
};
