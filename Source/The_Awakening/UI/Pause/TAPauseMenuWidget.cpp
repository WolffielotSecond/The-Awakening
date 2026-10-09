#include "UI/Pause/TAPauseMenuWidget.h"

#include "UI/Pause/TAPauseMenuOptionWidget.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "The_AwakeningPlayerController.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Widgets/SWidget.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "The_Awakening.h"

UTAPauseMenuWidget::UTAPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UInputAction> PreviousActionAsset(
		TEXT("/Game/Input/Actions/IA_ChoicePrevious.IA_ChoicePrevious"));
	static ConstructorHelpers::FObjectFinder<UInputAction> NextActionAsset(
		TEXT("/Game/Input/Actions/IA_ChoiceNext.IA_ChoiceNext"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ConfirmActionAsset(
		TEXT("/Game/Input/Actions/IA_ChoiceConfirm.IA_ChoiceConfirm"));
	if (PreviousActionAsset.Succeeded())
	{
		PreviousOptionAction = PreviousActionAsset.Object;
	}
	if (NextActionAsset.Succeeded())
	{
		NextOptionAction = NextActionAsset.Object;
	}
	if (ConfirmActionAsset.Succeeded())
	{
		ConfirmOptionAction = ConfirmActionAsset.Object;
	}

	Options = {
		FTAPauseMenuOption(FName(TEXT("Resume")), TEXT("UI_Pause_Resume"), NSLOCTEXT("PauseMenu", "Resume", "继续游戏"), ETAPauseMenuAction::Resume),
		FTAPauseMenuOption(FName(TEXT("Settings")), TEXT("UI_Settings"), NSLOCTEXT("PauseMenu", "Settings", "设置"), ETAPauseMenuAction::OpenSettings),
		FTAPauseMenuOption(FName(TEXT("Quit")), TEXT("UI_Pause_QuitGame"), NSLOCTEXT("PauseMenu", "Quit", "关闭游戏"), ETAPauseMenuAction::QuitGame)
	};
}

void UTAPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ValidateInputActions();
	BuildOptions();
	RefreshOptionHighlights();
	BuildActionPromptBar();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Localize = GameInstance->GetSubsystem<UTALocalizeSubsystem>())
		{
			Localize->OnLanguageChanged.AddUniqueDynamic(this, &UTAPauseMenuWidget::HandleLanguageChanged);
		}
	}
}

void UTAPauseMenuWidget::NativeDestruct()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Localize = GameInstance->GetSubsystem<UTALocalizeSubsystem>())
		{
			Localize->OnLanguageChanged.RemoveDynamic(this, &UTAPauseMenuWidget::HandleLanguageChanged);
		}
	}
	Super::NativeDestruct();
}

void UTAPauseMenuWidget::RemoveFromParent()
{
	const FTAInputRouter::FHandle Handle = InputRequestHandle;
	InputRequestHandle = 0;
	AThe_AwakeningPlayerController* PC = InputRequestController.Get();
	InputRequestController.Reset();
	if (Handle && PC)
	{
		PC->ReleaseInputRequest(Handle);
	}
	Super::RemoveFromParent();
}

void UTAPauseMenuWidget::SetPlayerInputRequest(AThe_AwakeningPlayerController* Controller, FTAInputRouter::FHandle Handle)
{
	InputRequestController = Controller;
	InputRequestHandle = Handle;
}

