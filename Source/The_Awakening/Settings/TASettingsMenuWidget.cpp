#include "Settings/TASettingsMenuWidget.h"

#include "Core/TALocalizeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "The_AwakeningPlayerController.h"
#include "UI/Pause/TAPauseMenuOptionWidget.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TASelectableMenuOptionWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "Settings/TASettingsSubsystem.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const ETAGameSettingsPage AllPages[] = {
		ETAGameSettingsPage::Game, ETAGameSettingsPage::Display, ETAGameSettingsPage::Audio,
		ETAGameSettingsPage::MouseKeyboard, ETAGameSettingsPage::Controller, ETAGameSettingsPage::Favorites
	};

	FName PageId(ETAGameSettingsPage Page)
	{
		static const FName Ids[] = {TEXT("Page.Game"), TEXT("Page.Display"), TEXT("Page.Audio"),
			TEXT("Page.MouseKeyboard"), TEXT("Page.Controller"), TEXT("Page.Favorites")};
		return Ids[static_cast<uint8>(Page)];
	}

	bool IsHorizontalNavigation(FKey Key)
	{
		return Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Left ||
			Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftX;
	}
}

UTASettingsMenuWidget::UTASettingsMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UInputAction> PreviousAsset(TEXT("/Game/Input/Actions/IA_ChoicePrevious.IA_ChoicePrevious"));
	static ConstructorHelpers::FObjectFinder<UInputAction> NextAsset(TEXT("/Game/Input/Actions/IA_ChoiceNext.IA_ChoiceNext"));
	static ConstructorHelpers::FObjectFinder<UInputAction> ConfirmAsset(TEXT("/Game/Input/Actions/IA_ChoiceConfirm.IA_ChoiceConfirm"));
	if (PreviousAsset.Succeeded()) PreviousAction = PreviousAsset.Object;
	if (NextAsset.Succeeded()) NextAction = NextAsset.Object;
	if (ConfirmAsset.Succeeded()) ConfirmAction = ConfirmAsset.Object;
}

void UTASettingsMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		SettingsSubsystem = LocalPlayer->GetSubsystem<UTASettingsSubsystem>();
	}
	if (SettingsSubsystem)
	{
		SettingsSubsystem->OnSettingValueChanged.AddUniqueDynamic(this, &UTASettingsMenuWidget::HandleSettingValueChanged);
		if (DefinitionAsset) SettingsSubsystem->UseDefinitionAsset(DefinitionAsset);
	}
	BuildPageButtons();
	BuildSettingRows();
	BuildPromptBar();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Localize = GameInstance->GetSubsystem<UTALocalizeSubsystem>())
		{
			Localize->OnLanguageChanged.AddUniqueDynamic(this, &UTASettingsMenuWidget::HandleLanguageChanged);
		}
	}
}

void UTASettingsMenuWidget::NativeDestruct()
{
	if (SettingsSubsystem) SettingsSubsystem->OnSettingValueChanged.RemoveDynamic(this, &UTASettingsMenuWidget::HandleSettingValueChanged);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Localize = GameInstance->GetSubsystem<UTALocalizeSubsystem>())
		{
			Localize->OnLanguageChanged.RemoveDynamic(this, &UTASettingsMenuWidget::HandleLanguageChanged);
		}
	}
	Super::NativeDestruct();
}

