#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAClothingDefinition.h"
#include "Core/TAPlayerState.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/NamedSlot.h"
#include "GameFramework/PlayerController.h"
#include "Core/TALocalizeSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

FReply UTAInventoryPanelWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Prevent OS/gamepad key repeat from reaching Blueprint close handlers after opening the panel.
	if (InKeyEvent.IsRepeat() && (InKeyEvent.GetKey() == EKeys::Tab || InKeyEvent.GetKey() == EKeys::Gamepad_Special_Left))
		return FReply::Handled();
	if (InKeyEvent.GetKey() != EKeys::Gamepad_FaceButton_Bottom)
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	if (!InKeyEvent.IsRepeat() && FSlateApplication::IsInitialized())
	{
		const FVector2D CursorPosition = FSlateApplication::Get().GetCursorPos();
		auto IsUnderCursor = [&CursorPosition](UButton* Button)
		{
			return Button && Button->IsVisible() && Button->GetIsEnabled() && Button->GetCachedGeometry().IsUnderLocation(CursorPosition);
		};
		if (IsUnderCursor(Button_Inventory)) OnClickInventoryTab();
		else if (IsUnderCursor(Button_Skills)) OnClickSkillsTab();
	}
	// Do not let an unrelated focused tab consume the same confirmation.
	return FReply::Handled();
}

void UTAInventoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (!SlotWidgetClass)
	{
		SlotWidgetClass = UTAInventorySlotWidget::StaticClass();
	}
	if (!ClothingPanelClass)
	{
		ClothingPanelClass = UTAClothingPanelWidget::StaticClass();
	}

	EnsureDynamicChildren();

	if (Button_Inventory)
	{
		Button_Inventory->OnClicked.AddDynamic(this, &UTAInventoryPanelWidget::OnClickInventoryTab);
	}
	if (Button_Skills)
	{
		Button_Skills->OnClicked.AddDynamic(this, &UTAInventoryPanelWidget::OnClickSkillsTab);
	}

	if (WidgetSwitcher)
	{
		WidgetSwitcher->SetActiveWidgetIndex(0);
	}

	RefreshLocalizedChrome();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddDynamic(this, &UTAInventoryPanelWidget::HandleLanguageChanged);
		}
	}

}

void UTAInventoryPanelWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleLanguageChanged);
		}
	}

	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleInventoryUpdated);
	}

	Super::NativeDestruct();
}

