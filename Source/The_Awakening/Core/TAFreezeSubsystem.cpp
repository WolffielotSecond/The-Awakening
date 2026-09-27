#include "Core/TAFreezeSubsystem.h"
#include "Core/TAFreezeComponent.h"

void UTAFreezeSubsystem::RegisterParticipant(UTAFreezeComponent* Component)
{
	if (IsValid(Component))
	{
		Participants.Add(Component);
		Component->SetFrozen(IsFrozen());
	}
}

void UTAFreezeSubsystem::UnregisterParticipant(UTAFreezeComponent* Component)
{
	Participants.Remove(Component);
}

void UTAFreezeSubsystem::RequestFreeze(UObject* Source)
{
	if (IsValid(Source))
	{
		Sources.Add(Source);
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
		if (!It->IsValid()) It.RemoveCurrent();
	}
	// Callbacks may register/unregister participants; iterate a snapshot.
	const auto Snapshot = Participants.Array();
	for (const auto& Entry : Snapshot)
	{
		if (UTAFreezeComponent* Component = Entry.Get())
		{
			if (Participants.Contains(Entry)) Component->SetFrozen(IsFrozen());
		}
		else
		{
			Participants.Remove(Entry);
		}
	}
}
