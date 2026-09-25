#include "Scan/TAScannableComponent.h"

#include "Components/PrimitiveComponent.h"

UTAScannableComponent::UTAScannableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTAScannableComponent::SetScanHighlightEnabled(bool bEnabled)
{
	TArray<UPrimitiveComponent*> Components;
	for (UPrimitiveComponent* Component : HighlightComponents)
	{
		if (IsValid(Component))
		{
			Components.Add(Component);
		}
	}

	if (Components.IsEmpty() && GetOwner())
	{
		GetOwner()->GetComponents<UPrimitiveComponent>(Components);
	}

	if (bEnabled)
	{
		int32 ResolvedStencil = CustomDepthStencilValue;
		if (bUseTargetTypeStencil)
		{
			switch (ScanInfo.TargetType)
			{
			case ETAScanTargetType::Terminal: ResolvedStencil = 3; break;
			case ETAScanTargetType::Enemy: ResolvedStencil = 4; break;
			case ETAScanTargetType::Item:
			case ETAScanTargetType::Generic:
			default: ResolvedStencil = 2; break;
			}
		}
		for (UPrimitiveComponent* Component : Components)
		{
			if (!OriginalRenderStates.Contains(Component))
			{
				FOriginalRenderState State;
				State.bRenderCustomDepth = Component->bRenderCustomDepth;
				State.StencilValue = Component->CustomDepthStencilValue;
				OriginalRenderStates.Add(Component, State);
			}
			Component->SetRenderCustomDepth(true);
			Component->SetCustomDepthStencilValue(FMath::Clamp(ResolvedStencil, 0, 255));
		}
		return;
	}

	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FOriginalRenderState>& Pair : OriginalRenderStates)
	{
		if (UPrimitiveComponent* Component = Pair.Key.Get())
		{
			Component->SetRenderCustomDepth(Pair.Value.bRenderCustomDepth);
			Component->SetCustomDepthStencilValue(Pair.Value.StencilValue);
		}
	}
	OriginalRenderStates.Reset();
}

void UTAScannableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetScanHighlightEnabled(false);
	Super::EndPlay(EndPlayReason);
}
