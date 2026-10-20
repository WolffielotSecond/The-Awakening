#include "Document/TAPaperAnimInstance.h"
#include "Animation/AnimSequence.h"

void UTAPaperAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	SetOpenness(Openness);
}

void UTAPaperAnimInstance::SetOpenness(float Value)
{
	Openness = FMath::Clamp(Value, 0.f, 1.f);
	if (Openness <= 0.f && FoldedAnimation)
	{
		SelectedAnimation = FoldedAnimation;
		SampleTime = 0.f;
	}
	else if (Openness >= 1.f && OpenedAnimation)
	{
		SelectedAnimation = OpenedAnimation;
		SampleTime = 0.f;
	}
	else
	{
		SelectedAnimation = FoldAnimation;
		SampleTime = (bFoldStartsOpened ? 1.f - Openness : Openness) * GetFoldDuration();
	}
}

float UTAPaperAnimInstance::GetFoldDuration() const
{
	return FoldAnimation ? FoldAnimation->GetPlayLength() : 0.f;
}