void UTASettingsMenuWidget::InitializeMenu(AThe_AwakeningPlayerController* InController,
	UTASettingsMenuWidget* InParent, ETASettingSubmenuTarget InSubmenuTarget)
{
	InputController = InController;
	ParentMenu = InParent;
	SubmenuTarget = InSubmenuTarget;
	if (DefinitionAsset && SettingsSubsystem) SettingsSubsystem->UseDefinitionAsset(DefinitionAsset);
	if (IsConstructed())
	{
		if (Box_Pages) Box_Pages->SetVisibility(InParent ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		BuildSettingRows();
	}
}

void UTASettingsMenuWidget::SetPlayerInputRequest(AThe_AwakeningPlayerController* InController, FTAInputRouter::FHandle Handle)
{
	InputController = InController;
	InputRequestHandle = Handle;
}

void UTASettingsMenuWidget::RemoveFromParent()
{
	if (ActiveSubmenu)
	{
		UTASettingsMenuWidget* Child = ActiveSubmenu;
		ActiveSubmenu = nullptr;
		Child->RemoveFromParent();
	}
	const auto Handle = InputRequestHandle;
	InputRequestHandle = 0;
	if (Handle)
	{
		if (AThe_AwakeningPlayerController* PC = InputController.Get()) PC->ReleaseInputRequest(Handle);
	}
	Super::RemoveFromParent();
}

UWidget* UTASettingsMenuWidget::GetInitialFocusTarget() const
{
	if (const TObjectPtr<UTAPauseMenuOptionWidget>* Widget = SettingWidgets.Find(SelectedSettingId); Widget && Widget->Get())
	{
		return Widget->Get()->GetFocusTarget();
	}
	if (Box_SettingsOptions && Box_SettingsOptions->GetChildrenCount() > 0)
	{
		if (const UTAPauseMenuOptionWidget* Widget = Cast<UTAPauseMenuOptionWidget>(Box_SettingsOptions->GetChildAt(0)))
			return Widget->GetFocusTarget();
	}
	return nullptr;
}

FText UTASettingsMenuWidget::ResolveText(const FString& TextId) const
{
	if (TextId.IsEmpty()) return FText::GetEmpty();
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UTALocalizeSubsystem* Localize = GameInstance->GetSubsystem<UTALocalizeSubsystem>())
			return Localize->GetText(TextId);
	}
	return FText::FromString(TextId);
}

FText UTASettingsMenuWidget::GetPageLabel(ETAGameSettingsPage Page) const
{
	static const TCHAR* TextIds[] = {TEXT("Settings.Page.Game"), TEXT("Settings.Page.Display"), TEXT("Settings.Page.Audio"),
		TEXT("Settings.Page.MouseKeyboard"), TEXT("Settings.Page.Controller"), TEXT("Settings.Page.Favorites")};
	return ResolveText(TextIds[static_cast<uint8>(Page)]);
}

FText UTASettingsMenuWidget::GetSettingRowLabel(const FTASettingDefinition& Definition) const
{
	FText Label = ResolveText(Definition.NameTextId);
	if (!SettingsSubsystem || Definition.Type == ETASettingType::Submenu) return Label;
	FTASettingValue Value;
	if (!SettingsSubsystem->GetValue(Definition.SettingId, Value)) return Label;

	FText ValueText;
	if (Definition.Type == ETASettingType::Toggle)
	{
		ValueText = ResolveText(Value.bBoolean ? TEXT("Settings.Value.On") : TEXT("Settings.Value.Off"));
	}
	else if (Definition.Type == ETASettingType::Choice)
	{
		if (const FTASettingChoice* Choice = Definition.Choices.FindByPredicate([&Value](const FTASettingChoice& Candidate)
			{ return Candidate.Value == Value.Choice; })) ValueText = ResolveText(Choice->NameTextId);
	}
	else if (Definition.Type == ETASettingType::Slider)
	{
		if (Definition.DisplayFormat == TEXT("Integer")) ValueText = FText::AsNumber(FMath::RoundToInt(Value.Number));
		else if (Definition.DisplayFormat == TEXT("Gamma")) ValueText = FText::AsNumber(Value.Number);
		else if (Definition.DisplayFormat == TEXT("Percent")) ValueText = FText::AsPercent(Value.Number);
		else ValueText = FText::AsNumber(Value.Number);
	}
	return ValueText.IsEmpty() ? Label : FText::Format(FText::FromString(TEXT("{0}: {1}")), Label, ValueText);
}

