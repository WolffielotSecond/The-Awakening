#pragma once
#include "CoreMinimal.h"

enum class ETAInputCapability : uint8 { Gameplay, Look, Scan, Menu, Cursor, Puzzle };
/** Policy only: no Slate, actions, movement, widgets or input-mode side effects. */
class FTAInputRouter
{
public:
	using FHandle = uint64;
	FHandle Acquire(UObject* Owner, int32 Priority, TSet<ETAInputCapability> Allowed)
	{
		const FHandle Handle = ++Sequence;
		Requests.Add({Handle, Owner, Priority, MoveTemp(Allowed)});
		return Handle;
	}
	void Release(FHandle Handle) { Requests.RemoveAll([Handle](const FRequest& R) { return R.Handle == Handle; }); }
	bool Allows(ETAInputCapability Capability) const
	{
		const FRequest* Winner = nullptr;
		for (const auto& R : Requests)
			if (R.Owner.IsValid() && (!Winner || R.Priority > Winner->Priority || (R.Priority == Winner->Priority && R.Handle > Winner->Handle))) Winner = &R;
		return Winner ? Winner->Allowed.Contains(Capability) : (Capability == ETAInputCapability::Gameplay || Capability == ETAInputCapability::Look || Capability == ETAInputCapability::Scan);
	}
private:
	struct FRequest { FHandle Handle; TWeakObjectPtr<UObject> Owner; int32 Priority; TSet<ETAInputCapability> Allowed; };
	TArray<FRequest> Requests;
	FHandle Sequence = 0;
};
