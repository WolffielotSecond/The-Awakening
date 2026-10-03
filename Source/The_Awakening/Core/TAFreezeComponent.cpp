#include "Core/TAFreezeComponent.h"
#include "Core/TAFreezeSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTAFreezeComponent::UTAFreezeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTAFreezeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UTAFreezeSubsystem* Registry = GetWorld()->GetSubsystem<UTAFreezeSubsystem>())
	{
		Registry->RegisterParticipant(this);
	}
}

void UTAFreezeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTAFreezeSubsystem* Registry = World->GetSubsystem<UTAFreezeSubsystem>())
		{
			Registry->UnregisterParticipant(this);
		}
	}
	// Also restore ticks when this component alone is removed from a living actor.
	SetFreezeStrength(0.f);
	Super::EndPlay(EndPlayReason);
}

bool UTAFreezeComponent::HasFreezeRequest() const
{
	const UWorld* World = GetWorld();
	const UTAFreezeSubsystem* Registry = World ? World->GetSubsystem<UTAFreezeSubsystem>() : nullptr;
	return Registry && Registry->HasFreezeRequestFor(this);
}

bool UTAFreezeComponent::IsActorFrozen(const AActor* Actor)
{
	const UTAFreezeComponent* Component = Actor ? Actor->FindComponentByClass<UTAFreezeComponent>() : nullptr;
	return Component && Component->IsFrozen();
}

void UTAFreezeComponent::SetFreezeStrength(float Strength)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	Strength = FMath::Clamp(Strength, 0.f, 1.f);
	if (FreezeStrength == 0.f && Strength > 0.f) SavedTimeDilation = Owner->CustomTimeDilation;
	if (FreezeStrength > 0.f || Strength > 0.f)
		Owner->CustomTimeDilation = SavedTimeDilation * (1.f - Strength);
	FreezeStrength = Strength;
	// Only suspend ticks at the endpoint; the transition uses actor/component delta scaling.
	SetFrozen(Strength >= 1.f);
}

void UTAFreezeComponent::SetFrozen(bool bInFrozen)
{
	AActor* Owner = GetOwner();
	if (bFrozen == bInFrozen || !Owner) return;
	bFrozen = bInFrozen;
	if (bFrozen)
	{
		bDidFreezeActorTick = bFreezeActorTick;
		bSavedActorTickEnabled = Owner->IsActorTickEnabled();
		if (bDidFreezeActorTick) Owner->SetActorTickEnabled(false);

		TInlineComponentArray<UActorComponent*> Components;
		Owner->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (!Component || Component == this || Component->ComponentHasTag(ExemptComponentTag)) continue;
			SavedComponentTicks.Add(Component, Component->IsComponentTickEnabled());
			Component->SetComponentTickEnabled(false);
		}
	}
	else
	{
		for (const auto& Entry : SavedComponentTicks)
		{
			if (UActorComponent* Component = Entry.Key.Get()) Component->SetComponentTickEnabled(Entry.Value);
		}
		SavedComponentTicks.Reset();
		if (bDidFreezeActorTick) Owner->SetActorTickEnabled(bSavedActorTickEnabled);
		bDidFreezeActorTick = false;
	}

	// TODO(enemy AI): AIController/BrainComponent/StateTree can tick outside this actor.
	// Pause and restore their previous state here or via OnFreezeChanged when AI exists.
	// World timers, latent actions, GAS durations and external damage/interaction calls
	// are NOT stopped by tick suspension: gate them with IsActorFrozen or pause explicitly.
	// Physics simulation and components added mid-freeze need dedicated adapters as well.
	// Do not change world dilation: scan Niagara, material Time and scan timers must run.
	OnFreezeChanged.Broadcast(bFrozen);
}
