#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "The_AwakeningPlayerController.h"
#include "Core/TAPlayerInput.h"
#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "UI/Inventory/TAInventoryTabBar.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "UI/Inventory/TAClothingSwapWidget.h"
#include "UI/Inventory/TAItemActionMenuWidget.h"
#include "UI/TAActionPromptWidget.h"
#include "Inventory/TAClothingRack.h"
#include "EnhancedInputComponent.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAClothingDefinition.h"
#include "Inventory/TAItemDefinition.h"
#include "Story/TADialogueWidget.h"
#include "Story/TADialogueHistoryWidget.h"
#include "Story/TADialogueController.h"
#include "Puzzle/TAPathPuzzleWidget.h"
#include "EnhancedInputSubsystemInterface.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Button.h"
#include "Components/NamedSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Core/TAPlayerState.h"
#include "AbilitySystem/TAAttributeSet.h"

namespace
{
struct FUIIngressMappings : IEnhancedInputSubsystemInterface
{
	UEnhancedPlayerInput* Input = nullptr;
	TMap<TObjectPtr<const UInputAction>, FInjectedInput> Injected;
	virtual UEnhancedPlayerInput* GetPlayerInput() const override { return Input; }
	virtual TMap<TObjectPtr<const UInputAction>, FInjectedInput>& GetContinuouslyInjectedInputs() override { return Injected; }
};

void EvaluateUIKey(AThe_AwakeningPlayerController* PC, FKey Key, EInputEvent Event)
{
	PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event,
		Event == IE_Released ? 0.f : 1.f, false, 0u));
	PC->PlayerInput->ProcessInputStack({PC->InputComponent}, 0.016f, false);
}

