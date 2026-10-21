#include "Camera/TAAutoFocusCameraComponent.h"

void UTAAutoFocusCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	Super::GetCameraView(DeltaTime, DesiredView);

	if (!bAutoFocusEnabled || !IsValid(FocusPoint))
	{
		ResetAutoFocus();
		return;
	}

	// Super supplies the actual world camera pose, including spring-arm lag/collision and additive offsets.
	const FVector ToFocus = FocusPoint->GetComponentLocation() - DesiredView.Location;
	const double OpticalDistance = FVector::DotProduct(DesiredView.Rotation.Vector(), ToFocus);
	if (!FMath::IsFinite(OpticalDistance))
	{
		ResetAutoFocus();
		return;
	}

	// Zero disables DOF in Unreal. A point behind the camera cannot have a negative focus distance.
	TargetFocalDistance = static_cast<float>(FMath::Max(1.0, OpticalDistance));
	CurrentFocalDistance = bFocusInitialized
		? FMath::FInterpTo(CurrentFocalDistance, TargetFocalDistance, FMath::Max(0.f, DeltaTime), FMath::Max(0.f, FocusInterpSpeed))
		: TargetFocalDistance;
	bFocusInitialized = true;

	// Do not mutate authored camera settings: disabling autofocus restores manual settings automatically.
	// Keep the existing blend weight, aperture, focal length, and all other post-process fields intact.
	DesiredView.PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
	DesiredView.PostProcessSettings.DepthOfFieldFocalDistance = CurrentFocalDistance;
}

void UTAAutoFocusCameraComponent::ResetAutoFocus()
{
	bFocusInitialized = false;
}
