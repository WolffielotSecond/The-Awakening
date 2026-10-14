#include "UI/Pickup/TAPickupRowWidget.h"
#include "Inventory/TANearbyPickupComponent.h"
#include "Inventory/TAWorldItem.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"

void UTAPickupRowWidget::SetItem(ATAWorldItem* Item, UTANearbyPickupComponent* Component, bool bSelected)
{
    WorldItem = Item;
    PickupComponent = Component;
    bIsSelected = bSelected;
    RefreshPresentation();
}

void UTAPickupRowWidget::RefreshPresentation()
{
    auto* Item = WorldItem.Get();
    auto* Def = Item ? Item->GetItemDef() : nullptr;
    auto* GI = GetGameInstance();
    auto* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
    auto* Icons = GI ? GI->GetSubsystem<UTAInputIconSubsystem>() : nullptr;
    const auto Localize = [Loc](const FString& Id) { return Loc ? Loc->GetText(Id) : FText::GetEmpty(); };
    if (Text_Name) Text_Name->SetText(Def ? Localize(Def->DisplayName.ToString()) : FText::GetEmpty());
    if (Text_Count)
    {
        Text_Count->SetText(Item ? FText::AsNumber(Item->GetCount()) : FText::GetEmpty());
        Text_Count->SetVisibility(Item && Item->GetCount() > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (Image_Item)
    {
        Image_Item->SetBrushFromTexture(Def ? Def->Icon.Get() : nullptr);
        Image_Item->SetVisibility(Def && Def->Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (Border_Selection) Border_Selection->SetBrushColor(bIsSelected ? SelectedColor : NormalColor);
    if (Text_Interact)
    {
        Text_Interact->SetText(Item ? Localize(Item->GetPromptTextId()) : FText::GetEmpty());
        Text_Interact->SetVisibility(bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    const auto SetActionIcon = [&](UImage* Image, UInputAction* Action)
    {
        if (!Image) return;
        auto* PC = GetOwningPlayer();
        auto* Icon = Icons && Action ? Icons->GetIconForActionForPlayer(Action, PC ? PC->GetLocalPlayer() : nullptr) : nullptr;
        Image->SetBrushFromTexture(Icon);
        Image->SetVisibility(bIsSelected && Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    };
    SetActionIcon(Image_InteractKey, PickupComponent.IsValid() ? PickupComponent->InteractAction.Get() : nullptr);
    SetActionIcon(Image_PreviousKey, PickupComponent.IsValid() ? PickupComponent->PreviousAction.Get() : nullptr);
    SetActionIcon(Image_NextKey, PickupComponent.IsValid() ? PickupComponent->NextAction.Get() : nullptr);
    if (VerticalBox_Keys)
        VerticalBox_Keys->SetVisibility(bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    OnPresentationChanged();
}