void UTAPauseMenuWidget::BuildOptions()
{
	if (!Box_Options)
	{
		UE_LOG(LogThe_Awakening, Error, TEXT("Pause menu requires a UVerticalBox named Box_Options."));
		return;
	}
	if (!OptionWidgetClass)
	{
		UE_LOG(LogThe_Awakening, Error, TEXT("Pause menu OptionWidgetClass is not configured."));
		return;
	}

	Box_Options->ClearChildren();
	ActionsByOptionId.Reset();
	OptionWidgetsById.Reset();
	SelectedOptionIndex = 0;

	for (const FTAPauseMenuOption& Option : Options)
	{
		if (Option.OptionId.IsNone() || ActionsByOptionId.Contains(Option.OptionId))
		{
			UE_LOG(LogThe_Awakening, Warning, TEXT("Pause menu contains an empty or duplicate option id; skipping it."));
			continue;
		}

		UTAPauseMenuOptionWidget* OptionWidget = CreateWidget<UTAPauseMenuOptionWidget>(this, OptionWidgetClass);
		if (!OptionWidget)
		{
			UE_LOG(LogThe_Awakening, Error, TEXT("Failed to create pause menu option widget for %s."), *Option.OptionId.ToString());
			continue;
		}

		OptionWidget->ConfigureOption(Option.OptionId, GetOptionLabel(Option));
		OptionWidget->OnHovered.AddUniqueDynamic(this, &UTAPauseMenuWidget::HandleOptionHovered);
		OptionWidget->OnOptionSelected.AddUniqueDynamic(this, &UTAPauseMenuWidget::HandleOptionSelected);
		Box_Options->AddChildToVerticalBox(OptionWidget);
		ActionsByOptionId.Add(Option.OptionId, Option.Action);
		OptionWidgetsById.Add(Option.OptionId, OptionWidget);
	}
	ApplyLanguageLayout(UsesEnglishLayout());
	RefreshOptionHighlights();
}

bool UTAPauseMenuWidget::UsesEnglishLayout() const
{
	if (auto* GI=GetGameInstance())
		if (auto* Loc=GI->GetSubsystem<UTALocalizeSubsystem>()) return Loc->GetCurrentLanguage()==TEXT("en");
	return false;
}
void UTAPauseMenuWidget::ApplyLanguageLayout(bool bEnglish)
{
	if (!Box_Options) return;
	auto* OptionsSlot=Cast<UCanvasPanelSlot>(Box_Options->Slot);
	if (!OptionsSlot) return;
	if (!AuthoredOptionsOffsets.IsSet()) AuthoredOptionsOffsets=OptionsSlot->GetOffsets();
	for (const auto& Entry:OptionWidgetsById)
		if (Entry.Value) Entry.Value->ApplyEnglishLayout(bEnglish);
	FMargin OptionsOffsets=AuthoredOptionsOffsets.GetValue();
	if (bEnglish)
	{
		// The VBox's desired width is the widest current label plus button padding.
		// Its Fill child slots give every option the same width; no label-length rules.
		Box_Options->TakeWidget()->SlatePrepass();
		OptionsOffsets.Right=FMath::Max(OptionsOffsets.Right,Box_Options->GetDesiredSize().X);
	}
	OptionsSlot->SetOffsets(OptionsOffsets);
}
FText UTAPauseMenuWidget::GetOptionLabel(const FTAPauseMenuOption& Option) const
{
	if (!Option.LocalizationId.IsEmpty())
	{
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (const UTALocalizeSubsystem* Localize = GameInstance->GetSubsystem<UTALocalizeSubsystem>())
			{
				return Localize->GetText(Option.LocalizationId);
			}
		}
	}
	return Option.Label;
}

void UTAPauseMenuWidget::RefreshOptionLabels()
{
	for (const FTAPauseMenuOption& Option : Options)
	{
		if (TObjectPtr<UTAPauseMenuOptionWidget>* Widget = OptionWidgetsById.Find(Option.OptionId); Widget && Widget->Get())
		{
			Widget->Get()->ConfigureOption(Option.OptionId, GetOptionLabel(Option));
		}
	}
	ApplyLanguageLayout(UsesEnglishLayout());
}

UWidget* UTAPauseMenuWidget::GetInitialFocusTarget() const
{
	if (Box_Options && Box_Options->GetChildrenCount() > 0)
	{
		if (const UTAPauseMenuOptionWidget* FirstOption = Cast<UTAPauseMenuOptionWidget>(Box_Options->GetChildAt(0)))
		{
			return FirstOption->GetFocusTarget();
		}
	}
	return nullptr;
}

