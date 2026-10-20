#include "UI/Inventory/TAItemActionMenuWidget.h"
#include "Document/TADocumentSettings.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TALocalizeSubsystem.h"
#include "The_AwakeningPlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "InputAction.h"

const FName UTAItemActionMenuWidget::ActionViewContent(TEXT("ViewContent"));
const FName UTAItemActionMenuWidget::ActionInspect(TEXT("Inspect"));
const FName UTAItemActionMenuWidget::ActionDrop(TEXT("Drop"));
const FName UTAItemActionMenuWidget::ActionPartialDrop(TEXT("PartialDrop"));
const FName UTAItemActionMenuWidget::ActionSplit(TEXT("Split"));

namespace
{
	constexpr int32 MenuZOrder = 70;
	const FLinearColor HighlightColor(0.12f, 0.42f, 0.82f, 1.f);
	const FLinearColor NormalColor(0.15f, 0.15f, 0.15f, 0.9f);
}

// ==================== 选项行 ====================

void UTAItemActionOptionWidget::Setup(UTAItemActionMenuWidget* InMenu, int32 InIndex, const FText& Label, bool bEnabled)
{
	Menu = InMenu;
	Index = InIndex;
	bOptionEnabled = bEnabled;
	if (Text_Option)
	{
		Text_Option->SetText(Label);
		Text_Option->SetColorAndOpacity(bEnabled ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.45f));
	}
	if (Button_Option)
	{
		Button_Option->SetIsEnabled(bEnabled);
	}
	SetHighlighted(false);
}

void UTAItemActionOptionWidget::SetHighlighted(bool bHighlighted)
{
	// 置灰的选项也能被选中：高亮照常显示，只是不能执行
	const bool bShow = bHighlighted;
	if (Border_Highlight)
	{
		Border_Highlight->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	else if (Button_Option)
	{
		Button_Option->SetBackgroundColor(bShow ? HighlightColor : NormalColor);
	}
}

void UTAItemActionOptionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Option)
	{
		Button_Option->OnClicked.AddUniqueDynamic(this, &UTAItemActionOptionWidget::HandleClicked);
	}
}

void UTAItemActionOptionWidget::NativeDestruct()
{
	if (Button_Option)
	{
		Button_Option->OnClicked.RemoveDynamic(this, &UTAItemActionOptionWidget::HandleClicked);
	}
	Super::NativeDestruct();
}

void UTAItemActionOptionWidget::HandleClicked()
{
	if (UTAItemActionMenuWidget* Owner = Menu.Get(); Owner && bOptionEnabled)
	{
		Owner->ChooseOption(Index);
	}
}

// ==================== 菜单 ====================

TArray<FTAItemActionOption> UTAItemActionMenuWidget::GetOptionsForSlot(const FTAInventorySlot& Slot, bool bHasEmptySlot)
{
	TArray<FTAItemActionOption> Result;
	const UTAItemDefinition* Item = Slot.IsEmpty() ? nullptr : Slot.ItemDef.Get();
	if (!Item)
	{
		return Result;
	}
	if (Item->ItemType == ETAItemType::QuestItem)
	{
		if (Item == GetDefault<UTADocumentSettings>()->DiaryItem.LoadSynchronous())
		{
			Result.Add({ ActionViewContent, TEXT("UI_Item_ViewContent"), true });
		}
		else
		{
			Result.Add({ ActionInspect, TEXT("UI_Item_Inspect"), true });
		}
		return Result;
	}
	Result.Add({ ActionInspect, TEXT("UI_Item_Inspect"), true });
	if (Item->bCanStack)
	{
		Result.Add({ ActionPartialDrop, TEXT("UI_Item_PartialDrop"), true });
		// 背包没有空格子时无法拆分（不管有没有可堆叠的格子）
		Result.Add({ ActionSplit, TEXT("UI_Item_Split"), bHasEmptySlot });
	}
	else
	{
		Result.Add({ ActionDrop, TEXT("UI_Item_Drop"), true });
	}
	return Result;
}

UTAItemActionMenuWidget* UTAItemActionMenuWidget::Open(APlayerController* PC, TSubclassOf<UTAItemActionMenuWidget> WidgetClass,
	TArray<FTAItemActionOption> InOptions, TFunction<void(FName)> InOnChosen)
{
	if (!PC || InOptions.IsEmpty())
	{
		return nullptr;
	}
	UClass* Class = WidgetClass ? WidgetClass.Get() : LoadClass<UTAItemActionMenuWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ItemActionMenu.WBP_ItemActionMenu_C"));
	if (!Class)
	{
		UE_LOG(LogTemp, Warning, TEXT("WBP_ItemActionMenu missing; run -run=TADocumentUI."));
		return nullptr;
	}
	UTAItemActionMenuWidget* Menu = CreateWidget<UTAItemActionMenuWidget>(PC, Class);
	if (!Menu)
	{
		return nullptr;
	}
	Menu->Options = MoveTemp(InOptions);
	Menu->OnChosen = MoveTemp(InOnChosen);
	Menu->AddToViewport(MenuZOrder);
	return Menu;
}

void UTAItemActionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	auto LoadAction = [](TObjectPtr<UInputAction>& Target, const TCHAR* Path)
	{
		if (!Target) Target = LoadObject<UInputAction>(nullptr, Path);
	};
	LoadAction(ConfirmAction, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Confirm.IA_Inventory_Confirm"));
	LoadAction(ChoiceConfirmAction, TEXT("/Game/Input/Actions/IA_ChoiceConfirm.IA_ChoiceConfirm"));
	LoadAction(PreviousAction, TEXT("/Game/Input/Actions/IA_ChoicePrevious.IA_ChoicePrevious"));
	LoadAction(NextAction, TEXT("/Game/Input/Actions/IA_ChoiceNext.IA_ChoiceNext"));
	if (!OptionWidgetClass)
	{
		OptionWidgetClass = LoadClass<UTAItemActionOptionWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ItemActionOption.WBP_ItemActionOption_C"));
	}
	BuildOptions();
	PlaceAtCursor();

	SetIsFocusable(true);
	if (auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		// 与背包相同：光标常显，手柄可用虚拟光标；上下选择和确定键直接操作选中项
		FTAInputRequest Request;
		Request.Owner = this;
		Request.Priority = InputPriority;
		Request.Allowed = {ETAInputCapability::Cursor, ETAInputCapability::Navigate, ETAInputCapability::Confirm, ETAInputCapability::Close};
		Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
		Request.Presentation.bShowCursor = true;
		Request.Presentation.Focus = ETAInputFocusRequirement::Target;
		Request.Presentation.FocusTarget = this;
		InputRequestController = PC;
		InputRequestHandle = PC->AcquireInputRequest(Request);
	}
}

void UTAItemActionMenuWidget::NativeDestruct()
{
	ReleaseInput();
	Super::NativeDestruct();
}

void UTAItemActionMenuWidget::BuildOptions()
{
	if (!VerticalBox_Options || !OptionWidgetClass)
	{
		return;
	}
	const UGameInstance* GI = GetGameInstance();
	const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	VerticalBox_Options->ClearChildren();
	OptionWidgets.Reset();
	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		UTAItemActionOptionWidget* Option = CreateWidget<UTAItemActionOptionWidget>(this, OptionWidgetClass);
		if (!Option)
		{
			continue;
		}
		VerticalBox_Options->AddChildToVerticalBox(Option);
		const FTAItemActionOption& Data = Options[Index];
		Option->Setup(this, Index, Loc ? Loc->GetText(Data.LabelTextId) : FText::FromString(Data.LabelTextId), Data.bEnabled);
		Option->OnHovered.AddUniqueDynamic(this, &UTAItemActionMenuWidget::HandleOptionHovered);
		OptionWidgets.Add(Option);
	}
	// 固定先选中第一项
	SelectOption(0);
}

void UTAItemActionMenuWidget::PlaceAtCursor()
{
	auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer());
	UCanvasPanelSlot* MenuSlot = Panel_Menu ? Cast<UCanvasPanelSlot>(Panel_Menu->Slot) : nullptr;
	float X = 0.f, Y = 0.f;
	if (!PC || !MenuSlot || !PC->GetPlayerCursorPosition(X, Y))
	{
		return;
	}
	// 本控件全屏铺在视口上，根 Canvas 的局部坐标就是视口坐标（Slate 单位）
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(PC), KINDA_SMALL_NUMBER);
	int32 ViewportX = 0, ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);
	Panel_Menu->ForceLayoutPrepass();
	const FVector2D Desired = Panel_Menu->GetDesiredSize();
	FVector2D Position(X / Scale + CursorOffset.X, Y / Scale + CursorOffset.Y);
	Position.X = FMath::Clamp(Position.X, 0.f, FMath::Max(0.f, ViewportX / Scale - Desired.X));
	Position.Y = FMath::Clamp(Position.Y, 0.f, FMath::Max(0.f, ViewportY / Scale - Desired.Y));
	MenuSlot->SetAnchors(FAnchors(0.f, 0.f));
	MenuSlot->SetAlignment(FVector2D::ZeroVector);
	MenuSlot->SetAutoSize(true);
	MenuSlot->SetPosition(Position);
}

