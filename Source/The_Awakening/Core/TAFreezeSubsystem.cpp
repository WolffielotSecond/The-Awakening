#include "Core/TAFreezeSubsystem.h"
#include "Core/TAFreezeComponent.h"

void UTAFreezeSubsystem::RegisterParticipant(UTAFreezeComponent* Component)
{
	if (IsValid(Component))
	{
		Participants.Add(Component);
		Component->SetFreezeStrength(GetFreezeStrength());
	}
}

void UTAFreezeSubsystem::UnregisterParticipant(UTAFreezeComponent* Component)
{
	Participants.Remove(Component);
}

float UTAFreezeSubsystem::GetFreezeStrength() const
{
	float Strength = 0.f;
	for (const auto& Entry : Sources)
		if (Entry.Key.IsValid()) Strength = FMath::Max(Strength, Entry.Value);
	return Strength;
}

bool UTAFreezeSubsystem::HasFreezeRequestFor(const UTAFreezeComponent* Participant) const
{
	if (!IsValid(Participant)) return false;
	// Derive from the existing participant/source registry; no second lifecycle state.
	for (const auto& Entry : Participants)
	{
		if (Entry.Get() != Participant) continue;
		for (const auto& Source : Sources) if (Source.Key.IsValid()) return true;
		return false;
	}
	return false;
}

void UTAFreezeSubsystem::RequestFreeze(UObject* Source, float Strength)
{
	if (IsValid(Source))
	{
		Strength = FMath::IsFinite(Strength) ? FMath::Clamp(Strength, 0.f, 1.f) : 0.f;
		Sources.Add(Source, Strength);
		ApplyState();
	}
}

void UTAFreezeSubsystem::ReleaseFreeze(UObject* Source)
{
	Sources.Remove(Source);
	ApplyState();
}

void UTAFreezeSubsystem::ApplyState()
{
	for (auto It = Sources.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	// Callbacks may register/unregister participants; iterate a snapshot.
	const auto Snapshot = Participants.Array();
	for (const auto& Entry : Snapshot)
	{
		if (UTAFreezeComponent* Component = Entry.Get())
		{
			if (Participants.Contains(Entry)) Component->SetFreezeStrength(GetFreezeStrength());
		}
		else
		{
			Participants.Remove(Entry);
		}
	}
}