void UTAPauseMenuWidget::HandleLanguageChanged()
{
	RefreshOptionLabels();
}

void UTAPauseMenuWidget::HandleOptionSelected(FName OptionId)
{
	SelectOptionById(OptionId);

	const ETAPauseMenuAction* Action = ActionsByOptionId.Find(OptionId);
	if (!Action)
	{
		return;
	}

	switch (*Action)
	{
	case ETAPauseMenuAction::Resume:
		RequestResume();
		break;
	case ETAPauseMenuAction::OpenSettings:
		if (OnSettingsRequested.IsBound())
		{
			OnSettingsRequested.Broadcast();
		}
		else
		{
			UE_LOG(LogThe_Awakening, Warning, TEXT("Pause menu Settings was selected, but OnSettingsRequested has no listener yet."));
		}
		break;
	case ETAPauseMenuAction::QuitGame:
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		break;
	default:
		break;
	}
}

void UTAPauseMenuWidget::HandleOptionHovered(UTASelectableMenuOptionWidget* OptionWidget)
{
	if (const UTAPauseMenuOptionWidget* PauseOption = Cast<UTAPauseMenuOptionWidget>(OptionWidget))
	{
		SelectOptionById(PauseOption->GetOptionId());
	}
}

void UTAPauseMenuWidget::SelectOptionById(FName OptionId)
{
	int32 VisibleIndex = 0;
	bool bFound = false;
	for (const FTAPauseMenuOption& Option : Options)
	{
		if (ActionsByOptionId.Contains(Option.OptionId))
		{
			if (Option.OptionId == OptionId)
			{
				SelectedOptionIndex = VisibleIndex;
				bFound = true;
				break;
			}
			++VisibleIndex;
		}
	}
	if (bFound)
	{
		RefreshOptionHighlights();
	}
}

void UTAPauseMenuWidget::ValidateInputActions() const
{
	if (!PreviousOptionAction || !NextOptionAction || !ConfirmOptionAction)
	{
		UE_LOG(LogThe_Awakening, Error,
			TEXT("Pause menu input actions are not configured. Assign previous, next, and confirm actions from IMC_UI in the WBP_PauseMenu class defaults."));
		return;
	}
	if (!PreviousOptionAction->bTriggerWhenPaused || !NextOptionAction->bTriggerWhenPaused || !ConfirmOptionAction->bTriggerWhenPaused)
	{
		UE_LOG(LogThe_Awakening, Error,
			TEXT("Pause menu navigation actions must have Trigger When Paused enabled in their Input Action assets."));
	}
}

void UTAPauseMenuWidget::BuildActionPromptBar()
{
	if (!HorizontalBox_Controls || ActionPromptWidgets.Num() > 0)
	{
		return;
	}
	auto AddPrompt = [this](UInputAction* Action, const TCHAR* TextId)
	{
		if (!Action)
		{
			return;
		}
		UTAActionPromptWidget* Prompt = FTAPromptWidgetUtils::AddActionPrompt(
			this, HorizontalBox_Controls, ActionPromptWidgetClass, Action, TextId);
		if (!Prompt)
		{
			return;
		}
		Prompt->OnPromptClicked.AddDynamic(this, &UTAPauseMenuWidget::HandleActionPromptClicked);
		ActionPromptWidgets.Add(Prompt);
	};
	AddPrompt(PreviousOptionAction, TEXT("UI_Pause_ChoiceUp"));
	AddPrompt(NextOptionAction, TEXT("UI_Pause_ChoiceDown"));
	AddPrompt(ConfirmOptionAction, TEXT("UI_Pause_ChoiceSelect"));
	if (AThe_AwakeningPlayerController* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		AddPrompt(PC->GetUIBackAction(), TEXT("UI_Pause_CloseMenu"));
	}
}

