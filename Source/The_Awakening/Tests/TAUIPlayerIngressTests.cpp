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

namespace
{
struct FUIIngressMappings : IEnhancedInputSubsystemInterface
{
	UEnhancedPlayerInput* Input = nullptr;
	TMap<TObjectPtr<const UInputAction>, FInjectedInput> Injected;
	virtual UEnhancedPlayerInput* GetPlayerInput() const override { return Input; }
	virtual TMap<TObjectPtr<const UInputAction>, FInjectedInput>& GetContinuouslyInjectedInputs() override { return Injected; }
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
#endif
