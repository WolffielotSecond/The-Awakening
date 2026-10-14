#include "UI/Pickup/TAPickupPanelWidget.h"
#include "UI/Pickup/TAPickupRowWidget.h"
#include "Inventory/TANearbyPickupComponent.h"
#include "Core/TAInputIconSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void UTAPickupPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(false);
    if (auto* GI = GetGameInstance())
        if (auto* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
            Icons->OnInputPromptsChanged.AddUniqueDynamic(this, &UTAPickupPanelWidget::RefreshPresentation);
    BindComponent();
    RefreshRows();
}

void UTAPickupPanelWidget::NativeDestruct()
{
    UnbindComponent();
    if (auto* GI = GetGameInstance())
        if (auto* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
            Icons->OnInputPromptsChanged.RemoveDynamic(this, &UTAPickupPanelWidget::RefreshPresentation);
    Super::NativeDestruct();
}

void UTAPickupPanelWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    // Widget tick still runs when Scan freezes the character's tick. Resuming
    // character discovery restores visibility after gameplay becomes available.
    if (PickupComponent && (!PickupComponent->HasItems() || !PickupComponent->IsPickupAvailable()))
        SetVisibility(ESlateVisibility::Collapsed);
}

void UTAPickupPanelWidget::UnbindComponent()
{
    if (!PickupComponent) return;
    PickupComponent->OnNearbyItemsChanged.RemoveDynamic(this, &UTAPickupPanelWidget::RefreshRows);
    PickupComponent->OnPickupResult.RemoveDynamic(this, &UTAPickupPanelWidget::HandlePickupResult);
}
void UTAPickupPanelWidget::BindComponent()
{
    if (!PickupComponent) return;
    PickupComponent->OnNearbyItemsChanged.AddUniqueDynamic(this, &UTAPickupPanelWidget::RefreshRows);
    PickupComponent->OnPickupResult.AddUniqueDynamic(this, &UTAPickupPanelWidget::HandlePickupResult);
}
void UTAPickupPanelWidget::SetPickupComponent(UTANearbyPickupComponent* Component)
{
    UnbindComponent();
    PickupComponent = Component;
    BindComponent();
    ClearFeedback();
    RefreshRows();
}

void UTAPickupPanelWidget::RefreshRows()
{
    if (!Scroll_Items || !PickupComponent || !RowWidgetClass) return;
    const auto Items = PickupComponent->GetNearbyItems();
    // Reuse rows on selection/count changes; preserve scrolling until a new row is selected.
    while (Rows.Num() > Items.Num())
    {
        Rows.Last()->RemoveFromParent();
        Rows.Pop();
    }
    while (Rows.Num() < Items.Num())
    {
        auto* Row = CreateWidget<UTAPickupRowWidget>(GetOwningPlayer(), RowWidgetClass);
        if (!Row) break;
        Rows.Add(Row);
        if (auto* RowSlot = Cast<UScrollBoxSlot>(Scroll_Items->AddChild(Row)))
            RowSlot->SetPadding(FMargin(0.f, RowVerticalPadding, 0.f, RowVerticalPadding));
    }
    UTAPickupRowWidget* SelectedRow = nullptr;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const bool bSelected = Items[Index] == PickupComponent->GetSelectedItem();
        Rows[Index]->SetItem(Items[Index], PickupComponent, bSelected);
        if (bSelected) SelectedRow = Rows[Index];
    }
    if (SelectedRow) Scroll_Items->ScrollWidgetIntoView(SelectedRow, false);
    if (Items.IsEmpty()) ClearFeedback();
    RefreshPresentation();
}

void UTAPickupPanelWidget::SetRowVerticalPadding(float InPadding)
{
    RowVerticalPadding = InPadding;
    for (const auto& Row : Rows)
        if (Row)
            if (auto* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
                RowSlot->SetPadding(FMargin(0.f, RowVerticalPadding, 0.f, RowVerticalPadding));
}

void UTAPickupPanelWidget::RefreshPresentation()
{
    auto* GI = GetGameInstance();
    auto* Icons = GI ? GI->GetSubsystem<UTAInputIconSubsystem>() : nullptr;
    auto* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
    auto* PC = GetOwningPlayer();
    const auto SetIcon = [&](UImage* Image, UInputAction* Action)
    {
        if (!Image) return;
        auto* Icon = Icons ? Icons->GetIconForActionForPlayer(Action, PC ? PC->GetLocalPlayer() : nullptr) : nullptr;
        Image->SetBrushFromTexture(Icon);
        Image->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    };
    SetIcon(Image_PreviousKey, PickupComponent ? PickupComponent->PreviousAction.Get() : nullptr);
    SetIcon(Image_NextKey, PickupComponent ? PickupComponent->NextAction.Get() : nullptr);
    if (Text_Previous) Text_Previous->SetText(Loc ? Loc->GetText(TEXT("Pickup_Previous")) : FText::GetEmpty());
    if (Text_Next) Text_Next->SetText(Loc ? Loc->GetText(TEXT("Pickup_Next")) : FText::GetEmpty());
    for (const auto& Row : Rows) if (Row) Row->RefreshPresentation();
    RefreshFeedbackText();
}

void UTAPickupPanelWidget::RefreshFeedbackText()
{
    if (!Text_Feedback) return;
    auto* GI = GetGameInstance();
    auto* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
    Text_Feedback->SetText(bHasFailureFeedback && Loc ? Loc->GetText(TEXT("Pickup_Failed")) : FText::GetEmpty());
    Text_Feedback->SetVisibility(bHasFailureFeedback ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
void UTAPickupPanelWidget::ClearFeedback()
{
    bHasFailureFeedback = false;
    RefreshFeedbackText();
}
void UTAPickupPanelWidget::HandlePickupResult(int32 AddedCount, FText Message)
{
    bHasFailureFeedback = AddedCount == 0;
    RefreshFeedbackText();
    OnPickupFeedback(AddedCount, Message);
}
