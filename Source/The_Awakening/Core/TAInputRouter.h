#pragma once
#include "CoreMinimal.h"

class UWidget;
// Actions the player may submit, never names of modes or current gameplay state.
enum class ETAInputCapability : uint8
{
	Move, Look, Interact, Parkour, Scan, InventoryToggle, Cursor,
	Confirm, Undo, Pan, Navigate, Close, Advance, ToggleHistory, ToggleDrag, OpenDebugUI, ToggleFavorite
};
enum class ETAInputModeRequirement : uint8 { GameOnly, GameAndUI, UIOnly };
enum class ETAInputFocusRequirement : uint8 { Viewport, Target };

/** Declarative only: the controller applies these requirements, never the router. */
struct FTAInputPresentation
{
	ETAInputModeRequirement InputMode = ETAInputModeRequirement::GameOnly;
	bool bShowCursor = false;
	ETAInputFocusRequirement Focus = ETAInputFocusRequirement::Viewport;
	// Independent of owner lifetime. Missing target means viewport fallback, not another winner.
	TWeakObjectPtr<UWidget> FocusTarget;
};

struct FTAInputRequest
{
	TWeakObjectPtr<UObject> Owner;
	int32 Priority = 0;
	TSet<ETAInputCapability> Allowed;
	FTAInputPresentation Presentation;
};

/** Policy only. Game-thread use; no Slate calls, action execution or UI side effects. */
class FTAInputRouter
{
public:
	// Router-local handles are never reused. Zero denotes rejection/the base policy.
	using FHandle = uint64;
	struct FWinner
	{
		FHandle Handle = 0;
		FTAInputRequest Request;
		bool IsBase() const { return Handle == 0; }
	};
	FHandle Acquire(const FTAInputRequest& Request)
	{
		PruneInvalidOwners();
		if (!Request.Owner.IsValid()) return 0;
		const FHandle Handle = ++Sequence;
		Requests.Add({Handle, Request});
		return Handle;
	}
	// Exact-handle, idempotent release. No implicit pop or owner-name matching.
	void Release(FHandle Handle)
	{
		PruneInvalidOwners();
		Requests.RemoveAll([Handle](const FWinner& R) { return R.Handle == Handle; });
	}
	// Value snapshot: later mutations do not invalidate it. Owner expiry is pruned
	// on every query/mutation. Focus expiry never changes arbitration.
	FWinner GetWinner() const
	{
		PruneInvalidOwners();
		const FWinner* Winner = nullptr;
		for (const auto& R : Requests)
			if (!Winner || R.Request.Priority > Winner->Request.Priority ||
				(R.Request.Priority == Winner->Request.Priority && R.Handle > Winner->Handle)) Winner = &R;
		if (Winner) return *Winner;
		FWinner Base;
		Base.Request.Allowed = {ETAInputCapability::Move, ETAInputCapability::Look, ETAInputCapability::Interact,
			ETAInputCapability::Parkour, ETAInputCapability::Scan, ETAInputCapability::InventoryToggle, ETAInputCapability::OpenDebugUI};
		return Base;
	}
	bool Allows(ETAInputCapability Capability) const { return GetWinner().Request.Allowed.Contains(Capability); }
	bool AllowsFor(FHandle Handle, const UObject* Owner, ETAInputCapability Capability) const
	{
		if (!Handle || !IsValid(Owner)) return false;
		const auto Winner = GetWinner();
		return Winner.Handle == Handle && Winner.Request.Owner.Get() == Owner && Winner.Request.Allowed.Contains(Capability);
	}
private:
	void PruneInvalidOwners() const
	{
		Requests.RemoveAll([](const FWinner& R) { return !R.Request.Owner.IsValid(); });
	}
	mutable TArray<FWinner> Requests;
	FHandle Sequence = 0;
};