void UTAInventoryPanelWidget::EnsureDynamicChildren()
{
	if (!SlotWidgetClass)
	{
		SlotWidgetClass = UTAInventorySlotWidget::StaticClass();
	}
	if (!ClothingPanelClass)
	{
		ClothingPanelClass = UTAClothingPanelWidget::StaticClass();
	}
	auto SpawnSlotInto = [&](UNamedSlot* Named, TObjectPtr<UTAInventorySlotWidget>& OutSlot)
		{
			if (!Named || OutSlot)
			{
				return;
			}
			OutSlot = CreateWidget<UTAInventorySlotWidget>(this, SlotWidgetClass);
			if (OutSlot)
			{
				Named->ClearChildren();
				Named->AddChild(OutSlot);
				OutSlot->SetEmpty();
			}
		};

	SpawnSlotInto(NamedSlot_OuterEquip, OuterEquipSlot);

	StorySlots.SetNum(4);
	SpawnSlotInto(NamedSlot_Story0, StorySlots[0]);
	SpawnSlotInto(NamedSlot_Story1, StorySlots[1]);
	SpawnSlotInto(NamedSlot_Story2, StorySlots[2]);
	SpawnSlotInto(NamedSlot_Story3, StorySlots[3]);

	if (NamedSlot_InnerClothing && !InnerClothingPanel)
	{
		InnerClothingPanel = CreateWidget<UTAClothingPanelWidget>(this, ClothingPanelClass);
		if (InnerClothingPanel)
		{
			InnerClothingPanel->SlotWidgetClass = SlotWidgetClass;
			NamedSlot_InnerClothing->ClearChildren();
			NamedSlot_InnerClothing->AddChild(InnerClothingPanel);
		}
	}

	if (NamedSlot_OuterClothing && !OuterClothingPanel)
	{
		OuterClothingPanel = CreateWidget<UTAClothingPanelWidget>(this, ClothingPanelClass);
		if (OuterClothingPanel)
		{
			OuterClothingPanel->SlotWidgetClass = SlotWidgetClass;
			NamedSlot_OuterClothing->ClearChildren();
			NamedSlot_OuterClothing->AddChild(OuterClothingPanel);
			OuterClothingPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTAInventoryPanelWidget::Init(UTAInventoryComponent* InInventory)
{
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UTAInventoryPanelWidget::HandleInventoryUpdated);
	}

	Inventory = InInventory;
	EnsureDynamicChildren();

	if (Inventory)
	{
		Inventory->OnInventoryUpdated.AddDynamic(this, &UTAInventoryPanelWidget::HandleInventoryUpdated);
	}

	RefreshAll();
}

void UTAInventoryPanelWidget::HandleInventoryUpdated()
{
	RefreshAll();
}

void UTAInventoryPanelWidget::OnClickInventoryTab()
{
	if (WidgetSwitcher)
	{
		WidgetSwitcher->SetActiveWidgetIndex(0);
	}
}

void UTAInventoryPanelWidget::OnClickSkillsTab()
{
	if (WidgetSwitcher)
	{
		WidgetSwitcher->SetActiveWidgetIndex(1);
	}
}

void UTAInventoryPanelWidget::RefreshAll()
{
	EnsureDynamicChildren();
	if (!Inventory)
	{
		return;
	}

	if (Text_Money)
	{
		int32 Money = 0;
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (ATAPlayerState* PS = PC->GetPlayerState<ATAPlayerState>())
			{
				Money = PS->GetMoney();
			}
		}

		FString Line = FString::Printf(TEXT("细胞: %d"), Money);
		if (UGameInstance* GI = GetGameInstance())
		{
			if (const UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
			{
				const FString Fmt = Loc->GetText(TEXT("UI_Cells")).ToString();
				Line = Fmt.Replace(TEXT("{0}"), *FString::FromInt(Money));
			}
		}
		Text_Money->SetText(FText::FromString(Line));
	}

	const TArray<FTAInventorySlot>& Stories = Inventory->GetStorySlots();
	for (int32 i = 0; i < StorySlots.Num(); ++i)
	{
		if (!StorySlots[i])
		{
			continue;
		}
		if (Stories.IsValidIndex(i) && !Stories[i].IsEmpty())
		{
			StorySlots[i]->SetSlotData(Stories[i], -100 - i);
		}
		else
		{
			StorySlots[i]->SetEmpty();
		}
	}

	if (OuterEquipSlot)
	{
		OuterEquipSlot->SetEmpty(); // 后续可显示外套图标
	}

	if (InnerClothingPanel)
	{
		InnerClothingPanel->BuildFromClothing(Inventory->GetInnerClothing(), 0);
	}

	if (OuterClothingPanel)
	{
		if (Inventory->HasOuterClothing())
		{
			OuterClothingPanel->SetVisibility(ESlateVisibility::Visible);
			if (NamedSlot_OuterClothing)
			{
				NamedSlot_OuterClothing->SetVisibility(ESlateVisibility::Visible);
			}

			int32 InnerSlots = 0;
			for (const FTAPocketRuntime& P : Inventory->GetInnerClothing().Pockets)
			{
				InnerSlots += P.Slots.Num();
			}
			OuterClothingPanel->BuildFromClothing(Inventory->GetOuterClothing(), InnerSlots);
		}
		else
		{
			OuterClothingPanel->ClearPanel();
			OuterClothingPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTAInventoryPanelWidget::RefreshLocalizedChrome()
{
	const UTALocalizeSubsystem* Loc = nullptr;
	if (UGameInstance* GI = GetGameInstance())
	{
		Loc = GI->GetSubsystem<UTALocalizeSubsystem>();
	}

	auto Apply = [Loc](UTextBlock* Block, const TCHAR* TextId, const TCHAR* Fallback)
		{
			if (!Block)
			{
				return;
			}
			if (Loc)
			{
				Block->SetText(Loc->GetText(TextId));
			}
			else
			{
				Block->SetText(FText::FromString(Fallback));
			}
		};

	Apply(Text_Inventory, TEXT("UI_Inventory"), TEXT("背包"));
	Apply(Text_Skills, TEXT("UI_Skill"), TEXT("技能"));
	Apply(Text_SkillsPlaceholder, TEXT("Common_UnavailableInThisVersion"), TEXT("当前版本中不可用"));
}

void UTAInventoryPanelWidget::HandleLanguageChanged()
{
	RefreshLocalizedChrome();
	RefreshAll();
}
