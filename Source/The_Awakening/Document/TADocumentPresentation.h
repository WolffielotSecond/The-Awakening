#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Document/TAPaperAnimInstance.h"
#include "TADocumentPresentation.generated.h"

/** Shared model-axis correction and paper animation configuration. Per-document transforms remain editable. */
UCLASS(BlueprintType)
class THE_AWAKENING_API UTADocumentPresentation : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	FRotator PreviewRotation = FRotator::ZeroRotator;
	/** 书中折叠姿态的画面旋转：正值为逆时针，不影响展开后的阅读朝向。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	float FoldedDiaryRotation = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document")
	TSubclassOf<UTAPaperAnimInstance> AnimationClass;
	/** Motion timings are real UI time, independent of gameplay freeze. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document", meta = (ClampMin = "0.01"))
	float TravelDuration = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document", meta = (ClampMin = "0.01"))
	float OrientDuration = 0.25f;
	/** Reserved framing for the fully opened sheet, avoiding animated bounds zoom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Document", meta = (ClampMin = "1"))
	float FramingScale = 1.15f;
};