// Observe whether input reaches the UMG path at all; production routing remains unchanged.
class SInventoryPreviewProbe : public SBorder
{
public:
	int32 Previews = 0;
	virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override
	{
		++Previews;
		return FReply::Unhandled();
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAUIPlayerIngressTest, "TheAwakening.Input.UIPlayerIngress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAUIPlayerIngressTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UClass* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller"), PC)) { World->DestroyWorld(false); return false; }
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	World->AddController(PC);
	PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
	PC->InputComponent = NewObject<UEnhancedInputComponent>(PC);
	FUIIngressMappings Mappings; Mappings.Input = Cast<UTAPlayerInput>(PC->PlayerInput);
	FModifyContextOptions Options; Options.bForceImmediately = true;
	const int32 User = Local->GetControllerId();
	auto* InventoryClass = LoadClass<UTAInventoryPanelWidget>(nullptr,
		TEXT("/Game/UI/Inventory/WBP_InventoryPanel.WBP_InventoryPanel_C"));
	auto* Inventory = CreateWidget<UTAInventoryPanelWidget>(PC, InventoryClass);
	if (!TestNotNull(TEXT("Actual inventory WBP"), Inventory)) { World->DestroyWorld(false); return false; }
	auto* Close = NewObject<UInputAction>();
	auto* Context = NewObject<UInputMappingContext>(); Context->MapKey(Close, EKeys::Tab);
	Mappings.AddMappingContext(Context, 0, Options);
	auto* Data = NewObject<UTAInventoryComponent>(PC);
	PC->AddInstanceComponent(Data); Data->RegisterComponent();
	auto* Clothing = NewObject<UTAClothingDefinition>();
	FTAPocketDef Pocket; Pocket.SlotCount = 2; Clothing->Pockets.Add(Pocket);
	Data->DefaultInnerClothing = Clothing; Data->BeginPlay();
	auto* Item = NewObject<UTAItemDefinition>(); Data->TryAddItem(Item, 1);
	Inventory->Init(Data, Close); Inventory->NativeConstruct();
	auto* Switcher = Cast<UWidgetSwitcher>(Inventory->GetWidgetFromName(TEXT("WidgetSwitcher")));
	auto* Skills = Cast<UButton>(Inventory->GetWidgetFromName(TEXT("Button_Skills")));
	auto* Tabs = Cast<UTAInventoryTabBar>(Inventory->GetWidgetFromName(TEXT("TabBar_Inventory")));
	TestNotNull(TEXT("Animated inventory header is installed on the real Blueprint"), Tabs);
	TestNull(TEXT("Old money display is removed from the Blueprint"), Inventory->GetWidgetFromName(TEXT("Text_Money")));
	if (Tabs)
	{
		auto* State = World->SpawnActor<ATAPlayerState>(); PC->SetPlayerState(State);
		State->GetAbilitySystemComponent()->AddAttributeSetSubobject(State->GetAttributeSet());
		State->GetAbilitySystemComponent()->InitAbilityActorInfo(State, State);
		State->AddMoney(128);
		State->GetAbilitySystemComponent()->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), 75.f);
		Inventory->RefreshAll();
		TestEqual(TEXT("Header uses the existing currency system"), Tabs->GetCellsLabel().ToString(), FString(TEXT("细胞: 128")));
		TestEqual(TEXT("Header reads the existing GAS health"), Tabs->GetHealthLabel().ToString(), FString(TEXT("HP: 75")));
		State->AddMoney(7);
		State->GetAbilitySystemComponent()->SetNumericAttributeBase(UTAAttributeSet::GetHealthAttribute(), 44.f);
		Inventory->NativeTick(FGeometry(), 0.016f);
		TestEqual(TEXT("Currency changes refresh while inventory is open"), Tabs->GetCellsLabel().ToString(), FString(TEXT("细胞: 135")));
		TestEqual(TEXT("Health changes refresh while inventory is open"), Tabs->GetHealthLabel().ToString(), FString(TEXT("HP: 44")));
	}
	auto* Container = Cast<UNamedSlot>(Inventory->GetWidgetFromName(TEXT("NamedSlot_InnerClothing")));
	auto* ClothingUI = Container ? Cast<UTAClothingPanelWidget>(Container->GetContent()) : nullptr;
	TestNotNull(TEXT("Actual clothing panel"), ClothingUI);
	TestEqual(TEXT("Inventory runtime slots"), Data->GetSlots().Num(), 2);
	// Headless widgets have not been attached to Slate; build the child as the viewport does.
	if (ClothingUI) { ClothingUI->TakeWidget(); Inventory->RefreshAll(); }
	TArray<UTAInventorySlotWidget*> Slots;
	if (ClothingUI && ClothingUI->WidgetTree)
		ClothingUI->WidgetTree->ForEachWidget([&Slots](UWidget* Widget)
		{
			if (auto* Slot = Cast<UTAInventorySlotWidget>(Widget)) Slots.Add(Slot);
		});
	if (!TestNotNull(TEXT("Switcher"), Switcher) || !TestNotNull(TEXT("Skills button"), Skills) ||
		!TestEqual(TEXT("Real pocket slots generated"), Slots.Num(), 2))
	{ Inventory->NativeDestruct(); World->DestroyWorld(false); return false; }
	Slots.Sort([](const auto& A, const auto& B) { return A.GetFlatIndex() < B.GetFlatIndex(); });
	const FGeometry Geometry;
	const FPointerEvent Pointer(0, 0, FVector2D::ZeroVector, FVector2D::ZeroVector,
		TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
	UDragDropOperation* Drag = nullptr;
	Slots[0]->NativeOnDragDetected(Geometry, Pointer, Drag);
	TestNotNull(TEXT("Actual clothing slot receives its owner and can start drag"), Drag);
	FTAInputRequest Cover; Cover.Owner = PC; Cover.Priority = 400;
	Cover.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Confirm, ETAInputCapability::Close,
		ETAInputCapability::ToggleDrag, ETAInputCapability::InventoryToggle, ETAInputCapability::Advance};
	const auto CoverHandle = PC->AcquireInputRequest(Cover);
	Skills->OnClicked.Broadcast();
	TestEqual(TEXT("Covered button cannot use new owner's Navigate"), Switcher->GetActiveWidgetIndex(), 0);
	if (Tabs)
	{
		Tabs->OnTabRequested.Broadcast(1);
		TestEqual(TEXT("Covered animated header cannot change pages"), Switcher->GetActiveWidgetIndex(), 0);
		TestEqual(TEXT("Covered header retains selection"), Tabs->GetSelectedPage(), 0);
	}
	const FDragDropEvent Drop(Pointer, TSharedPtr<FDragDropOperation>());
	TestFalse(TEXT("Covered Drop cannot mutate inventory"), Slots[1]->NativeOnDrop(Geometry, Drop, Drag));
	TestTrue(TEXT("Denied Drop preserves item location"), !Data->GetSlots()[0].IsEmpty() && Data->GetSlots()[1].IsEmpty());
	UDragDropOperation* DeniedDrag = nullptr;
	Slots[0]->NativeOnDragDetected(Geometry, Pointer, DeniedDrag);
	TestNull(TEXT("Covered drag cannot start"), DeniedDrag);
	Slots[0]->NativeOnDragCancelled(Drop, Drag); // cleanup remains callable while covered
	Inventory->ExecutePlayerAction(Close, ETriggerEvent::Started, FInputActionInstance(Close));
	TestTrue(TEXT("Covered close retains original request"), Inventory->GetPlayerInputRequestHandle() != 0);
	PC->ReleaseInputRequest(CoverHandle);
	TestTrue(TEXT("Drop works again after remaining winner restores"), Slots[1]->NativeOnDrop(Geometry, Drop, Drag));
	TestTrue(TEXT("Authorized Drop moves the stack once"), Data->GetSlots()[0].IsEmpty() && Data->GetSlots()[1].Count == 1);
	Skills->OnClicked.Broadcast(); TestEqual(TEXT("Authorized button changes page"), Switcher->GetActiveWidgetIndex(), 1);
	if (Tabs)
	{
		TestEqual(TEXT("Existing button keeps animated header synchronized"), Tabs->GetSelectedPage(), 1);
		Tabs->OnTabRequested.Broadcast(0);
		TestEqual(TEXT("Animated header changes the actual content page"), Switcher->GetActiveWidgetIndex(), 0);
		TestEqual(TEXT("Animated header restores inventory selection"), Tabs->GetSelectedPage(), 0);
		Tabs->OnTabRequested.Broadcast(99);
		TestEqual(TEXT("Invalid header page is ignored"), Switcher->GetActiveWidgetIndex(), 0);
	}
	// Use the actual WBP actions, remapped physical keys and gamepad buttons through an IMC.
	auto GetPageAction = [Inventory](FName Name) -> UInputAction*
	{
		const auto* Property = FindFProperty<FObjectPropertyBase>(Inventory->GetClass(), Name);
		return Property ? Cast<UInputAction>(Property->GetObjectPropertyValue_InContainer(Inventory)) : nullptr;
	};
	auto* PreviousPage = GetPageAction(TEXT("PreviousPageAction"));
	auto* NextPage = GetPageAction(TEXT("NextPageAction"));
	if (TestNotNull(TEXT("Configured previous-page action"), PreviousPage) && TestNotNull(TEXT("Configured next-page action"), NextPage))
	{
		Context->MapKey(PreviousPage, EKeys::U); Context->MapKey(NextPage, EKeys::I);
		Context->MapKey(PreviousPage, EKeys::Gamepad_LeftShoulder); Context->MapKey(NextPage, EKeys::Gamepad_RightShoulder);
		Mappings.RemoveMappingContext(Context, Options); Mappings.AddMappingContext(Context, 0, Options);
		auto PressPage = [&](FKey Key, int32 ExpectedPage)
		{
			TestFalse(TEXT("Raw key routing cannot execute inventory actions"), PC->RoutePlayerInputKey(Key, false, User));
			EvaluateUIKey(PC, Key, IE_Pressed); EvaluateUIKey(PC, Key, IE_Released);
			TestEqual(TEXT("Paging stops at the boundary and moves normally in range"), Switcher->GetActiveWidgetIndex(), ExpectedPage);
			if (Tabs) TestEqual(TEXT("Paging keeps the animated header synchronized"), Tabs->GetSelectedPage(), ExpectedPage);
		};
		for (bool bGamepad : {false, true})
		{
			const FKey PreviousKey = bGamepad ? EKeys::Gamepad_LeftShoulder : EKeys::U;
			const FKey NextKey = bGamepad ? EKeys::Gamepad_RightShoulder : EKeys::I;
			PressPage(PreviousKey, 0); // no wrap from inventory to skills
			PressPage(NextKey, 1);
			PressPage(NextKey, 1); // no wrap from skills to inventory
			PressPage(PreviousKey, 0);
		}
	}
	PC->RecordHeldInput(EKeys::Tab, 1.f, User);
	EvaluateUIKey(PC, EKeys::Tab, IE_Pressed); EvaluateUIKey(PC, EKeys::Tab, IE_Released);
	TestEqual(TEXT("Close releases the exact owner"), Inventory->GetPlayerInputRequestHandle(), uint64(0));
	Inventory->NativeDestruct(); PC->RecordHeldInput(EKeys::Tab, 0.f, User);

	FTAStoryData Story; Story.Entry = TEXT("choice");
	FTAStoryNode Choice; Choice.Id = Story.Entry; Choice.Type = TEXT("choice");
	FTAStoryChoice Option; Option.Target = TEXT("line"); Choice.Choices.Add(Option);
	FTAStoryNode Line; Line.Id = TEXT("line"); Line.Type = TEXT("dialogue"); Line.TextId = TEXT("test-line");
	Story.Nodes = {Choice, Line}; Story.RebuildIndex();
	auto* Session = NewObject<UTADialogueController>(); Session->Initialize(nullptr, Story, PC); Session->Start();
	auto* Dialogue = CreateWidget<UTADialogueWidget>(PC, UTADialogueWidget::StaticClass());
 // The native fixture has no WBP defaults; configure shared choice actions
 // before validation and before adding the test's physical-key mappings.
 Dialogue->ChoicePreviousAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/Input/Actions/IA_ChoicePrevious"));
 Dialogue->ChoiceNextAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/Input/Actions/IA_ChoiceNext"));
 Dialogue->ChoiceConfirmAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/Input/Actions/IA_ChoiceConfirm"));
	Dialogue->Setup(nullptr, Session); Dialogue->NativeConstruct();
	Context->MapKey(Dialogue->ChoiceConfirmAction, EKeys::Gamepad_FaceButton_Bottom);
	Dialogue->AdvanceAction = NewObject<UInputAction>(); Context->MapKey(Dialogue->AdvanceAction, EKeys::Gamepad_FaceButton_Bottom);
	Context->MapKey(Dialogue->AdvanceAction, EKeys::LeftMouseButton);
	Dialogue->HistoryAction = NewObject<UInputAction>(); Context->MapKey(Dialogue->HistoryAction, EKeys::H);
	Mappings.RemoveMappingContext(Context, Options); Mappings.AddMappingContext(Context, 0, Options);
	const auto DialogueCover = PC->AcquireInputRequest(Cover);
	Dialogue->OnChoiceClicked(0);
	TestTrue(TEXT("Covered UMG choice cannot execute"), Session->IsChoiceNode());
	PC->ReleaseInputRequest(DialogueCover);
	TestTrue(TEXT("Shared Confirm/Advance key resolves one command"), PC->RoutePlayerInputKey(EKeys::Gamepad_FaceButton_Bottom, false, User));
	TestTrue(TEXT("Confirmation enters line without advancing it again"), Session->IsActive() && !Session->IsChoiceNode());
	const bool Typing = Session->IsTyping();
	PC->RoutePlayerInputKey(EKeys::Gamepad_FaceButton_Bottom, true, User);
	TestEqual(TEXT("Repeat does not complete the newly displayed line"), Session->IsTyping(), Typing);
	TestFalse(TEXT("Slate leaves dialogue pointer hit testing to UMG"), PC->RoutePlayerInputKey(EKeys::LeftMouseButton, false, User));
	const auto PointerCover = PC->AcquireInputRequest(Cover);
	Dialogue->NativeOnMouseButtonDown(Geometry, Pointer);
	TestEqual(TEXT("Covered background click cannot advance"), Session->IsTyping(), Typing);
	PC->ReleaseInputRequest(PointerCover);
	Dialogue->NativeOnMouseButtonDown(Geometry, Pointer);
	TestFalse(TEXT("Authorized background click completes typing"), Session->IsTyping());

	auto* History = CreateWidget<UTADialogueHistoryWidget>(PC, UTADialogueHistoryWidget::StaticClass());
	History->SetupClosePrompt(Dialogue->HistoryAction); History->NativeConstruct();
	Dialogue->ActiveHistoryWidget = History;
	History->OnCloseRequested.AddDynamic(Dialogue, &UTADialogueWidget::CloseHistoryOverlay);
	const auto HistoryCover = PC->AcquireInputRequest(Cover);
	History->HandleCloseClicked();
	TestTrue(TEXT("Covered history click cannot release request"), History->GetPlayerInputRequestHandle() != 0);
	PC->ReleaseInputRequest(HistoryCover);
	PC->NotifyApplicationActivationChanged(false);
	History->HandleCloseClicked();
	TestTrue(TEXT("External ownership also blocks UMG close"), History->GetPlayerInputRequestHandle() != 0);
	PC->NotifyApplicationActivationChanged(true);
	TestTrue(TEXT("Current history owns its shortcut"), PC->RoutePlayerInputKey(EKeys::H, false, User));
	TestEqual(TEXT("History close releases its request"), History->GetPlayerInputRequestHandle(), uint64(0));
	TestTrue(TEXT("History close restores current remaining dialogue"), PC->GetInputWinner().Request.Owner.Get() == Dialogue);
	TestNull(TEXT("Player debug puzzle opener cannot bypass dialogue owner"),
		UTAPathPuzzleWidget::OpenPuzzleWithSettingsFromPlayerInput(PC, nullptr, FTAPuzzleSettings()));
	History->NativeDestruct(); Dialogue->RemoveFromParent(); Dialogue->NativeDestruct(); Session->Shutdown();
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInventoryRepeatRoutingTest, "TheAwakening.Input.InventoryRepeatRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInventoryRepeatRoutingTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	auto* PanelClass = LoadClass<UTAInventoryPanelWidget>(nullptr,
		TEXT("/Game/UI/Inventory/WBP_InventoryPanel.WBP_InventoryPanel_C"));
	if (!TestNotNull(TEXT("Controller"), PC) || !TestNotNull(TEXT("Actual inventory WBP"), PanelClass) ||
		!TestTrue(TEXT("Slate initialized"), FSlateApplication::IsInitialized()))
	{ World->DestroyWorld(false); return false; }
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	World->AddController(PC);
	PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
	PC->InputComponent = NewObject<UEnhancedInputComponent>(PC);
	FUIIngressMappings Mappings; Mappings.Input = Cast<UTAPlayerInput>(PC->PlayerInput);
	FModifyContextOptions Options; Options.bForceImmediately = true;
	auto* Close = NewObject<UInputAction>();
	auto* Context = NewObject<UInputMappingContext>();
	Local->SetControllerId(0); // A raw NewObject LocalPlayer has no valid Slate user yet.
	const int32 User = Local->GetControllerId();
	auto& Slate = FSlateApplication::Get();
	const auto PreviousFocus = Slate.GetUserFocusedWidget(User);
	const auto Host = SNew(SWindow);
	Slate.RegisterVirtualWindow(Host);
	const auto Processor = MakeShared<FTAInputDeviceDetector>(PC);
	Slate.RegisterInputPreProcessor(Processor, 0);
	for (const FKey Key : {EKeys::Tab, EKeys::Gamepad_Special_Left, EKeys::K})
	{
		AddInfo(TEXT("Toggle source: ") + Key.ToString());
		Mappings.RemoveMappingContext(Context, Options);
		Context->UnmapAll(); Context->MapKey(Close, Key);
		Mappings.AddMappingContext(Context, 0, Options);
		const FKeyEvent Down(Key, FModifierKeysState(), User, false, 0, 0);
		const FKeyEvent Repeat(Key, FModifierKeysState(), User, true, 0, 0);
		// The first base-state Down goes to Gameplay's Enhanced Input Started entry.
		// Exercise its existing toggle latch, then construct the real WBP as that entry does.
		TestFalse(TEXT("Base Down is not executed by UI router"), Processor->HandleKeyDownEvent(Slate, Down));
		TestTrue(TEXT("Gameplay opening press accepted once"), PC->ConsumeInventoryTogglePress(Close));
		auto* Inventory = CreateWidget<UTAInventoryPanelWidget>(PC, PanelClass);
		Inventory->Init(nullptr, Close);
		const auto Surface = Inventory->TakeWidget();
		const auto Probe = SNew(SInventoryPreviewProbe)[Surface];
		Host->SetContent(Probe);
		TestTrue(TEXT("Focus actual inventory Slate widget"), Slate.SetUserFocus(User, Surface));
		const auto OpenHandle = Inventory->GetPlayerInputRequestHandle();
		TestTrue(TEXT("Actual Inventory acquired ownership"), OpenHandle != 0);
		for (int32 I = 0; I < 3; ++I)
			TestTrue(TEXT("Mapped Repeat consumed before UMG Preview"), Slate.ProcessKeyDownEvent(Repeat));
		TestEqual(TEXT("No Blueprint/UMG Preview path for mapped Repeat"), Probe->Previews, 0);
		TestEqual(TEXT("Opening hold cannot close Inventory"), Inventory->GetPlayerInputRequestHandle(), OpenHandle);
		TestTrue(TEXT("Duplicate non-repeat delivery is also latched"), Slate.ProcessKeyDownEvent(Down));
		PC->PlayerInput->ProcessInputStack({PC->InputComponent}, 0.016f, false);
		TestEqual(TEXT("Same opening Down cannot toggle twice"), Inventory->GetPlayerInputRequestHandle(), OpenHandle);
		TestTrue(TEXT("Held remains observed while menu consumes input"), PC->ReadHeldKey(Key).X != 0.f);
		if (Key == EKeys::K)
			for (const FKey OldKey : {EKeys::Tab, EKeys::Gamepad_Special_Left})
			{
				TestFalse(TEXT("Rebound close does not resolve old key names"), Inventory->ResolvePlayerInput(OldKey).IsSet());
				TestFalse(TEXT("Authorized unmapped Repeat has no hardcoded Preview interception"),
					Surface->OnPreviewKeyDown(FGeometry(), FKeyEvent(OldKey, FModifierKeysState(), User, true, 0, 0)).IsEventHandled());
			}
		TestTrue(TEXT("Up pairs consumed menu Down/Repeat"), Slate.ProcessKeyUpEvent(Down));
		PC->PlayerInput->ProcessInputStack({PC->InputComponent}, 0.016f, false);
		TestTrue(TEXT("Physical Release is observed as zero"), PC->ReadHeldKey(Key).IsNearlyZero());
		TestTrue(TEXT("New Press reaches Enhanced Input"), Slate.ProcessKeyDownEvent(Down));
		PC->PlayerInput->ProcessInputStack({PC->InputComponent}, 0.016f, false);
		TestEqual(TEXT("New Press closes exactly once"), Inventory->GetPlayerInputRequestHandle(), uint64(0));
		Slate.ProcessKeyDownEvent(Repeat);
		TestFalse(TEXT("Closing hold cannot reopen through Gameplay latch"), PC->ConsumeInventoryTogglePress(Close));
		TestTrue(TEXT("Stale Widget stops Preview before Blueprint Super"), Surface->OnPreviewKeyDown(FGeometry(), Repeat).IsEventHandled());
		TestTrue(TEXT("Closing Down's Up stays paired after Request release"), Slate.ProcessKeyUpEvent(Down));
		PC->PlayerInput->ProcessInputStack({PC->InputComponent}, 0.016f, false);
		TestFalse(TEXT("Base Down after Release remains Gameplay ingress"), Processor->HandleKeyDownEvent(Slate, Down));
		TestTrue(TEXT("Release then Press rearms normal opening"), PC->ConsumeInventoryTogglePress(Close));
		Inventory->NativeDestruct();

		auto* Reopened = CreateWidget<UTAInventoryPanelWidget>(PC, PanelClass);
		Reopened->Init(nullptr, Close);
		const auto ReopenedSurface = Reopened->TakeWidget();
		Host->SetContent(ReopenedSurface); Slate.SetUserFocus(User, ReopenedSurface);
		FTAInputRequest Cover; Cover.Owner = PC; Cover.Priority = 400;
		Cover.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::InventoryToggle};
		const auto CoverHandle = PC->AcquireInputRequest(Cover);
		TestFalse(TEXT("Covered Inventory cannot borrow winner Navigate"), Reopened->AllowsPlayerInput(ETAInputCapability::Navigate));
		TestTrue(TEXT("Covered Preview stops before Blueprint Super"), ReopenedSurface->OnPreviewKeyDown(FGeometry(), Repeat).IsEventHandled());
		Reopened->ExecutePlayerAction(Close, ETriggerEvent::Started, FInputActionInstance(Close));
		TestTrue(TEXT("Covered player behavior cannot close request"), Reopened->GetPlayerInputRequestHandle() != 0);
		PC->ReleaseInputRequest(CoverHandle);
		Reopened->RemoveFromParent(); Reopened->NativeDestruct();
		Processor->HandleKeyUpEvent(Slate, Down);
	}
	Slate.ClearUserFocus(User); Slate.UnregisterInputPreProcessor(Processor);
	Slate.UnregisterVirtualWindow(Host);
	if (PreviousFocus.IsValid()) Slate.SetUserFocus(User, PreviousFocus);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInventoryPlaceActionTest, "TheAwakening.Input.InventoryPlaceAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInventoryPlaceActionTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller"), PC)) { World->DestroyWorld(false); return false; }
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	Local->SetControllerId(0); World->AddController(PC);
	PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
	PC->InputComponent = NewObject<UEnhancedInputComponent>(PC);
	FUIIngressMappings Mappings; Mappings.Input = Cast<UTAPlayerInput>(PC->PlayerInput);
	FModifyContextOptions Options; Options.bForceImmediately = true;
	auto* Original = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_UI.IMC_UI"));
	auto* Context = DuplicateObject<UInputMappingContext>(Original, GetTransientPackage());
	Mappings.AddMappingContext(Context, 10, Options);
	auto* Move = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Drag"));
	auto* Place = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Confirm"));
	auto* ItemActions = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/UI/IA_ItemActions"));
	auto* Data = NewObject<UTAInventoryComponent>(PC); PC->AddInstanceComponent(Data); Data->RegisterComponent();
	auto* Inner = NewObject<UTAClothingDefinition>();
	FTAPocketDef Pocket; Pocket.SlotCount = 2; Inner->Pockets.Add(Pocket);
	Data->DefaultInnerClothing = Inner; Data->BeginPlay();
	auto* Item = NewObject<UTAItemDefinition>(); Data->TryAddItem(Item, 1);
	auto* Class = LoadClass<UTAInventoryPanelWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_InventoryPanel.WBP_InventoryPanel_C"));
	auto* UI = CreateWidget<UTAInventoryPanelWidget>(PC, Class);
	if (!TestNotNull(TEXT("Inventory WBP"), UI)) { World->DestroyWorld(false); return false; }
	UI->Init(Data); const auto InventorySurface = UI->TakeWidget();
	auto* Named = Cast<UNamedSlot>(UI->GetWidgetFromName(TEXT("NamedSlot_InnerClothing")));
	auto* Panel = Named ? Cast<UTAClothingPanelWidget>(Named->GetContent()) : nullptr;
	if (!Panel || !TestEqual(TEXT("Two pocket slots"), Panel->GetGeneratedSlots().Num(), 2))
	{ UI->RemoveFromParent(); World->DestroyWorld(false); return false; }
	auto* Source = Cast<UTAInventorySlotWidget>(Panel->GetGeneratedSlots()[0]);
	auto* Target = Cast<UTAInventorySlotWidget>(Panel->GetGeneratedSlots()[1]);
	const FKey Y = EKeys::Gamepad_FaceButton_Top, ConfirmKey = EKeys::Gamepad_FaceButton_Bottom;
	TestTrue(TEXT("Y/triangle comes from IMC"), PC->IsKeyMappedToAction(Y, Move));
	TestTrue(TEXT("A/cross confirmation comes from IMC"), PC->IsKeyMappedToAction(ConfirmKey, Place));
	auto Tap = [&](FKey Key) { EvaluateUIKey(PC, Key, IE_Pressed); EvaluateUIKey(PC, Key, IE_Released); };
	auto Select = [&]() { Source->OnInventorySlotHovered.Broadcast(Source, Item); Tap(Y); Target->OnInventorySlotHovered.Broadcast(Target, nullptr); };
	TestTrue(TEXT("Inventory owns input before selection"), UI->AllowsPlayerInput(ETAInputCapability::ToggleDrag));
	Select();
	// The cursor visual is the observable selection state.
	const auto* PreviewField = FindFProperty<FObjectPropertyBase>(UI->GetClass(), TEXT("GamepadDragVisualWidget"));
	TestTrue(TEXT("Y selects an item through evaluated action"), PreviewField && PreviewField->GetObjectPropertyValue_InContainer(UI) != nullptr);
	TestFalse(TEXT("Slate raw routing cannot place the item"), PC->RoutePlayerInputKey(ConfirmKey, false, 0));
	TestTrue(TEXT("Raw routing leaves the item in source"), !Data->GetSlots()[0].IsEmpty());
	Tap(ConfirmKey);
	TestTrue(TEXT("Evaluated A/cross places the stack"), Data->GetSlots()[0].IsEmpty() && Data->GetSlots()[1].Count == 1);
	TestTrue(TEXT("Placement retains inventory ownership"), PC->GetInputWinner().Request.Owner == UI);
	Data->MoveItemBetweenSlots(1, 0);
	Select(); Tap(Y);
	TestTrue(TEXT("Y/triangle still places the stack"), Data->GetSlots()[0].IsEmpty() && Data->GetSlots()[1].Count == 1);
	Data->MoveItemBetweenSlots(1, 0);
	// Both actions originally share A/cross; move both away to isolate the new binding.
	Context->UnmapKey(Place, ConfirmKey); Context->UnmapKey(ItemActions, ConfirmKey);
	Context->MapKey(Place, EKeys::J); Mappings.RequestRebuildControlMappings(Options);
	Select(); Tap(ConfirmKey);
	TestTrue(TEXT("Removed A/cross binding no longer places"), !Data->GetSlots()[0].IsEmpty());
	FTAInputRequest Cover; Cover.Owner = PC; Cover.Priority = 400; Cover.Allowed = {ETAInputCapability::Confirm};
	const auto Handle = PC->AcquireInputRequest(Cover);
	UI->ExecutePlayerAction(Place, ETriggerEvent::Started, FInputActionInstance(Place));
	TestTrue(TEXT("Covered inventory cannot place"), !Data->GetSlots()[0].IsEmpty());
	PC->ReleaseInputRequest(Handle); Tap(EKeys::J);
	TestTrue(TEXT("Rebound IMC key places"), Data->GetSlots()[0].IsEmpty() && Data->GetSlots()[1].Count == 1);
	Context->MapKey(ItemActions, ConfirmKey); Mappings.RequestRebuildControlMappings(Options);
	Target->OnInventorySlotHovered.Broadcast(Target, Item); Tap(ConfirmKey);
	// This isolated world has no viewport; retain and construct the menu surface
	// that AddToViewport would normally attach in-game.
	UTAItemActionMenuWidget* Menu = nullptr;
	for (TObjectIterator<UTAItemActionMenuWidget> It; It; ++It)
		if (!It->IsTemplate() && It->GetOwningPlayer() == PC) { Menu = *It; break; }
	TestNotNull(TEXT("Idle item action still opens its menu"), Menu);
	if (Menu)
	{
		const auto MenuSurface = Menu->TakeWidget();
		TestTrue(TEXT("Idle menu receives input ownership"), PC->GetInputWinner().Request.Owner.Get() == Menu);
		Menu->RemoveFromParent();
	}
	UI->RemoveFromParent();
	// The same action places whole coats in the swap UI through Enhanced Input.
	auto* Coat = NewObject<UTAClothingDefinition>(); Coat->Layer = ETAClothingLayer::Outer; Coat->Pockets.Add(Pocket);
	auto* Rack = World->SpawnActor<ATAClothingRack>(); UTAInventoryComponent::BuildClothingInstance(Coat, Rack->GetStoredClothingMutable());
	auto* SwapClass = LoadClass<UTAClothingSwapWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ClothingSwap.WBP_ClothingSwap_C"));
	auto* Swap = CreateWidget<UTAClothingSwapWidget>(PC, SwapClass); Swap->Init(Rack, Data); const auto SwapSurface = Swap->TakeWidget();
	auto* RackNamed = Cast<UNamedSlot>(Swap->GetWidgetFromName(TEXT("NamedSlot_RackCoat")));
	auto* PlayerNamed = Cast<UNamedSlot>(Swap->GetWidgetFromName(TEXT("NamedSlot_PlayerCoat")));
	auto* RackSlot = Cast<UTAInventorySlotWidget>(RackNamed->GetContent());
	auto* PlayerSlot = Cast<UTAInventorySlotWidget>(PlayerNamed->GetContent());
	RackSlot->OnInventorySlotHovered.Broadcast(RackSlot, nullptr); Tap(Y);
	PlayerSlot->OnInventorySlotHovered.Broadcast(PlayerSlot, nullptr); Tap(EKeys::J);
	TestTrue(TEXT("Remapped place action equips coat in swap UI"), Data->HasOuterClothing() && !Rack->HasClothing());
	Tap(EKeys::Escape);
	TestEqual(TEXT("IMC Back closes swap"), Swap->GetPlayerInputRequestHandle(), uint64(0));
	Swap->RemoveFromParent(); World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInventoryContextBackTest, "TheAwakening.Input.InventoryContextBack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInventoryContextBackTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller"), PC)) { World->DestroyWorld(false); return false; }
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	Local->SetControllerId(0); World->AddController(PC);
	PC->PlayerInput = NewObject<UTAPlayerInput>(PC); PC->InputComponent = NewObject<UEnhancedInputComponent>(PC);
	FUIIngressMappings Mappings; Mappings.Input = Cast<UTAPlayerInput>(PC->PlayerInput);
	FModifyContextOptions Options; Options.bForceImmediately = true;
	auto* Context = DuplicateObject<UInputMappingContext>(LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_UI")), GetTransientPackage());
	Mappings.AddMappingContext(Context, 10, Options);
	auto* ContextAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_ContextAction"));
	auto* BackAction = PC->GetUIBackAction();
	TestTrue(TEXT("Right-click contextual action is authored in IMC"), PC->IsKeyMappedToAction(EKeys::RightMouseButton, ContextAction));
	TestTrue(TEXT("B/circle Back is authored in IMC"), PC->IsKeyMappedToAction(EKeys::Gamepad_FaceButton_Right, BackAction));
	auto Tap = [&](FKey Key) { EvaluateUIKey(PC, Key, IE_Pressed); EvaluateUIKey(PC, Key, IE_Released); };
	auto* Data = NewObject<UTAInventoryComponent>(PC); PC->AddInstanceComponent(Data); Data->RegisterComponent();
	auto* Inner = NewObject<UTAClothingDefinition>(); FTAPocketDef Pocket; Pocket.SlotCount = 2; Inner->Pockets.Add(Pocket);
	Data->DefaultInnerClothing = Inner; Data->BeginPlay();
	auto* Item = NewObject<UTAItemDefinition>(); Data->TryAddItem(Item, 1);
	auto* InventoryClass = LoadClass<UTAInventoryPanelWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_InventoryPanel.WBP_InventoryPanel_C"));
	TArray<TSharedPtr<SWidget>> Surfaces;
	auto OpenInventory = [&]()
	{
		auto* UI = CreateWidget<UTAInventoryPanelWidget>(PC, InventoryClass); UI->Init(Data); Surfaces.Add(UI->TakeWidget()); return UI;
	};
	auto Slot = [](UUserWidget* UI, const TCHAR* PanelName, int32 Index)
	{
		auto* Named = Cast<UNamedSlot>(UI->GetWidgetFromName(PanelName));
		auto* Panel = Named ? Cast<UTAClothingPanelWidget>(Named->GetContent()) : nullptr;
		return Panel && Panel->GetGeneratedSlots().IsValidIndex(Index) ? Cast<UTAInventorySlotWidget>(Panel->GetGeneratedSlots()[Index]) : nullptr;
	};
	auto CheckPrompts = [&](UUserWidget* UI)
	{
		auto* Bar = Cast<UHorizontalBox>(UI->GetWidgetFromName(TEXT("HorizontalBox_Controls")));
		bool bHasContext = false, bHasBack = false;
		if (Bar) for (UWidget* Child : Bar->GetAllChildren()) if (auto* Prompt = Cast<UTAActionPromptWidget>(Child))
		{
			bHasContext |= Prompt->GetPromptAction() == ContextAction;
			bHasBack |= Prompt->GetPromptAction() == BackAction;
		}
		TestTrue(TEXT("Bottom-right contains contextual right-click hint"), bHasContext);
		TestTrue(TEXT("Bottom-right contains gamepad Back hint"), bHasBack);
	};
	auto* UI = OpenInventory(); CheckPrompts(UI);
	auto* Occupied = Slot(UI, TEXT("NamedSlot_InnerClothing"), 0);
	auto* Empty = Slot(UI, TEXT("NamedSlot_InnerClothing"), 1);
	if (!TestNotNull(TEXT("Occupied slot"), Occupied) || !TestNotNull(TEXT("Empty slot"), Empty))
	{ UI->RemoveFromParent(); World->DestroyWorld(false); return false; }
	Occupied->OnInventorySlotHovered.Broadcast(Occupied, Item); Tap(EKeys::RightMouseButton);
	UTAItemActionMenuWidget* Menu = nullptr; int32 MenuCount = 0;
	for (TObjectIterator<UTAItemActionMenuWidget> It; It; ++It)
		if (!It->IsTemplate() && It->GetOwningPlayer() == PC) { Menu = *It; ++MenuCount; }
	TestEqual(TEXT("Right-click on item creates exactly one menu"), MenuCount, 1);
	TestTrue(TEXT("Item right-click does not close inventory"), UI->GetPlayerInputRequestHandle() != 0);
	if (Menu) { Surfaces.Add(Menu->TakeWidget()); Menu->RemoveFromParent(); }
	Empty->OnInventorySlotHovered.Broadcast(Empty, nullptr);
	FTAInputRequest Cover; Cover.Owner = PC; Cover.Priority = 400; Cover.Allowed = {ETAInputCapability::Close, ETAInputCapability::Confirm};
	const auto CoverHandle = PC->AcquireInputRequest(Cover);
	UI->ExecutePlayerAction(ContextAction, ETriggerEvent::Started, FInputActionInstance(ContextAction));
	TestTrue(TEXT("Covered inventory cannot close"), UI->GetPlayerInputRequestHandle() != 0);
	PC->ReleaseInputRequest(CoverHandle); Tap(EKeys::RightMouseButton);
	TestEqual(TEXT("Empty-slot right-click closes inventory"), UI->GetPlayerInputRequestHandle(), uint64(0));
	UI = OpenInventory(); Tap(EKeys::RightMouseButton);
	TestEqual(TEXT("Background right-click closes inventory"), UI->GetPlayerInputRequestHandle(), uint64(0));
	UI = OpenInventory(); Tap(EKeys::Gamepad_FaceButton_Right);
	TestEqual(TEXT("B/circle closes inventory"), UI->GetPlayerInputRequestHandle(), uint64(0));
	// Remap only the context action; right-click continues to mean item actions.
	Context->UnmapKey(ContextAction, EKeys::RightMouseButton); Context->MapKey(ContextAction, EKeys::J); Mappings.RequestRebuildControlMappings(Options);
	UI = OpenInventory(); Tap(EKeys::RightMouseButton);
	TestTrue(TEXT("Removing the IMC binding removes background right-click Back"), UI->GetPlayerInputRequestHandle() != 0);
	Tap(EKeys::J); TestEqual(TEXT("Rebound context action closes inventory"), UI->GetPlayerInputRequestHandle(), uint64(0));
	Context->UnmapKey(ContextAction, EKeys::J); Context->MapKey(ContextAction, EKeys::RightMouseButton); Mappings.RequestRebuildControlMappings(Options);
	UI = OpenInventory(); Occupied = Slot(UI, TEXT("NamedSlot_InnerClothing"), 0);
	Occupied->OnInventorySlotHovered.Broadcast(Occupied, Item); Tap(EKeys::Gamepad_FaceButton_Top);
	Occupied->OnInventorySlotUnhovered.Broadcast(Occupied); Tap(EKeys::RightMouseButton);
	TestTrue(TEXT("Right-click while carrying an item cannot also close inventory"), UI->GetPlayerInputRequestHandle() != 0);
	UI->RemoveFromParent();
	auto* Coat = NewObject<UTAClothingDefinition>(); Coat->Layer = ETAClothingLayer::Outer; Coat->Pockets.Add(Pocket);
	auto* Rack = World->SpawnActor<ATAClothingRack>(); UTAInventoryComponent::BuildClothingInstance(Coat, Rack->GetStoredClothingMutable());
	auto* SwapClass = LoadClass<UTAClothingSwapWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ClothingSwap.WBP_ClothingSwap_C"));
	auto OpenSwap = [&]() { auto* Swap = CreateWidget<UTAClothingSwapWidget>(PC, SwapClass); Swap->Init(Rack, Data); Surfaces.Add(Swap->TakeWidget()); return Swap; };
	auto* Swap = OpenSwap(); CheckPrompts(Swap);
	auto* CoatNamed = Cast<UNamedSlot>(Swap->GetWidgetFromName(TEXT("NamedSlot_RackCoat")));
	auto* CoatSlot = Cast<UTAInventorySlotWidget>(CoatNamed->GetContent());
	CoatSlot->OnInventorySlotHovered.Broadcast(CoatSlot, nullptr); Tap(EKeys::RightMouseButton);
	TestTrue(TEXT("Right-click on a displayed coat does not close swap"), Swap->GetPlayerInputRequestHandle() != 0);
	CoatSlot->OnInventorySlotUnhovered.Broadcast(CoatSlot); Tap(EKeys::RightMouseButton);
	TestEqual(TEXT("Background right-click closes swap"), Swap->GetPlayerInputRequestHandle(), uint64(0));
	Swap = OpenSwap(); auto* EmptyPocket = Slot(Swap, TEXT("NamedSlot_RackClothing"), 0);
	if (EmptyPocket) EmptyPocket->OnInventorySlotHovered.Broadcast(EmptyPocket, nullptr);
	Tap(EKeys::RightMouseButton); TestEqual(TEXT("Empty pocket right-click closes swap"), Swap->GetPlayerInputRequestHandle(), uint64(0));
	Swap = OpenSwap(); Tap(EKeys::Gamepad_FaceButton_Right);
	TestEqual(TEXT("B/circle closes swap"), Swap->GetPlayerInputRequestHandle(), uint64(0));
	World->DestroyWorld(false);
	return true;
}
#endif
