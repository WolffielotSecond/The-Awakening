#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TAInputRouter.h"
#include "InputAction.h"
#include "Components/Button.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInputRouterTest, "TheAwakening.Input.Router",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInputRouterTest::RunTest(const FString&)
{
	FTAInputRouter Router;
	UObject* Owner = NewObject<UInputAction>();
	auto Acquire = [&](UObject* InOwner, int32 Priority, TSet<ETAInputCapability> Allowed)
	{
		FTAInputRequest Request;
		Request.Owner = InOwner; Request.Priority = Priority; Request.Allowed = MoveTemp(Allowed);
		return Router.Acquire(Request);
	};
	TestTrue(TEXT("Base is explicit and allows gameplay"), Router.GetWinner().IsBase() && Router.Allows(ETAInputCapability::Move));
	TestFalse(TEXT("Base does not allow UI confirm"), Router.Allows(ETAInputCapability::Confirm));
	const auto Scan = Acquire(Owner, 100, {ETAInputCapability::Scan, ETAInputCapability::Look, ETAInputCapability::Cursor});
	TestFalse(TEXT("Scan blocks gameplay"), Router.Allows(ETAInputCapability::Move));
	TestTrue(TEXT("Scan allows camera and cursor"), Router.Allows(ETAInputCapability::Look) && Router.Allows(ETAInputCapability::Cursor));
	const auto Menu = Acquire(Owner, 200, {ETAInputCapability::Navigate});
	const auto Modal = Acquire(Owner, 300, {ETAInputCapability::Confirm, ETAInputCapability::Pan});
	Router.Release(Menu);
	TestTrue(TEXT("Out of order release preserves winner"), Router.GetWinner().Handle == Modal);
	TestFalse(TEXT("Missing capability never falls through"), Router.Allows(ETAInputCapability::Look));
	TestFalse(TEXT("Confirm does not imply undo"), Router.Allows(ETAInputCapability::Undo));
	const auto Peer = Acquire(Owner, 300, {ETAInputCapability::Navigate});
	TestTrue(TEXT("Same owner can acquire distinct requests; latest peer wins"), Peer != Modal && Router.GetWinner().Handle == Peer);
	const auto Snapshot = Router.GetWinner();
	Router.Release(Peer); Router.Release(Peer); Router.Release(0);
	TestTrue(TEXT("Duplicate/zero release cannot pop another request"), Router.GetWinner().Handle == Modal);
	TestTrue(TEXT("Winner query returns a stable value snapshot"), Snapshot.Handle == Peer);
	Router.Release(Modal);
	TestTrue(TEXT("Lower request restored"), Router.GetWinner().Handle == Scan);
	Router.Release(Scan);
	TestTrue(TEXT("Last release restores base"), Router.GetWinner().IsBase());
	TestTrue(TEXT("Null owner is rejected"), Acquire(nullptr, 999, {}) == 0);
	TestTrue(TEXT("Rejected request leaves base intact"), Router.GetWinner().IsBase());

	UObject* TransientOwner = NewObject<UInputAction>();
	UButton* Focus = NewObject<UButton>();
	FTAInputRequest Request;
	Request.Owner = TransientOwner; Request.Priority = 400;
	Request.Allowed = {ETAInputCapability::Confirm};
	Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
	Request.Presentation.bShowCursor = true;
	Request.Presentation.Focus = ETAInputFocusRequirement::Target;
	Request.Presentation.FocusTarget = Focus;
	const auto Lower = Acquire(Owner, 100, {ETAInputCapability::Look});
	const auto Focused = Router.Acquire(Request);
	Request.Allowed.Reset(); // Caller mutations must not edit registered policy.
	TestTrue(TEXT("Registered request is copied"), Router.Allows(ETAInputCapability::Confirm));
	TestTrue(TEXT("Presentation requirements preserved"), Router.GetWinner().Request.Presentation.bShowCursor &&
		Router.GetWinner().Request.Presentation.InputMode == ETAInputModeRequirement::GameAndUI);
	Focus->MarkAsGarbage();
	const auto WithoutFocus = Router.GetWinner();
	TestTrue(TEXT("Dead focus does not change owner/winner"), WithoutFocus.Handle == Focused && WithoutFocus.Request.Owner.Get() == TransientOwner);
	TestFalse(TEXT("Dead focus is exposed for controller fallback"), WithoutFocus.Request.Presentation.FocusTarget.IsValid());
	TestTrue(TEXT("Focus expiry preserves permission"), Router.Allows(ETAInputCapability::Confirm));
	TransientOwner->MarkAsGarbage();
	TestTrue(TEXT("Owner expiry restores live lower request"), Router.GetWinner().Handle == Lower);
	Router.Release(Focused);
	TestTrue(TEXT("Release of expired request cannot release successor"), Router.GetWinner().Handle == Lower);
	TestTrue(TEXT("Garbage owner rejected on acquire"), Acquire(TransientOwner, 999, {}) == 0);
	Router.Release(Lower);
	TestTrue(TEXT("All requests gone returns base presentation"), Router.GetWinner().IsBase() &&
		!Router.GetWinner().Request.Presentation.bShowCursor &&
		Router.GetWinner().Request.Presentation.Focus == ETAInputFocusRequirement::Viewport);
	const auto DenyAll = Acquire(Owner, 1, {});
	TestFalse(TEXT("Live empty request denies rather than falling back"), Router.Allows(ETAInputCapability::Move));
	Router.Release(DenyAll);
	const auto First = Acquire(Owner, 200, {ETAInputCapability::Confirm});
	auto* OtherOwner = NewObject<UInputAction>();
	const auto Second = Acquire(OtherOwner, 300, {ETAInputCapability::Confirm});
	TestFalse(TEXT("Covered owner cannot borrow winner confirm"), Router.AllowsFor(First, Owner, ETAInputCapability::Confirm));
	TestFalse(TEXT("Winner handle cannot be borrowed by another owner"), Router.AllowsFor(Second, Owner, ETAInputCapability::Confirm));
	TestFalse(TEXT("Zero handle never authorizes an owner"), Router.AllowsFor(0, OtherOwner, ETAInputCapability::Confirm));
	TestTrue(TEXT("Exact owner and winner handle authorize"), Router.AllowsFor(Second, OtherOwner, ETAInputCapability::Confirm));
	TestFalse(TEXT("Owner identity does not bypass capability"), Router.AllowsFor(Second, OtherOwner, ETAInputCapability::Undo));
	Router.Release(Second);
	TestTrue(TEXT("Remaining owner regains its own permission"), Router.AllowsFor(First, Owner, ETAInputCapability::Confirm));
	Router.Release(First);
	return true;
}
#endif
