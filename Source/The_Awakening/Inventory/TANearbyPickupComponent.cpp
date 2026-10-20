#include "Inventory/TANearbyPickupComponent.h"
#include "Inventory/TAWorldItem.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TAFreezeComponent.h"
#include "Core/TALocalizeSubsystem.h"
#include "The_AwakeningPlayerController.h"
#include "UI/Pickup/TAPickupPanelWidget.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UTANearbyPickupComponent::UTANearbyPickupComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTANearbyPickupComponent::Configure(float InRadius, UInputAction* InInteract, UInputAction* InPrevious, UInputAction* InNext)
{
    PickupRadius = InRadius;
    InteractAction = InInteract;
    PreviousAction = InPrevious;
    NextAction = InNext;
}

bool UTANearbyPickupComponent::IsPickupAvailable() const
{
    const auto* Pawn = Cast<APawn>(GetOwner());
    const auto* PC = Pawn ? Cast<AThe_AwakeningPlayerController>(Pawn->GetController()) : nullptr;
    return PC && PC->IsLocalController() && PC->AllowsInput(ETAInputCapability::Interact)
        && !UTAFreezeComponent::IsActorFrozen(GetOwner());
}

bool UTANearbyPickupComponent::IsEligible(ATAWorldItem* Item) const
{
    if (!IsValid(Item) || Item->IsActorBeingDestroyed() || !GetOwner() ||
        FVector::DistSquared(GetOwner()->GetActorLocation(), Item->GetActorLocation()) > FMath::Square(PickupRadius) ||
        !ITAInteractable::Execute_CanInteract(Item, GetOwner())) return false;
    if (bRequireLineOfSight)
    {
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(NearbyPickupSight), false, GetOwner());
        if (GetWorld()->LineTraceSingleByChannel(Hit, GetOwner()->GetActorLocation(), Item->GetActorLocation(), ECC_Visibility, Params)
            && Hit.GetActor() != Item) return false;
    }
    return true;
}

void UTANearbyPickupComponent::RefreshNearbyItems()
{
    if (!GetWorld() || !GetOwner()) return;
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(NearbyPickup), false, GetOwner());
    // Inspect results even when the query returns false (overlap-only responses).
    GetWorld()->OverlapMultiByChannel(Overlaps, GetOwner()->GetActorLocation(), FQuat::Identity,
        ECC_Visibility, FCollisionShape::MakeSphere(PickupRadius), Params);
    TArray<ATAWorldItem*> Candidates;
    for (const auto& Overlap : Overlaps)
        if (auto* Item = Cast<ATAWorldItem>(Overlap.GetActor()); IsEligible(Item)) Candidates.AddUnique(Item);
    Candidates.Sort([this](const ATAWorldItem& A, const ATAWorldItem& B)
    {
        const double DA = FVector::DistSquared(GetOwner()->GetActorLocation(), A.GetActorLocation());
        const double DB = FVector::DistSquared(GetOwner()->GetActorLocation(), B.GetActorLocation());
        return DA == DB ? A.GetUniqueID() < B.GetUniqueID() : DA < DB;
    });

    const auto OldItems = Items;
    const auto OldSelection = SelectedItem;
    const int32 OldIndex = GetSelectedIndex();
    Items.RemoveAll([&Candidates](const auto& Item) { return !Candidates.Contains(Item.Get()); });
    for (auto* Candidate : Candidates) Items.AddUnique(Candidate);
    if (!Items.Contains(SelectedItem) || !SelectedItem.IsValid())
    {
        SelectedItem.Reset();
        // Prefer the next surviving old row, then the previous one, before new arrivals.
        for (int32 Index = OldIndex + 1; Index < OldItems.Num(); ++Index)
            if (Items.Contains(OldItems[Index])) { SelectedItem = OldItems[Index]; break; }
        if (!SelectedItem.IsValid())
            for (int32 Index = OldIndex - 1; Index >= 0; --Index)
                if (Items.Contains(OldItems[Index])) { SelectedItem = OldItems[Index]; break; }
        if (!SelectedItem.IsValid() && !Items.IsEmpty()) SelectedItem = Items[0];
    }

    TArray<int32> Counts;
    TArray<TWeakObjectPtr<UTAItemDefinition>> Definitions;
    for (const auto& Item : Items)
    {
        Counts.Add(Item->GetCount());
        Definitions.Add(Item->GetItemDef());
    }
    const bool bChanged = OldItems != Items || OldSelection != SelectedItem || Counts != ObservedCounts || Definitions != ObservedDefinitions;
    ObservedCounts = MoveTemp(Counts);
    ObservedDefinitions = MoveTemp(Definitions);
    UpdatePanel();
    if (bChanged) OnNearbyItemsChanged.Broadcast();
}