void UTAPauseMenuWidget::RefreshOptionHighlights()
{
	int32 VisibleIndex = 0;
	for (const FTAPauseMenuOption& Option : Options)
	{
		if (TObjectPtr<UTAPauseMenuOptionWidget>* Widget = OptionWidgetsById.Find(Option.OptionId); Widget && Widget->Get())
		{
			Widget->Get()->SetHighlighted(VisibleIndex == SelectedOptionIndex);
			++VisibleIndex;
		}
	}
}

void UTAPauseMenuWidget::MoveSelection(int32 Delta)
{
	if (ActionsByOptionId.IsEmpty())
	{
		return;
	}
	const int32 Count = ActionsByOptionId.Num();
	SelectedOptionIndex = (SelectedOptionIndex + Delta + Count) % Count;
	RefreshOptionHighlights();
}

void UTAPauseMenuWidget::ConfirmSelection()
{
	int32 VisibleIndex = 0;
	for (const FTAPauseMenuOption& Option : Options)
	{
		if (ActionsByOptionId.Contains(Option.OptionId))
		{
			if (VisibleIndex == SelectedOptionIndex)
			{
				HandleOptionSelected(Option.OptionId);
				return;
			}
			++VisibleIndex;
		}
	}
}

void UTAPauseMenuWidget::HandleActionPromptClicked(UTAActionPromptWidget* Prompt)
{
	if (!Prompt)
	{
		return;
	}
	if (Prompt->GetPromptAction() == PreviousOptionAction)
	{
		MoveSelection(-1);
	}
	else if (Prompt->GetPromptAction() == NextOptionAction)
	{
		MoveSelection(1);
	}
	else if (Prompt->GetPromptAction() == ConfirmOptionAction)
	{
		ConfirmSelection();
	}
	else if (AThe_AwakeningPlayerController* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer());
		PC && Prompt->GetPromptAction() == PC->GetUIBackAction())
	{
		PC->ClosePauseMenu();
	}
}

TOptional<ETAInputCapability> UTAPauseMenuWidget::ResolvePlayerInput(FKey Key) const
{
	if (Key.IsMouseButton())
	{
		return {};
	}
	const AThe_AwakeningPlayerController* PC = InputRequestController.Get();
	if (!PC)
	{
		return {};
	}
	if (PC->IsKeyMappedToAction(Key, ConfirmOptionAction))
	{
		return ETAInputCapability::Confirm;
	}
	if (PC->IsKeyMappedToAction(Key, PreviousOptionAction) || PC->IsKeyMappedToAction(Key, NextOptionAction))
	{
		return ETAInputCapability::Navigate;
	}
	return {};
}

void UTAPauseMenuWidget::ExecutePlayerInput(FKey Key, ETAInputCapability Capability)
{
	const AThe_AwakeningPlayerController* PC = InputRequestController.Get();
	if (!PC || !PC->AllowsInputFor(InputRequestHandle, this, Capability))
	{
		return;
	}
	if (Capability == ETAInputCapability::Confirm)
	{
		ConfirmSelection();
	}
	else if (Capability == ETAInputCapability::Navigate)
	{
		MoveSelection(PC->IsKeyMappedToAction(Key, PreviousOptionAction) ? -1 : 1);
	}
}

FReply UTAPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && Box_Options)
	{
		const FVector2D Position = Event.GetScreenSpacePosition();
		int32 Index = 0;
		for (const FTAPauseMenuOption& Option : Options)
		{
			if (TObjectPtr<UTAPauseMenuOptionWidget>* Widget = OptionWidgetsById.Find(Option.OptionId); Widget && Widget->Get())
			{
				if (Widget->Get()->GetCachedGeometry().IsUnderLocation(Position))
				{
					SelectedOptionIndex = Index;
					RefreshOptionHighlights();
					break;
				}
				++Index;
			}
		}
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void UTAPauseMenuWidget::RequestResume()
{
	if (AThe_AwakeningPlayerController* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		PC->ClosePauseMenu();
	}
}

bool UTAPauseMenuWidget::HandleMenuBackRequested()
{
	if (AThe_AwakeningPlayerController* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		PC->ClosePauseMenu();
		return true;
	}
	return false;
}
