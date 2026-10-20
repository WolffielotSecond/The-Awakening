#include "UI/Inventory/TAQuantityDialogWidget.h"
#include "Core/TALocalizeSubsystem.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "The_AwakeningPlayerController.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "InputAction.h"

namespace
{
	constexpr int32 QuantityDialogZOrder = 72;

	void QuantityLoadAction(TObjectPtr<UInputAction>& Target, const TCHAR* Path)
	{
		if (!Target)
		{
			Target = LoadObject<UInputAction>(nullptr, Path);
		}
	}
}

UTAQuantityDialogWidget* UTAQuantityDialogWidget::Open(APlayerController* PC, TSubclassOf<UTAQuantityDialogWidget> WidgetClass,
	const FString& InTitleTextId, int32 Min, int32 Max, TFunction<void(int32)> InOnConfirm)
{
	if (!PC || Max < Min || Max < 1)
	{
		return nullptr;
	}
	UClass* Class = WidgetClass ? WidgetClass.Get() : LoadClass<UTAQuantityDialogWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_QuantityDialog.WBP_QuantityDialog_C"));
	if (!Class)
	{
		UE_LOG(LogTemp, Warning, TEXT("WBP_QuantityDialog missing; run -run=TADocumentUI."));
		return nullptr;
	}
	UTAQuantityDialogWidget* Dialog = CreateWidget<UTAQuantityDialogWidget>(PC, Class);
	if (!Dialog)
	{
		return nullptr;
	}
	Dialog->TitleTextId = InTitleTextId;
	Dialog->MinQuantity = FMath::Max(1, Min);
	Dialog->MaxQuantity = Max;
	Dialog->Quantity = Dialog->MinQuantity;
	Dialog->OnConfirm = MoveTemp(InOnConfirm);
	Dialog->AddToViewport(QuantityDialogZOrder);
	return Dialog;
}

FText UTAQuantityDialogWidget::Localize(const FString& TextId) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	return Loc ? Loc->GetText(TextId) : FText::FromString(TextId);
}

void UTAQuantityDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	QuantityLoadAction(DecreaseAction, TEXT("/Game/Input/Actions/UI/IA_QuantityDecrease.IA_QuantityDecrease"));
	QuantityLoadAction(IncreaseAction, TEXT("/Game/Input/Actions/UI/IA_QuantityIncrease.IA_QuantityIncrease"));
	QuantityLoadAction(ConfirmAction, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Confirm.IA_Inventory_Confirm"));
	QuantityLoadAction(ChoiceConfirmAction, TEXT("/Game/Input/Actions/IA_ChoiceConfirm.IA_ChoiceConfirm"));
	QuantityLoadAction(BackAction, TEXT("/Game/Input/Actions/IA_UIBack.IA_UIBack"));

	if (Button_Decrease) Button_Decrease->OnClicked.AddUniqueDynamic(this, &UTAQuantityDialogWidget::HandleDecreaseClicked);
	if (Button_Increase) Button_Increase->OnClicked.AddUniqueDynamic(this, &UTAQuantityDialogWidget::HandleIncreaseClicked);
	if (Button_Confirm) Button_Confirm->OnClicked.AddUniqueDynamic(this, &UTAQuantityDialogWidget::HandleConfirmClicked);
	if (Button_Cancel) Button_Cancel->OnClicked.AddUniqueDynamic(this, &UTAQuantityDialogWidget::HandleCancelClicked);
	if (Text_Title) Text_Title->SetText(Localize(TitleTextId));
	if (Text_Confirm) Text_Confirm->SetText(Localize(TEXT("UI_Common_Confirm")));
	if (Text_Cancel) Text_Cancel->SetText(Localize(TEXT("UI_Common_Cancel")));
	BuildControls();
	Refresh();

	SetIsFocusable(true);
	if (auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		// 与背包相同：光标常显，手柄可用虚拟光标点按钮
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

void UTAQuantityDialogWidget::NativeDestruct()
{
	ReleaseInput();
	if (Button_Decrease) Button_Decrease->OnClicked.RemoveDynamic(this, &UTAQuantityDialogWidget::HandleDecreaseClicked);
	if (Button_Increase) Button_Increase->OnClicked.RemoveDynamic(this, &UTAQuantityDialogWidget::HandleIncreaseClicked);
	if (Button_Confirm) Button_Confirm->OnClicked.RemoveDynamic(this, &UTAQuantityDialogWidget::HandleConfirmClicked);
	if (Button_Cancel) Button_Cancel->OnClicked.RemoveDynamic(this, &UTAQuantityDialogWidget::HandleCancelClicked);
	Super::NativeDestruct();
}

void UTAQuantityDialogWidget::BuildControls()
{
	if (!HorizontalBox_Controls)
	{
		return;
	}
	HorizontalBox_Controls->ClearChildren();
	auto Add = [this](UInputAction* Action, const TCHAR* TextId) -> UTAActionPromptWidget*
	{
		UTAActionPromptWidget* Prompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, Action, TextId);
		if (Prompt)
		{
			Prompt->OnPromptClicked.AddDynamic(this, &UTAQuantityDialogWidget::HandlePromptClicked);
		}
		return Prompt;
	};
	DecreasePrompt = Add(DecreaseAction, TEXT("UI_Quantity_Decrease"));
	IncreasePrompt = Add(IncreaseAction, TEXT("UI_Quantity_Increase"));
	ConfirmPrompt = Add(ConfirmAction, TEXT("UI_Common_Confirm"));
	CancelPrompt = Add(BackAction, TEXT("UI_Common_Cancel"));
}

void UTAQuantityDialogWidget::Refresh()
{
	if (Text_Quantity)
	{
		Text_Quantity->SetText(FText::AsNumber(Quantity));
	}
	if (Button_Decrease) Button_Decrease->SetIsEnabled(Quantity > MinQuantity);
	if (Button_Increase) Button_Increase->SetIsEnabled(Quantity < MaxQuantity);
}

void UTAQuantityDialogWidget::ChangeQuantity(int32 Delta)
{
	Quantity = FMath::Clamp(Quantity + Delta, MinQuantity, MaxQuantity);
	Refresh();
}

void UTAQuantityDialogWidget::Confirm()
{
	TFunction<void(int32)> Callback = MoveTemp(OnConfirm);
	OnConfirm = nullptr;
	const int32 Chosen = Quantity;
	// 先关窗口（交还输入），再执行
	RemoveFromParent();
	if (Callback)
	{
		Callback(Chosen);
	}
}

void UTAQuantityDialogWidget::Cancel()
{
	OnConfirm = nullptr;
	RemoveFromParent();
}

void UTAQuantityDialogWidget::HandleDecreaseClicked() { if (AllowsPlayerInput(ETAInputCapability::Cursor)) ChangeQuantity(-1); }
void UTAQuantityDialogWidget::HandleIncreaseClicked() { if (AllowsPlayerInput(ETAInputCapability::Cursor)) ChangeQuantity(1); }
void UTAQuantityDialogWidget::HandleConfirmClicked() { if (AllowsPlayerInput(ETAInputCapability::Cursor)) Confirm(); }
void UTAQuantityDialogWidget::HandleCancelClicked() { if (AllowsPlayerInput(ETAInputCapability::Cursor)) Cancel(); }

void UTAQuantityDialogWidget::HandlePromptClicked(UTAActionPromptWidget* Prompt)
{
	if (!AllowsPlayerInput(ETAInputCapability::Cursor)) return;
	if (Prompt == DecreasePrompt) ChangeQuantity(-1);
	else if (Prompt == IncreasePrompt) ChangeQuantity(1);
	else if (Prompt == ConfirmPrompt) Confirm();
	else if (Prompt == CancelPrompt) Cancel();
}

FReply UTAQuantityDialogWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 窗口内交给按钮；窗口外吃掉点击，不穿透到背包（窗口保持打开，用取消或返回键关闭）
	if (Panel_Dialog && Panel_Dialog->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	return FReply::Handled();
}

// ==================== 输入 ====================

void UTAQuantityDialogWidget::ReleaseInput()
{
	const auto Handle = InputRequestHandle;
	InputRequestHandle = 0;
	const auto Issuer = InputRequestController;
	InputRequestController.Reset();
	if (Handle)
		if (auto* PC = Issuer.Get()) PC->ReleaseInputRequest(Handle);
}

void UTAQuantityDialogWidget::RemoveFromParent()
{
	ReleaseInput();
	Super::RemoveFromParent();
}

bool UTAQuantityDialogWidget::AllowsPlayerInput(ETAInputCapability Capability) const
{
	const auto* PC = InputRequestController.Get();
	return PC && PC->AllowsInputFor(InputRequestHandle, this, Capability);
}

TOptional<ETAInputCapability> UTAQuantityDialogWidget::ResolvePlayerInput(FKey Key) const
{
	const auto* PC = InputRequestController.Get();
	if (!PC || Key.IsMouseButton()) return {};
	if ((DecreaseAction && PC->IsKeyMappedToAction(Key, DecreaseAction)) || (IncreaseAction && PC->IsKeyMappedToAction(Key, IncreaseAction)))
		return ETAInputCapability::Navigate;
	if ((ConfirmAction && PC->IsKeyMappedToAction(Key, ConfirmAction)) || (ChoiceConfirmAction && PC->IsKeyMappedToAction(Key, ChoiceConfirmAction)))
		return ETAInputCapability::Confirm;
	return {};
}

void UTAQuantityDialogWidget::ExecutePlayerInput(FKey Key, ETAInputCapability Capability)
{
	if (!AllowsPlayerInput(Capability)) return;
	const auto* PC = InputRequestController.Get();
	if (Capability == ETAInputCapability::Navigate)
	{
		ChangeQuantity(PC && DecreaseAction && PC->IsKeyMappedToAction(Key, DecreaseAction) ? -1 : 1);
	}
	else if (Capability == ETAInputCapability::Confirm)
	{
		Confirm();
	}
}

bool UTAQuantityDialogWidget::HandleMenuBackRequested()
{
	if (!AllowsPlayerInput(ETAInputCapability::Close)) return false;
	Cancel();
	return true;
}
