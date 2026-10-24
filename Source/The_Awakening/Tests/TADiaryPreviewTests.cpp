#include "Document/TADiaryPreviewStage.h"
#include "Document/TADiaryDocumentComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TATimeSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UI/Document/TADiaryWidget.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentSubsystem.h"
#include "Tests/TADocumentTestGameInstance.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "The_AwakeningPlayerController.h"
#include "Core/TAPlayerInput.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystemInterface.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADiaryPreviewTest, "TheAwakening.Document.Diary.Preview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADiaryPreviewTest::RunTest(const FString&)
{
	TArray<FString> CookDirectories;
	GConfig->GetArray(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("DirectoriesToAlwaysCook"), CookDirectories, GGameIni);
	TestTrue(TEXT("Native diary soft references have an explicit cook root"), CookDirectories.Contains(TEXT("(Path=\"/Game/Models/Diary\")")));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ATADiaryPreviewStage* Stage = ATADiaryPreviewStage::SpawnDiaryStage(World);
	if (TestNotNull(TEXT("Diary assets load with matching skeletons"), Stage))
	{
		auto* Body = Stage->GetSkeletalMeshComponent();
		auto* Page = Stage->GetTurningPageComponent();
		const FVector Camera = Stage->GetCaptureComponent()->GetRelativeLocation();
		const TArray<FTransform> ClosedBones = Body->GetComponentSpaceTransforms();
		TestTrue(TEXT("Page copies the book pose"), Page->LeaderPoseComponent.Get() == Body);
		TestFalse(TEXT("Reusable page hidden at rest"), Page->IsVisible());
		TestFalse(TEXT("Cannot turn closed book"), Stage->TurnPage(true));
		TestTrue(TEXT("Open starts"), Stage->OpenBook());
		TestFalse(TEXT("Cannot close during opening"), Stage->CloseBook());
		UTATimeSubsystem* Time = World->GetSubsystem<UTATimeSubsystem>();
		Time->RequestFreeze(Stage);
		Time->AdvanceGameplayTime(2.f);
		TestEqual(TEXT("Gameplay remains frozen"), Time->GetGameplayTime(), 0.0);
		Stage->Tick(2.f);
		TestTrue(TEXT("Book finishes opening during freeze"), Stage->GetMotion() == ETADiaryMotion::Open);
		const TArray<FTransform> OpenBones = Body->GetComponentSpaceTransforms();
		bool bPoseChanged = false;
		for (int32 I = 0; I < FMath::Min(ClosedBones.Num(), OpenBones.Num()); ++I)
			bPoseChanged |= !ClosedBones[I].Equals(OpenBones[I], 0.001f);
		TestTrue(TEXT("Opening actually moves book bones"), bPoseChanged);
		for (bool bForward : {true, false})
		{
			TestTrue(TEXT("Turn starts"), Stage->TurnPage(bForward));
			TestTrue(TEXT("Page visible only while turning"), Page->IsVisible());
			TestFalse(TEXT("Repeated turn rejected"), Stage->TurnPage(bForward));
			Stage->Tick(0.5f);
			bool bPageMoved = false;
			const auto& TurningBones = Body->GetComponentSpaceTransforms();
			for (int32 I = 0; I < FMath::Min(OpenBones.Num(), TurningBones.Num()); ++I)
				bPageMoved |= !OpenBones[I].Equals(TurningBones[I], 0.001f);
			TestTrue(TEXT("Turn evaluates moving bones"), bPageMoved);
			Stage->Tick(1.f);
			TestTrue(TEXT("Turn ends open"), Stage->GetMotion() == ETADiaryMotion::Open);
			TestFalse(TEXT("Page hidden before reset"), Page->IsVisible());
			TestTrue(TEXT("Fixed camera across book motions"), Camera.Equals(Stage->GetCaptureComponent()->GetRelativeLocation()));
		}
		TestTrue(TEXT("Close starts"), Stage->CloseBook());
		Stage->Tick(2.f);
		TestTrue(TEXT("Close finishes"), Stage->GetMotion() == ETADiaryMotion::Closed);
		Stage->SetOpenImmediate(true);
		TestTrue(TEXT("Quick view skips opening"), Stage->GetMotion() == ETADiaryMotion::Open);
		{
			const uint32 Before = Body->GetBoneTransformRevisionNumber();
			TGuardValue<uint64> NewFrame(GFrameCounter, GFrameCounter + 1);
			Stage->Tick(0.f);
			TestTrue(TEXT("Idle quick view publishes a new GPU bone revision next frame"), Body->GetBoneTransformRevisionNumber() > Before);
			TestTrue(TEXT("Presentation ready only after publishing in a later frame"), Stage->IsPresentationReady());
		}
		Stage->TurnPage(true);
		Stage->SetOpenImmediate(false);
		Stage->Tick(2.f);
		TestTrue(TEXT("Forced reset cancels active motion"), Stage->GetMotion() == ETADiaryMotion::Closed);
		TestFalse(TEXT("Forced reset hides page"), Page->IsVisible());
		Time->ReleaseFreeze(Stage);
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADiaryWidgetIntegrationTest, "TheAwakening.Document.Diary.WidgetIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADiaryWidgetIntegrationTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* GI = NewObject<UTADocumentTestGameInstance>();
	GI->SetTestWorld(World);
	World->SetGameInstance(GI);
	GI->Init();
	World->InitializeActorsForPlay(FURL());
	auto* Documents = GI->GetSubsystem<UTADocumentSubsystem>();
	UClass* WidgetClass = LoadClass<UTADiaryWidget>(nullptr, TEXT("/Game/UI/Document/WBP_Diary.WBP_Diary_C"));
	auto* Widget = WidgetClass ? CreateWidget<UTADiaryWidget>(World, WidgetClass) : nullptr;
	if (TestNotNull(TEXT("Rebuilt diary Blueprint loads"), Widget) && TestNotNull(TEXT("Document subsystem"), Documents))
	{
		const auto DiarySlate = Widget->TakeWidget();
		if (TestNotNull(TEXT("Book stage initialized from actual widget"), Widget->BookStage.Get()))
		{
			TestTrue(TEXT("Old 2D book borders replaced"), Widget->Panel_Book->IsA<UCanvasPanel>());
			TestNotNull(TEXT("Book image has render material"), Widget->Image_Book->GetBrush().GetResourceObject());
			TestNotNull(TEXT("Documents share the book stage"), Widget->BookStage->GetRightDocument());
			Widget->RequestClose();
			TestTrue(TEXT("Back ignored during opening"), Widget->Phase == UTADiaryWidget::EPhase::Opening);
			Widget->BookStage->Tick(2.f);
			TestTrue(TEXT("Animation completion unlocks widget"), Widget->Phase == UTADiaryWidget::EPhase::Open);
			TestTrue(TEXT("Empty diary message visible"), Widget->Text_Empty->IsVisible());
			Widget->TurnPage(1);
			TestTrue(TEXT("Empty diary cannot flip"), Widget->Phase == UTADiaryWidget::EPhase::Open);
			auto* First = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_Sample01.DA_Document_Sample01"));
			auto* Second = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_Sample02.DA_Document_Sample02"));
			if (TestNotNull(TEXT("First sample"), First) && TestNotNull(TEXT("Second sample"), Second))
			{
				Documents->Collect(First->DocumentId);
				Widget->ShowPage(0);
				Widget->TurnPage(1);
				TestTrue(TEXT("Single document cannot flip"), Widget->Phase == UTADiaryWidget::EPhase::Open);
				Documents->Collect(Second->DocumentId);
				Widget->TurnPage(1);
				TestEqual(TEXT("Page not committed before animation completes"), Widget->CurrentPage, 0);
				TestTrue(TEXT("Outgoing document remains visible on moving page"), Widget->BookStage->GetMovingDocument()->IsVisible());
				Widget->TurnPage(1);
				Widget->RequestClose();
				TestTrue(TEXT("Repeated turn/back do not interrupt flip"), Widget->Phase == UTADiaryWidget::EPhase::Flipping);
				Widget->BookStage->Tick(1.1f);
				TestEqual(TEXT("Forward completion commits target page"), Widget->CurrentPage, 1);
				Widget->TurnPage(-1);
				Widget->BookStage->Tick(1.1f);
				TestEqual(TEXT("Backward completion commits previous page"), Widget->CurrentPage, 0);
				Widget->TurnPage(-1);
				Widget->BookStage->Tick(1.1f);
				TestEqual(TEXT("Previous at first page does not wrap"), Widget->CurrentPage, 0);
				Widget->TurnToPage(1);
				Widget->BookStage->Tick(1.1f);
				Widget->TurnPage(1);
				TestTrue(TEXT("Next at last page does not start a flip"), Widget->Phase == UTADiaryWidget::EPhase::Open);
				TestEqual(TEXT("Next at last page does not wrap"), Widget->CurrentPage, 1);
				auto* Third = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_67.DA_Document_67"));
				Documents->Collect(Third->DocumentId);
				Widget->HeldPageDirection = -1;
				Widget->bPageHoldTriggered = false;
				Widget->PageHeldTime = 0.f;
				Widget->UpdatePageHold(Widget->PageHoldDuration, true, true);
				Widget->BookStage->Tick(1.1f);
				TestEqual(TEXT("Hold previous jumps to first page"), Widget->CurrentPage, 0);
				TestNull(TEXT("First page has no wrapped last document on left"), Widget->BookStage->GetLeftDocument()->GetDefinition());
				Widget->HeldPageDirection = 1;
				Widget->bPageHoldTriggered = false;
				Widget->PageHeldTime = 0.f;
				Widget->UpdatePageHold(Widget->PageHoldDuration * 0.5f, true, true);
				Widget->UpdatePageHold(Widget->PageHoldDuration, false, true);
				TestEqual(TEXT("Released short hold does not jump"), Widget->CurrentPage, 0);
				Widget->HeldPageDirection = 1;
				Widget->PageHeldTime = 0.f;
				Widget->bPageHoldTriggered = false;
				Widget->TurnPage(1);
				Widget->UpdatePageHold(Widget->PageHoldDuration, true, true);
				Widget->BookStage->Tick(1.1f);
				Widget->UpdatePageHold(0.01f, true, true);
				Widget->BookStage->Tick(1.1f);
				TestEqual(TEXT("Hold next waits for active flip then jumps to last"), Widget->CurrentPage, 2);
				Widget->UpdatePageHold(2.f, true, true);
				TestTrue(TEXT("Continued hold fires only once"), Widget->Phase == UTADiaryWidget::EPhase::Open);
				Widget->HeldPageDirection = -1;
				Widget->bPageHoldTriggered = false;
				Widget->PageHeldTime = 0.f;
				Widget->UpdatePageHold(2.f, true, false);
				TestEqual(TEXT("Input ownership loss cancels hold"), Widget->CurrentPage, 2);
			}
			Widget->RequestClose();
			TestEqual(TEXT("Close persists current page"), Documents->GetDiaryPageIndex(), Widget->CurrentPage);
			Widget->BookStage->Tick(2.f);
			TestNull(TEXT("Normal close releases book stage"), Widget->BookStage.Get());
		}
		Widget->NativeDestruct();
		auto* Quick = CreateWidget<UTADiaryWidget>(World, WidgetClass);
		Quick->bSkipOpenAnimation = true;
		Quick->RequestedPage = 0;
		const auto QuickSlate = Quick->TakeWidget();
		TestTrue(TEXT("Quick view immediately open"), Quick->Phase == UTADiaryWidget::EPhase::Open);
		TestEqual(TEXT("Requested page honored"), Quick->CurrentPage, 0);
		Quick->TurnPage(1);
		Quick->RemoveFromParent();
		TestNull(TEXT("Forced removal during turn releases book"), Quick->BookStage.Get());
		Quick->NativeDestruct();
		// Waiting for icon geometry must remain cancellable even before the opening starts.
		auto* Pending = CreateWidget<UTADiaryWidget>(World, WidgetClass);
		Pending->SourcePosition = FVector2D(20.f, 20.f);
		const auto PendingSlate = Pending->TakeWidget();
		TestTrue(TEXT("Icon layout pending before opening"), Pending->bPendingOpenFromIcon);
		Pending->RequestClose();
		TestNull(TEXT("Cancel while waiting for icon releases stage"), Pending->BookStage.Get());
		Pending->NativeDestruct();
		// Reproduce a packaged asset missing from cook: both entry modes must fail before
		// AddToViewport can attach an uninitialized, uncloseable modal background.
		auto* PC = World->SpawnActor<APlayerController>();
		auto* Local = NewObject<ULocalPlayer>(GEngine);
		PC->Player = Local; Local->PlayerController = PC;
		AddExpectedError(TEXT("Diary preview: book meshes missing or incompatible"), EAutomationExpectedErrorFlags::Contains, 2);
		AddExpectedError(TEXT("refusing to display modal UI"), EAutomationExpectedErrorFlags::Contains, 2);
		for (bool QuickView : {false, true})
		{
			auto* Failed = CreateWidget<UTADiaryWidget>(PC, WidgetClass);
			Failed->bSkipOpenAnimation = QuickView;
			Failed->bOpenDocumentImmediately = QuickView;
			Failed->BookStage = Cast<ATADiaryPreviewStage>(ATAPreviewStage::SpawnPreviewStage(World, ATADiaryPreviewStage::StaticClass(), 1));
			Failed->BookStage->BodyMesh.Reset();
			TestFalse(TEXT("Missing cooked book refuses modal creation in both entry modes"), Failed->TryOpen());
			TestFalse(TEXT("Failed modal never attaches to viewport"), Failed->IsInViewport());
			TestNull(TEXT("Failed modal releases book"), Failed->BookStage.Get());
		}
		int32 RemainingStages = 0;
		for (TActorIterator<ATADiaryPreviewStage> It(World); It; ++It) ++RemainingStages;
		TestEqual(TEXT("Failed initialization leaves no preview actors"), RemainingStages, 0);
	}
	GI->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADiaryEnhancedInputTest, "TheAwakening.Document.Diary.EnhancedInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADiaryEnhancedInputTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* GI = NewObject<UTADocumentTestGameInstance>();
	GI->SetTestWorld(World); World->SetGameInstance(GI); GI->Init();
	World->InitializeActorsForPlay(FURL());
	auto* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
	auto* PC = PCClass ? World->SpawnActor<AThe_AwakeningPlayerController>(PCClass) : nullptr;
	if (!TestNotNull(TEXT("Controller for evaluated action dispatch"), PC)) return false;
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
	PC->InputComponent = NewObject<UEnhancedInputComponent>(PC);
	struct FMappingSubsystem : IEnhancedInputSubsystemInterface
	{
		UEnhancedPlayerInput* Input = nullptr;
		TMap<TObjectPtr<const UInputAction>, FInjectedInput> Injected;
		virtual UEnhancedPlayerInput* GetPlayerInput() const override { return Input; }
		virtual TMap<TObjectPtr<const UInputAction>, FInjectedInput>& GetContinuouslyInjectedInputs() override { return Injected; }
	} Mappings;
	Mappings.Input = Cast<UTAPlayerInput>(PC->PlayerInput);
	auto* Source = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_UI.IMC_UI"));
	auto* Context = DuplicateObject<UInputMappingContext>(Source, GetTransientPackage());
	FModifyContextOptions Options; Options.bForceImmediately = true;
	Mappings.AddMappingContext(Context, 10, Options);
	auto* Docs = GI->GetSubsystem<UTADocumentSubsystem>();
	for (FName Id : {FName(TEXT("Doc_Sample01")), FName(TEXT("Doc_Sample02")), FName(TEXT("Doc_67"))}) Docs->Collect(Id);
	auto* Class = LoadClass<UTADiaryWidget>(nullptr, TEXT("/Game/UI/Document/WBP_Diary.WBP_Diary_C"));
	auto* Diary = CreateWidget<UTADiaryWidget>(PC, Class);
	Diary->bSkipOpenAnimation = true;
	const auto Slate = Diary->TakeWidget();
	auto Send = [&](FKey Key, EInputEvent Event)
	{
		PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event, Event == IE_Released ? 0.f : 1.f, false, 0u));
		PC->PlayerInput->ProcessInputStack({PC->InputComponent}, 0.016f, false);
	};
	TestTrue(TEXT("Q uses previous action from IMC"), PC->IsKeyMappedToAction(EKeys::Q, Diary->PreviousPageAction));
	TestTrue(TEXT("E uses next action from IMC"), PC->IsKeyMappedToAction(EKeys::E, Diary->NextPageAction));
	TestFalse(TEXT("A no longer means previous"), PC->IsKeyMappedToAction(EKeys::A, Diary->PreviousPageAction));
	TestFalse(TEXT("Slate key dispatch cannot turn the diary"), PC->RoutePlayerInputKey(EKeys::E, false, Local->GetControllerId()));
	TestTrue(TEXT("Slate transports E into Enhanced Input"), PC->ShouldForwardEnhancedUIKey(EKeys::E));
	Send(EKeys::E, IE_Pressed);
	TestTrue(TEXT("Real Enhanced Input Started begins page turn"), Diary->Phase == UTADiaryWidget::EPhase::Flipping);
	TestTrue(TEXT("Action Started arms hold"), Diary->bPageActionHeld);
	Send(EKeys::E, IE_Released);
	TestFalse(TEXT("Action Completed clears hold"), Diary->bPageActionHeld);
	Diary->BookStage->Tick(1.1f);
	TestEqual(TEXT("E commits next page"), Diary->CurrentPage, 1);
	Context->UnmapKey(Diary->NextPageAction, EKeys::E);
	Context->MapKey(Diary->NextPageAction, EKeys::T);
	Mappings.RequestRebuildControlMappings(Options);
	Send(EKeys::E, IE_Pressed); Send(EKeys::E, IE_Released);
	TestTrue(TEXT("Removed E binding has no effect"), Diary->Phase == UTADiaryWidget::EPhase::Open);
	Send(EKeys::T, IE_Pressed); Send(EKeys::T, IE_Released);
	TestTrue(TEXT("Remapped T triggers same action"), Diary->Phase == UTADiaryWidget::EPhase::Flipping);
	Diary->BookStage->Tick(1.1f);
	TestEqual(TEXT("Remapped action commits last page"), Diary->CurrentPage, 2);
	Send(EKeys::Escape, IE_Pressed); Send(EKeys::Escape, IE_Released);
	TestTrue(TEXT("IMC Back action closes diary"), Diary->Phase == UTADiaryWidget::EPhase::Closing);
	Diary->RemoveFromParent(); Diary->NativeDestruct();
	GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