void UTASettingsMenuWidget::BuildPageButtons()
{
	if (!Box_Pages) return;
	Box_Pages->ClearChildren();
	PageWidgets.Reset();
	if (ParentMenu.IsValid() || SubmenuTarget != ETASettingSubmenuTarget::None)
	{
		Box_Pages->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Box_Pages->SetVisibility(ESlateVisibility::Visible);
	if (!OptionWidgetClass) return;
	for (const ETAGameSettingsPage Page : AllPages)
	{
		UTAPauseMenuOptionWidget* Widget = CreateWidget<UTAPauseMenuOptionWidget>(this, OptionWidgetClass);
		if (!Widget) continue;
		const FName Id = PageId(Page);
		Widget->ConfigureOption(Id, GetPageLabel(Page));
		Widget->OnHovered.AddUniqueDynamic(this, &UTASettingsMenuWidget::HandleOptionHovered);
		Widget->OnOptionSelected.AddUniqueDynamic(this, &UTASettingsMenuWidget::HandleOptionSelected);
		Box_Pages->AddChildToVerticalBox(Widget);
		PageWidgets.Add(Id, Widget);
	}
	for (uint8 Index = 0; Index < UE_ARRAY_COUNT(AllPages); ++Index)
	{
		if (AllPages[Index] == CurrentPage) SelectedPageIndex = Index;
	}
}

void UTASettingsMenuWidget::BuildSettingRows()
{
	if (!Box_SettingsOptions) return;
	Box_SettingsOptions->ClearChildren();
	SettingWidgets.Reset();
	SelectedSettingIndex = 0;
	if (SettingsSubsystem && OptionWidgetClass)
	{
		const TArray<FTASettingDefinition>& Definitions = SettingsSubsystem->GetDefinitions();
		for (const FTASettingDefinition& Definition : Definitions)
		{
			const bool bShow = SubmenuTarget == ETASettingSubmenuTarget::Brightness
				? Definition.SettingId == TEXT("Display.Brightness")
				: (SubmenuTarget == ETASettingSubmenuTarget::KeyBindings
					? Definition.Page == ETAGameSettingsPage::MouseKeyboard && !Definition.bSubmenuOnly
					: !Definition.bSubmenuOnly && (CurrentPage == ETAGameSettingsPage::Favorites
						? SettingsSubsystem->IsFavorite(Definition.SettingId) : Definition.Page == CurrentPage));
			if (!bShow) continue;
			UTAPauseMenuOptionWidget* Widget = CreateWidget<UTAPauseMenuOptionWidget>(this, OptionWidgetClass);
			if (!Widget) continue;
			Widget->ConfigureOption(Definition.SettingId, GetSettingRowLabel(Definition));
			Widget->OnHovered.AddUniqueDynamic(this, &UTASettingsMenuWidget::HandleOptionHovered);
			Widget->OnOptionSelected.AddUniqueDynamic(this, &UTASettingsMenuWidget::HandleOptionSelected);
			Box_SettingsOptions->AddChildToVerticalBox(Widget);
			SettingWidgets.Add(Definition.SettingId, Widget);
		}
	}
	if (SettingWidgets.Num() > 0) SelectedSettingId = SettingWidgets.begin()->Key;
	if (Text_Description)
	{
		const FTASettingDefinition* SelectedDefinition = SettingsSubsystem
			? SettingsSubsystem->GetDefinitions().FindByPredicate([this](const FTASettingDefinition& D) { return D.SettingId == SelectedSettingId; })
			: nullptr;
		Text_Description->SetText(SelectedDefinition ? ResolveText(SelectedDefinition->DescriptionTextId) : FText::GetEmpty());
	}
	RefreshRows();
}

void UTASettingsMenuWidget::BuildPromptBar()
{
	if (!HorizontalBox_Controls || ActionPromptWidgets.Num() > 0) return;
	const auto AddPrompt = [this](UInputAction* Action, const TCHAR* TextId)
	{
		if (!Action) return;
		if (UTAActionPromptWidget* Prompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls,
			ActionPromptWidgetClass, Action, TextId))
		{
			Prompt->OnPromptClicked.AddDynamic(this, &UTASettingsMenuWidget::HandleActionPromptClicked);
			ActionPromptWidgets.Add(Prompt);
		}
	};
	AddPrompt(PreviousAction, TEXT("UI_Settings_Previous"));
	AddPrompt(NextAction, TEXT("UI_Settings_Next"));
	AddPrompt(ConfirmAction, TEXT("UI_Settings_Confirm"));
	if (AThe_AwakeningPlayerController* PC = InputController.Get()) AddPrompt(PC->GetUIBackAction(), TEXT("UI_Settings_Back"));
}

