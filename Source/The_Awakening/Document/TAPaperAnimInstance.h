#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "TAPaperAnimInstance.generated.h"

class UAnimSequence;
class UAnimSequenceBase;

/** Explicitly sampled by ABP_Paper, so UI transitions and bone poses share the same clock. */
UCLASS(Blueprintable)
class THE_AWAKENING_API UTAPaperAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paper")
	TObjectPtr<UAnimSequence> FoldAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paper")
	TObjectPtr<UAnimSequence> FoldedAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paper")
	TObjectPtr<UAnimSequence> OpenedAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paper")
	bool bFoldStartsOpened = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Paper")
	TObjectPtr<UAnimSequenceBase> SelectedAnimation;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Paper")
	float SampleTime = 0.f;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Paper")
	float Openness = 0.f;

	UFUNCTION(BlueprintCallable, Category = "Paper")
	void SetOpenness(float Value);
	float GetFoldDuration() const;
	virtual void NativeInitializeAnimation() override;
};
