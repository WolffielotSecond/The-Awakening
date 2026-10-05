#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "The_AwakeningPlayerController.h"
#include "Core/TAPlayerInput.h"
#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
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
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"

namespace
{
struct FUIIngressMappings : IEnhancedInputSubsystemInterface
{
	UEnhancedPlayerInput* Input = nullptr;
	TMap<TObjectPtr<const UInputAction>, FInjectedInput> Injected;
	virtual UEnhancedPlayerInput* GetPlayerInput() const override { return Input; }
	virtual TMap<TObjectPtr<const UInputAction>, FInjectedInput>& GetContinuouslyInjectedInputs() override { return Injected; }
};

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
	const FDragDropEvent Drop(Pointer, TSharedPtr<FDragDropOperation>());
	TestFalse(TEXT("Covered Drop cannot mutate inventory"), Slots[1]->NativeOnDrop(Geometry, Drop, Drag));
	TestTrue(TEXT("Denied Drop preserves item location"), !Data->GetSlots()[0].IsEmpty() && Data->GetSlots()[1].IsEmpty());
	UDragDropOperation* DeniedDrag = nullptr;
	Slots[0]->NativeOnDragDetected(Geometry, Pointer, DeniedDrag);
	TestNull(TEXT("Covered drag cannot start"), DeniedDrag);
	Slots[0]->NativeOnDragCancelled(Drop, Drag); // cleanup remains callable while covered
	Inventory->ExecutePlayerInput(EKeys::Tab, ETAInputCapability::InventoryToggle);
	TestTrue(TEXT("Covered close retains original request"), Inventory->GetPlayerInputRequestHandle() != 0);
	PC->ReleaseInputRequest(CoverHandle);
	TestTrue(TEXT("Drop works again after remaining winner restores"), Slots[1]->NativeOnDrop(Geometry, Drop, Drag));
	TestTrue(TEXT("Authorized Drop moves the stack once"), Data->GetSlots()[0].IsEmpty() && Data->GetSlots()[1].Count == 1);
	Skills->OnClicked.Broadcast(); TestEqual(TEXT("Authorized button changes page"), Switcher->GetActiveWidgetIndex(), 1);
	PC->RecordHeldInput(EKeys::Tab, 1.f, User);
	TestTrue(TEXT("Close key routed once"), PC->RoutePlayerInputKey(EKeys::Tab, false, User));
	TestEqual(TEXT("Close releases the exact owner"), Inventory->GetPlayerInputRequestHandle(), uint64(0));
	Inventory->NativeDestruct(); PC->RecordHeldInput(EKeys::Tab, 0.f, User);

	FTAStoryData Story; Story.Entry = TEXT("choice");
	FTAStoryNode Choice; Choice.Id = Story.Entry; Choice.Type = TEXT("choice");
	FTAStoryChoice Option; Option.Target = TEXT("line"); Choice.Choices.Add(Option);
	FTAStoryNode Line; Line.Id = TEXT("line"); Line.Type = TEXT("dialogue"); Line.TextId = TEXT("test-line");
	Story.Nodes = {Choice, Line}; Story.RebuildIndex();
	auto* Session = NewObject<UTADialogueController>(); Session->Initialize(nullptr, Story, PC); Session->Start();
	auto* Dialogue = CreateWidget<UTADialogueWidget>(PC, UTADialogueWidget::StaticClass());
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
		TestTrue(TEXT("Physical Release is observed as zero"), PC->ReadHeldKey(Key).IsNearlyZero());
		TestTrue(TEXT("New Press routes authorized close"), Slate.ProcessKeyDownEvent(Down));
		TestEqual(TEXT("New Press closes exactly once"), Inventory->GetPlayerInputRequestHandle(), uint64(0));
		Slate.ProcessKeyDownEvent(Repeat);
		TestFalse(TEXT("Closing hold cannot reopen through Gameplay latch"), PC->ConsumeInventoryTogglePress(Close));
		TestTrue(TEXT("Stale Widget stops Preview before Blueprint Super"), Surface->OnPreviewKeyDown(FGeometry(), Repeat).IsEventHandled());
		TestTrue(TEXT("Closing Down's Up stays paired after Request release"), Slate.ProcessKeyUpEvent(Down));
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
		Reopened->ExecutePlayerInput(Key, ETAInputCapability::InventoryToggle);
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
#endif