void UTASettingsMenuWidget::RefreshRows()
{
	int32 Index = 0;
	for (const TPair<FName, TObjectPtr<UTAPauseMenuOptionWidget>>& Pair : SettingWidgets)
	{
		const FTASettingDefinition* Definition = SettingsSubsystem ? SettingsSubsystem->GetDefinitions().FindByPredicate(
			[&Pair](const FTASettingDefinition& D) { return D.SettingId == Pair.Key; }) : nullptr;
		if (Pair.Value && Definition) Pair.Value->ConfigureOption(Pair.Key, GetSettingRowLabel(*Definition));
		if (Pair.Value) Pair.Value->SetHighlighted(Pair.Key == SelectedSettingId);
		++Index;
	}
	for (const TPair<FName, TObjectPtr<UTAPauseMenuOptionWidget>>& Pair : PageWidgets)
		if (Pair.Value) Pair.Value->SetHighlighted(Pair.Key == PageId(CurrentPage));
	if (Text_Description && SettingsSubsystem)
	{
		if (const FTASettingDefinition* Definition = SettingsSubsystem->GetDefinitions().FindByPredicate(
			[this](const FTASettingDefinition& D) { return D.SettingId == SelectedSettingId; }))
			Text_Description->SetText(ResolveText(Definition->DescriptionTextId));
	}
}

void UTASettingsMenuWidget::SelectPage(ETAGameSettingsPage Page)
{
	if (SubmenuTarget != ETASettingSubmenuTarget::None) return;
	CurrentPage = Page;
	BuildSettingRows();
	RefreshRows();
}

void UTASettingsMenuWidget::MoveSelection(int32 Direction)
{
	if (SettingWidgets.IsEmpty()) return;
	TArray<FName> Ids;
	for (const auto& Pair : SettingWidgets) Ids.Add(Pair.Key);
	Ids.Sort([](FName A, FName B) { return A.ToString() < B.ToString(); });
	int32 Index = Ids.IndexOfByKey(SelectedSettingId);
	Index = Index < 0 ? 0 : (Index + Direction + Ids.Num()) % Ids.Num();
	SelectedSettingId = Ids[Index];
	RefreshRows();
}

void UTASettingsMenuWidget::AdjustSelectedValue(int32 Direction)
{
	if (!SettingsSubsystem) return;
	const FTASettingDefinition* Definition = SettingsSubsystem->GetDefinitions().FindByPredicate(
		[this](const FTASettingDefinition& D) { return D.SettingId == SelectedSettingId; });
	if (!Definition) return;
	if (Definition->Type == ETASettingType::Submenu) OpenSubmenu(*Definition);
	else SettingsSubsystem->AdjustValue(SelectedSettingId, Direction);
	RefreshRows();
}

void UTASettingsMenuWidget::ConfirmSelection()
{
	if (!SettingsSubsystem) return;
	for (uint8 Index = 0; Index < UE_ARRAY_COUNT(AllPages); ++Index)
		if (PageId(AllPages[Index]) == SelectedSettingId) { SelectPage(AllPages[Index]); return; }
	const FTASettingDefinition* Definition = SettingsSubsystem->GetDefinitions().FindByPredicate(
		[this](const FTASettingDefinition& D) { return D.SettingId == SelectedSettingId; });
	if (!Definition) return;
	if (Definition->Type == ETASettingType::Submenu) OpenSubmenu(*Definition);
	else if (Definition->Type == ETASettingType::Toggle) SettingsSubsystem->AdjustValue(SelectedSettingId, 1);
	else if (Definition->Type == ETASettingType::Choice) SettingsSubsystem->AdjustValue(SelectedSettingId, 1);
	RefreshRows();
}

void UTASettingsMenuWidget::OpenSubmenu(const FTASettingDefinition& Definition)
{
	if (Definition.SubmenuTarget == ETASettingSubmenuTarget::None) return;
	AThe_AwakeningPlayerController* PC = InputController.Get();
	if (!PC) return;
	UTASettingsMenuWidget* Child = CreateWidget<UTASettingsMenuWidget>(PC, GetClass());
	if (!Child) return;
	Child->InitializeMenu(PC, this, Definition.SubmenuTarget);
	Child->AddToViewport(1100);
	FTAInputRequest Request;
	Request.Owner = Child;
	Request.Priority = 610;
	Request.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Cursor, ETAInputCapability::Confirm, ETAInputCapability::Close};
	Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
	Request.Presentation.bShowCursor = true;
	Request.Presentation.Focus = ETAInputFocusRequirement::Target;
	Request.Presentation.FocusTarget = Child->GetInitialFocusTarget();
	const auto Handle = PC->AcquireInputRequest(Request);
	if (!Handle)
	{
		Child->RemoveFromParent();
		return;
	}
	Child->SetPlayerInputRequest(PC, Handle);
	ActiveSubmenu = Child;
}