TArray<ATAWorldItem*> UTANearbyPickupComponent::GetNearbyItems() const
{
    TArray<ATAWorldItem*> Result;
    for (const auto& Item : Items) if (Item.IsValid()) Result.Add(Item.Get());
    return Result;
}

ATAWorldItem* UTANearbyPickupComponent::GetSelectedItem() const { return SelectedItem.Get(); }
bool UTANearbyPickupComponent::HasPanel() const { return IsValid(Panel.Get()); }
int32 UTANearbyPickupComponent::GetSelectedIndex() const { return Items.IndexOfByKey(SelectedItem); }
void UTANearbyPickupComponent::SelectPrevious() { MoveSelection(-1); }
void UTANearbyPickupComponent::SelectNext() { MoveSelection(1); }

void UTANearbyPickupComponent::MoveSelection(int32 Delta)
{
    if (!IsPickupAvailable() || bPickingUp) return;
    RefreshNearbyItems();
    if (Items.IsEmpty()) return;
    const int32 Index = FMath::Clamp(GetSelectedIndex() + Delta, 0, Items.Num() - 1);
    if (SelectedItem != Items[Index])
    {
        SelectedItem = Items[Index];
        OnNearbyItemsChanged.Broadcast();
    }
}

bool UTANearbyPickupComponent::TryPickupSelected()
{
    const bool bHadItems = HasItems();
    const auto RequestedItem = SelectedItem;
    if (bPickingUp || !IsPickupAvailable()) return bHadItems;
    RefreshNearbyItems();
    // Never redirect a press aimed at a stale row to a different item or a nearby door.
    if (bHadItems && (!RequestedItem.IsValid() || RequestedItem != SelectedItem)) return true;
    auto* Item = SelectedItem.Get();
    if (!Item) return bHadItems;
    if (!IsEligible(Item)) return true;
    TGuardValue<bool> Guard(bPickingUp, true);
    const int32 Before = Item->GetCount();
    ITAInteractable::Execute_OnInteract(Item, GetOwner());
    const int32 Remaining = IsValid(Item) && !Item->IsActorBeingDestroyed() ? Item->GetCount() : 0;
    const int32 Added = FMath::Max(0, Before - Remaining);
    RefreshNearbyItems();
    FText Message;
    if (Added == 0 && IsValid(Item) && Item->ShouldReportPickupFailure())
        if (auto* GI = GetWorld()->GetGameInstance())
            if (auto* Loc = GI->GetSubsystem<UTALocalizeSubsystem>()) Message = Loc->GetText(TEXT("Pickup_Failed"));
    OnPickupResult.Broadcast(Added, Message);
    return true;
}

void UTANearbyPickupComponent::UpdatePanel()
{
    auto* Pawn = Cast<APawn>(GetOwner());
    auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController())
    {
        if (Panel) Panel->RemoveFromParent();
        Panel = nullptr;
        return;
    }
    if (!Panel && !Items.IsEmpty() && PickupPanelClass)
    {
        Panel = CreateWidget<UTAPickupPanelWidget>(PC, PickupPanelClass);
        if (Panel)
        {
            Panel->SetPickupComponent(this);
            Panel->AddToPlayerScreen();
        }
    }
    if (Panel) Panel->SetVisibility(HasItems() && IsPickupAvailable() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UTANearbyPickupComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Panel) Panel->RemoveFromParent();
    Panel = nullptr;
    Items.Reset();
    SelectedItem.Reset();
    Super::EndPlay(Reason);
}