void UTAItemActionMenuWidget::SelectOption(int32 OptionIndex)
{
	if (OptionWidgets.IsEmpty())
	{
		return;
	}
	SelectedIndex = (OptionIndex % OptionWidgets.Num() + OptionWidgets.Num()) % OptionWidgets.Num();
	for (int32 Index = 0; Index < OptionWidgets.Num(); ++Index)
	{
		if (OptionWidgets[Index])
		{
			OptionWidgets[Index]->SetHighlighted(Index == SelectedIndex);
		}
	}
}

void UTAItemActionMenuWidget::HandleOptionHovered(UTASelectableMenuOptionWidget* Option)
{
	if (!AllowsPlayerInput(ETAInputCapability::Cursor))
	{
		return;
	}
	// 光标移到哪一项就选中哪一项（只选中，不执行）
	const int32 Index = OptionWidgets.IndexOfByKey(Cast<UTAItemActionOptionWidget>(Option));
	if (Index != INDEX_NONE)
	{
		SelectOption(Index);
	}
}

void UTAItemActionMenuWidget::ChooseOption(int32 OptionIndex)
{
	if (!Options.IsValidIndex(OptionIndex) || !Options[OptionIndex].bEnabled || !AllowsPlayerInput(ETAInputCapability::Confirm))
	{
		return;
	}
	const FName ActionId = Options[OptionIndex].ActionId;
	TFunction<void(FName)> Callback = MoveTemp(OnChosen);
	OnChosen = nullptr;
	// 先关菜单（交还输入），再执行选项：选项可能打开新界面
	Close();
	if (Callback)
	{
		Callback(ActionId);
	}
}

void UTAItemActionMenuWidget::Close()
{
	RemoveFromParent();
}

void UTAItemActionMenuWidget::RemoveFromParent()
{
	ReleaseInput();
	Super::RemoveFromParent();
}

void UTAItemActionMenuWidget::ReleaseInput()
{
	const auto Handle = InputRequestHandle;
	InputRequestHandle = 0;
	const auto Issuer = InputRequestController;
	InputRequestController.Reset();
	if (Handle)
		if (auto* PC = Issuer.Get()) PC->ReleaseInputRequest(Handle);
}

bool UTAItemActionMenuWidget::AllowsPlayerInput(ETAInputCapability Capability) const
{
	const auto* PC = InputRequestController.Get();
	return PC && PC->AllowsInputFor(InputRequestHandle, this, Capability);
}

TOptional<ETAInputCapability> UTAItemActionMenuWidget::ResolvePlayerInput(FKey Key) const
{
	const auto* PC = InputRequestController.Get();
	// 鼠标按键走 Slate 点击（选项按钮 / 空白处关闭），不在这里解析
	if (!PC || Key.IsMouseButton()) return {};
	if ((PreviousAction && PC->IsKeyMappedToAction(Key, PreviousAction)) || (NextAction && PC->IsKeyMappedToAction(Key, NextAction)))
		return ETAInputCapability::Navigate;
	if ((ConfirmAction && PC->IsKeyMappedToAction(Key, ConfirmAction)) || (ChoiceConfirmAction && PC->IsKeyMappedToAction(Key, ChoiceConfirmAction)))
		return ETAInputCapability::Confirm;
	return {};
}

void UTAItemActionMenuWidget::ExecutePlayerInput(FKey Key, ETAInputCapability Capability)
{
	if (!AllowsPlayerInput(Capability)) return;
	const auto* PC = InputRequestController.Get();
	if (Capability == ETAInputCapability::Navigate)
	{
		SelectOption(SelectedIndex + (PC && PreviousAction && PC->IsKeyMappedToAction(Key, PreviousAction) ? -1 : 1));
	}
	else if (Capability == ETAInputCapability::Confirm)
	{
		// 确定键执行选中项（置灰的选项不执行）
		ChooseOption(SelectedIndex);
	}
}

FReply UTAItemActionMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Panel_Menu && Panel_Menu->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	// 点在菜单以外：关闭菜单，并吃掉这次点击，后面的背包收不到
	if (AllowsPlayerInput(ETAInputCapability::Cursor))
	{
		Close();
	}
	return FReply::Handled();
}

bool UTAItemActionMenuWidget::HandleMenuBackRequested()
{
	if (!AllowsPlayerInput(ETAInputCapability::Close)) return false;
	Close();
	return true;
}