void UTASettingsMenuWidget::CloseSubmenu()
{
	if (UTASettingsMenuWidget* Parent = ParentMenu.Get())
	{
		Parent->ActiveSubmenu = nullptr;
		RemoveFromParent();
	}
}

void UTASettingsMenuWidget::HandleOptionHovered(UTASelectableMenuOptionWidget* OptionWidget)
{
	const UTAPauseMenuOptionWidget* Option = Cast<UTAPauseMenuOptionWidget>(OptionWidget);
	if (!Option) return;
	const FName Id = Option->GetOptionId();
	for (uint8 Index = 0; Index < UE_ARRAY_COUNT(AllPages); ++Index)
	{
		if (PageId(AllPages[Index]) == Id) { SelectedPageIndex = Index; return; }
	}
	SelectedSettingId = Id;
	RefreshRows();
}

void UTASettingsMenuWidget::HandleOptionSelected(FName OptionId)
{
	for (uint8 Index = 0; Index < UE_ARRAY_COUNT(AllPages); ++Index)
	{
		if (PageId(AllPages[Index]) == OptionId) { SelectPage(AllPages[Index]); return; }
	}
	SelectedSettingId = OptionId;
	ConfirmSelection();
}

void UTASettingsMenuWidget::HandleActionPromptClicked(UTAActionPromptWidget* Prompt)
{
	if (!Prompt) return;
	if (Prompt->GetPromptAction() == PreviousAction) MoveSelection(-1);
	else if (Prompt->GetPromptAction() == NextAction) MoveSelection(1);
	else if (Prompt->GetPromptAction() == ConfirmAction) ConfirmSelection();
	else if (AThe_AwakeningPlayerController* PC = InputController.Get(); PC && Prompt->GetPromptAction() == PC->GetUIBackAction()) HandleMenuBackRequested();
}

void UTASettingsMenuWidget::HandleLanguageChanged()
{
	BuildPageButtons();
	BuildSettingRows();
	for (UTAActionPromptWidget* Prompt : ActionPromptWidgets) if (Prompt) Prompt->RefreshPrompt();
}

void UTASettingsMenuWidget::HandleSettingValueChanged(FName SettingId)
{
	if (SettingId == SelectedSettingId || CurrentPage == ETAGameSettingsPage::Favorites) BuildSettingRows();
}

TOptional<ETAInputCapability> UTASettingsMenuWidget::ResolvePlayerInput(FKey Key) const
{
	if (Key.IsMouseButton()) return {};
	AThe_AwakeningPlayerController* PC = InputController.Get();
	if (!PC) return {};
	if (PC->IsKeyMappedToAction(Key, ConfirmAction)) return ETAInputCapability::Confirm;
	if (PC->IsKeyMappedToAction(Key, PreviousAction) || PC->IsKeyMappedToAction(Key, NextAction) || IsHorizontalNavigation(Key))
		return ETAInputCapability::Navigate;
	return {};
}

void UTASettingsMenuWidget::ExecutePlayerInput(FKey Key, ETAInputCapability Capability)
{
	AThe_AwakeningPlayerController* PC = InputController.Get();
	if (!PC || !PC->AllowsInputFor(InputRequestHandle, this, Capability)) return;
	if (Capability == ETAInputCapability::Confirm) ConfirmSelection();
	else if (Capability == ETAInputCapability::Navigate)
	{
		if (IsHorizontalNavigation(Key)) AdjustSelectedValue((Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) ? -1 : 1);
		else MoveSelection(PC->IsKeyMappedToAction(Key, PreviousAction) ? -1 : 1);
	}
}

bool UTASettingsMenuWidget::HandleMenuBackRequested()
{
	if (ActiveSubmenu)
	{
		ActiveSubmenu->HandleMenuBackRequested();
		return true;
	}
	if (ParentMenu.IsValid())
	{
		CloseSubmenu();
		return true;
	}
	if (AThe_AwakeningPlayerController* PC = InputController.Get())
	{
		PC->CloseSettingsMenu();
		return true;
	}
	return false;
}
