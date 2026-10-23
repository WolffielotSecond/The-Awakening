#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Story/TADialogueSubsystem.h"
#include "Story/TAWorldEventDefinition.h"
#include "Interaction/TAInteractionBehaviorComponent.h"
#include "Interaction/TASpecialActionComponent.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TAGameplayTags.h"
#include "Core/TAPlayerInput.h"
#include "Core/TAPlayerState.h"
#include "Core/TAInputIconSubsystem.h"
#include "Health/TARollbackSubsystem.h"
#include "Health/TADamageLibrary.h"
#include "Document/TACharacterActionComponent.h"
#include "Core/TATimeSubsystem.h"
#include "Core/TAFreezeComponent.h"
#include "Core/TAFreezeSubsystem.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/TAInteractableActor.h"
#include "Debug/TADebugQTETester.h"
#include "Interaction/TAQTESequenceComponent.h"
#include "Save/TASaveSubsystem.h"
#include "UI/TAQTEWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "ShaderCompiler.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#include "Tests/TADocumentTestGameInstance.h"
#include "Tests/TAInteractionTestTypes.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace TAWorldInteractionTests
{
 struct FFixture
 {
  UWorld* World;
  UTADocumentTestGameInstance* GI;
  UTADialogueSubsystem* Story;
  FFixture()
  {
   World = UWorld::CreateWorld(EWorldType::Game, false);
   GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
   GI = NewObject<UTADocumentTestGameInstance>();
   GI->SetTestWorld(World); World->SetGameInstance(GI); GI->Init();
   World->InitializeActorsForPlay(FURL());
   Story = GI->GetSubsystem<UTADialogueSubsystem>();
  }
  ~FFixture()
  {
   GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
  }
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAWorldEventLifecycleTest, "TheAwakening.Interaction.WorldEventLifecycle",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAWorldEventLifecycleTest::RunTest(const FString&)
{
 TAWorldInteractionTests::FFixture F;
 auto* Actor = F.World->SpawnActor<AActor>();
 auto* Executor = F.World->SpawnActor<AActor>();
 auto* OtherExecutor = F.World->SpawnActor<AActor>();
 auto* Pipe = NewObject<UTAWorldEventDefinition>();
 Pipe->EventId = TEXT("Attack"); Pipe->ExclusiveGroup = TEXT("Monster");
 Pipe->Preconditions.RequiredTags.AddTag(TAGameplayTags::Story_Chapter1_PipeAcquired);
 Pipe->CompletionTags.AddTag(TAGameplayTags::Story_Chapter1_Attacked);
 Pipe->CompletionFlagDeltas.Add(TEXT("Morality"), -1);
 auto* Crane = NewObject<UTAWorldEventDefinition>();
 Crane->EventId = TEXT("Crane"); Crane->ExclusiveGroup = TEXT("Monster");
 TestFalse(TEXT("Attack requires actual pipe acquisition"), F.Story->StartWorldEvent(Pipe, Actor, Executor));
 F.Story->SetWorldStateTag(TAGameplayTags::Story_Chapter1_PipeAcquired, true);
 TestTrue(TEXT("Start attack"), F.Story->StartWorldEvent(Pipe, Actor, Executor));
 TestFalse(TEXT("Selection has not committed result"), F.Story->HasCompletedWorldEvent(Pipe->EventId));
 TestFalse(TEXT("Running branch reserves group"), F.Story->StartWorldEvent(Crane, Actor, OtherExecutor));
 TestFalse(TEXT("Foreign executor cannot complete"), F.Story->FinishWorldEvent(Pipe->EventId, ETAWorldEventResult::Completed, OtherExecutor));
 TestTrue(TEXT("Cancellation releases reservation"), F.Story->FinishWorldEvent(Pipe->EventId, ETAWorldEventResult::Cancelled, Executor));
 TestTrue(TEXT("Cancelled event can retry"), F.Story->StartWorldEvent(Pipe, Actor, Executor));
 TestTrue(TEXT("Complete attack"), F.Story->FinishWorldEvent(Pipe->EventId, ETAWorldEventResult::Completed, Executor));
 TestEqual(TEXT("Morality applied only on completion"), F.Story->GetFlagInt(TEXT("Morality")), -1);
 TestFalse(TEXT("One shot blocks replay"), F.Story->StartWorldEvent(Pipe, Actor, Executor));
 TestFalse(TEXT("Committed branch excludes crane"), F.Story->CanStartWorldEvent(Crane, Actor));
 auto Save = F.Story->GetSaveData();
 F.Story->RestoreSaveData(FTAStorySaveData());
 TestTrue(TEXT("Reset releases completed branch"), F.Story->CanStartWorldEvent(Crane, Actor));
 TestTrue(TEXT("Start event before load"), F.Story->StartWorldEvent(Crane, Actor, Executor));
 F.Story->RestoreSaveData(Save);
 TestFalse(TEXT("Load cancels live event"), F.Story->IsWorldEventActive(Crane->EventId));
 TestEqual(TEXT("Branch persisted"), F.Story->GetWorldBranch(TEXT("Monster")), Pipe->EventId);
 TestTrue(TEXT("State persisted"), F.Story->GetWorldStateTags().HasTagExact(TAGameplayTags::Story_Chapter1_Attacked));
 TestFalse(TEXT("Once persisted"), F.Story->CanStartWorldEvent(Pipe, Actor));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAWorldConditionTest, "TheAwakening.Interaction.ConditionsAcquisitionRollback",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAWorldConditionTest::RunTest(const FString&)
{
 TAWorldInteractionTests::FFixture F;
 auto* Actor = F.World->SpawnActor<AActor>();
 auto* Door = NewObject<UTAInteractionBehaviorComponent>(Actor);
 Door->Conditions.RequiredTags.AddTag(TAGameplayTags::Story_Prologue_Diary);
 TestTrue(TEXT("Locked door remains selectable"), Door->IsSelectable(Actor));
 TestFalse(TEXT("Locked door refuses execution"), Door->TryExecute(Actor));
 Door->bAllowDeniedAttempt = false;
 TestFalse(TEXT("Hidden conditional interaction"), Door->IsSelectable(Actor));
 auto* Inventory = NewObject<UTAInventoryComponent>(Actor);
 auto* Diary = NewObject<UTAItemDefinition>();
 Diary->ItemType = ETAItemType::QuestItem; Diary->StorySlotIndex = 0;
 Diary->GrantedStoryTags.AddTag(TAGameplayTags::Story_Prologue_Diary);
 auto* Rollback = F.World->GetSubsystem<UTARollbackSubsystem>();
 auto Scope = Rollback->BeginScope(Actor, TEXT("Acquisition"));
 TestEqual(TEXT("Existing story-slot pickup succeeds"), Inventory->TryAddItem(Diary, 1), 1);
 TestTrue(TEXT("Pickup changes conditions immediately"), Door->MeetsConditions(Actor));
 TestTrue(TEXT("Door now selectable"), Door->IsSelectable(Actor));
 TestEqual(TEXT("Duplicate story pickup refused"), Inventory->TryAddItem(Diary, 1), 0);
 Rollback->RollbackAll();
 TestFalse(TEXT("Death rollback restores story condition"), Door->MeetsConditions(Actor));
 TestFalse(TEXT("Death rollback restores existing inventory"), Inventory->HasStoryItem(Diary));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASpecialActionInputTest, "TheAwakening.Interaction.SpecialActionEnhancedInput",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTASpecialActionInputTest::RunTest(const FString&)
{
 TAWorldInteractionTests::FFixture F;
 auto* PCClass = LoadClass<AThe_AwakeningPlayerController>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PawnClass = LoadClass<AThe_AwakeningCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
 auto* PC = F.World->SpawnActor<AThe_AwakeningPlayerController>(PCClass);
 auto* Pawn = F.World->SpawnActor<AThe_AwakeningCharacter>(PawnClass);
 auto* Local = NewObject<UTAInteractionTestLocalPlayer>(GEngine);
 Local->TestWorld = F.World; Local->PlayerController = PC; PC->Player = Local;
 PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
 F.GI->AddLocalPlayer(Local, FPlatformUserId::CreateFromInternalId(0));
 F.World->AddController(PC);
 if (PC->PlayerState) PC->PlayerState->Destroy();
 PC->PlayerState = F.World->SpawnActor<ATAPlayerState>();
 PC->Possess(Pawn);
 auto* Input = NewObject<UEnhancedInputComponent>(Pawn); Pawn->InputComponent = Input;
 auto* Actions = Pawn->FindComponentByClass<UTASpecialActionComponent>();
 Actions->BeginPlay();
 auto* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
 auto* Mash = NewObject<UInputAction>();
 auto* Pull = NewObject<UInputAction>();
 auto* Context = NewObject<UInputMappingContext>();
 Context->MapKey(Mash, EKeys::LeftMouseButton); Context->MapKey(Mash, EKeys::Gamepad_FaceButton_Bottom);
 Context->MapKey(Pull, EKeys::D);
 auto* Definition = NewObject<UTASpecialActionDefinition>();
 Definition->bAlign = false; Definition->MappingContext = Context;
 FTAQTEStage Stage; Stage.Kind = ETAQTEKind::Mash; Stage.Action = Mash; Stage.Required = 2;
 Definition->Stages.Add(Stage);
 Stage.Kind = ETAQTEKind::Press; Stage.Action = Pull; Stage.Required = 1; Definition->Stages.Add(Stage);
 auto* Target = F.World->SpawnActor<AActor>();
 TestTrue(TEXT("Special action starts"), Actions->Start(Definition, Target));
 FModifyContextOptions Options; Options.bForceImmediately = true; Options.bIgnoreAllPressedKeysUntilRelease = false;
 Subsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
 auto Send = [&](FKey Key, EInputEvent Event, float Delta = 0.016f)
 {
  PC->InputKey(FInputKeyEventArgs(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event, Event == IE_Released ? 0.f : 1.f, false, 0u));
  PC->PlayerInput->ProcessInputStack({Input}, Delta, false);
 };
 auto* Icons = F.GI->GetSubsystem<UTAInputIconSubsystem>();
 Icons->OnInputPromptsChanged.AddDynamic(Local, &UTAInteractionTestLocalPlayer::PromptChanged);
 Icons->SetCurrentDeviceType(EInputDeviceType::KeyboardMouse);
 TestTrue(TEXT("Prompt queries active keyboard mapping"), Icons->GetPromptKeysForAction(Mash, Local).Contains(EKeys::LeftMouseButton));
 TestNotNull(TEXT("Existing mouse icon is reused"), Icons->GetIconForActionForPlayer(Mash, Local));
 Icons->SetCurrentDeviceType(EInputDeviceType::Xbox);
 TestTrue(TEXT("Prompt queries active pad mapping"), Icons->GetPromptKeysForAction(Mash, Local).Contains(EKeys::Gamepad_FaceButton_Bottom));
 TestNotNull(TEXT("Existing gamepad icon is reused"), Icons->GetIconForActionForPlayer(Mash, Local));
 Icons->SetCurrentDeviceType(EInputDeviceType::KeyboardMouse);
 TestTrue(TEXT("Device changes broadcast prompt refresh"), Local->PromptRefreshes > 0);
 Context->UnmapAllKeysFromAction(Pull); Context->MapKey(Pull, EKeys::E);
 Subsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
 TestTrue(TEXT("Prompt follows remapped pull key"), Icons->GetPromptKeysForAction(Pull, Local).Contains(EKeys::E));
 TestFalse(TEXT("Prompt removes old pull key"), Icons->GetPromptKeysForAction(Pull, Local).Contains(EKeys::D));
 TestTrue(TEXT("Action owns exact permission"), Actions->HasInputPermission());
 TestFalse(TEXT("Normal interact blocked"), PC->AllowsInput(ETAInputCapability::Interact));
 Send(EKeys::D, IE_Pressed); Send(EKeys::D, IE_Released);
 TestEqual(TEXT("Wrong phase cannot pull early"), Actions->GetStageIndex(), 0);
 Send(EKeys::LeftMouseButton, IE_Pressed);
 TestEqual(TEXT("EI Started increments mash once"), Actions->GetProgress(), 0.5f);
 PC->PlayerInput->ProcessInputStack({Input}, 0.1f, false);
 TestEqual(TEXT("Holding is not a second mash"), Actions->GetProgress(), 0.5f);
 Actions->TickComponent(0.1f, LEVELTICK_All, nullptr);
 Send(EKeys::LeftMouseButton, IE_Released); Send(EKeys::LeftMouseButton, IE_Pressed);
 TestEqual(TEXT("Second real press advances"), Actions->GetStageIndex(), 1);
 Send(EKeys::D, IE_Pressed); Send(EKeys::D, IE_Released);
 TestTrue(TEXT("Old binding cannot complete action"), Actions->IsSpecialActionActive());
 Send(EKeys::E, IE_Pressed);
 TestFalse(TEXT("Final stage finishes"), Actions->IsSpecialActionActive());
 TestTrue(TEXT("Input restored"), PC->GetInputWinner().IsBase());
 TestFalse(TEXT("Temporary context removed"), Subsystem->HasMappingContext(Context));
 Send(EKeys::E, IE_Released); Send(EKeys::LeftMouseButton, IE_Released);
 TestTrue(TEXT("Can start again"), Actions->Start(Definition, Target));
 auto* UI = NewObject<UInputAction>(); FTAInputRequest Request; Request.Owner = UI; Request.Priority = 200;
 auto Handle = PC->AcquireInputRequest(Request);
 TestFalse(TEXT("Preempting UI cancels action"), Actions->IsSpecialActionActive());
 TestTrue(TEXT("Cleanup preserves UI winner"), PC->GetInputWinner().Handle == Handle);
 PC->ReleaseInputRequest(Handle);
 TestTrue(TEXT("Target destruction fixture starts"), Actions->Start(Definition, Target));
 Target->Destroy();
 TestFalse(TEXT("Target destruction cancels immediately"), Actions->IsSpecialActionActive());
 TestTrue(TEXT("No leaked ownership"), PC->GetInputWinner().IsBase());
 // Animation stages use the existing fallback executor and do not auto-award branch results.
 Target = F.World->SpawnActor<AActor>();
 auto* Animated = NewObject<UTASpecialActionDefinition>(); Animated->bAlign = false; Animated->MappingContext = Context;
 Stage = FTAQTEStage(); Stage.Kind = ETAQTEKind::Animation; Stage.AnimationTag = TAGameplayTags::Anim_GrabPipe;
 Animated->Stages.Add(Stage); Animated->Stages.Add(Definition->Stages[0]);
 TestTrue(TEXT("Animation stage starts"), Actions->Start(Animated, Target));
 F.World->GetSubsystem<UTATimeSubsystem>()->AdvanceGameplayTime(0.7f);
 TestEqual(TEXT("Existing animation fallback reaches QTE"), Actions->GetStageIndex(), 1);
 Actions->TickComponent(20.f, LEVELTICK_All, nullptr);
 TestFalse(TEXT("QTE timeout releases restrictions"), Actions->IsSpecialActionActive());
 TestTrue(TEXT("External loss fixture starts"), Actions->Start(Definition, Target));
 PC->NotifyApplicationActivationChanged(false); PC->RefreshInputOwnership();
 TestFalse(TEXT("Alt-tab cancels without retaining QTE state"), Actions->IsSpecialActionActive());
 PC->NotifyApplicationActivationChanged(true); PC->RefreshInputOwnership();
 auto* Hold = NewObject<UTASpecialActionDefinition>(); Hold->bAlign = false; Hold->MappingContext = Context;
 Stage = FTAQTEStage(); Stage.Kind = ETAQTEKind::Hold; Stage.Action = Mash; Stage.InputEvent = ETriggerEvent::Triggered; Stage.Required = 0.1f;
 Hold->Stages.Add(Stage);
 TestTrue(TEXT("Hold fixture starts"), Actions->Start(Hold, Target));
 Subsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
 Send(EKeys::LeftMouseButton, IE_Pressed, 0.04f);
 TestTrue(TEXT("Hold uses Enhanced Input elapsed time"), Actions->GetProgress() > 0.f && Actions->GetProgress() < 1.f);
 Send(EKeys::LeftMouseButton, IE_Released);
 TestEqual(TEXT("Release resets continuous hold"), Actions->GetProgress(), 0.f);
 Send(EKeys::LeftMouseButton, IE_Pressed, 0.06f);
 PC->PlayerInput->ProcessInputStack({Input}, 0.06f, false);
 TestFalse(TEXT("Continuous hold completes"), Actions->IsSpecialActionActive());
 Send(EKeys::LeftMouseButton, IE_Released);
 auto* DirectionAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
 Context->MapKey(DirectionAction, EKeys::D);
 Context->MapKey(DirectionAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(Context));
 auto* DirectionDefinition = NewObject<UTASpecialActionDefinition>(); DirectionDefinition->bAlign = false; DirectionDefinition->MappingContext = Context;
 Stage = FTAQTEStage(); Stage.Kind = ETAQTEKind::Direction; Stage.Action = DirectionAction; Stage.InputEvent = ETriggerEvent::Triggered;
 DirectionDefinition->Stages.Add(Stage);
 TestTrue(TEXT("Direction fixture starts"), Actions->Start(DirectionDefinition, Target));
 Subsystem->RequestRebuildControlMappings(Options, EInputMappingRebuildType::RebuildWithFlush);
 Send(EKeys::A, IE_Pressed);
 TestTrue(TEXT("Wrong direction cannot finish"), Actions->IsSpecialActionActive());
 Send(EKeys::A, IE_Released); Send(EKeys::D, IE_Pressed);
 TestFalse(TEXT("Existing 2D Move IA completes direction stage"), Actions->IsSpecialActionActive());
 Send(EKeys::D, IE_Released);
 TestTrue(TEXT("Death fixture starts"), Actions->Start(Definition, Target));
 UTADamageLibrary::ApplyDamage(Pawn, Pawn, 100000.f, ETADamageType::Forced, TAGameplayTags::Damage_Source_Console);
 TestFalse(TEXT("Death clears action, bindings and context"), Actions->IsSpecialActionActive());
 TestFalse(TEXT("Death removes temporary IMC"), Subsystem->HasMappingContext(Context));
 Icons->OnInputPromptsChanged.RemoveDynamic(Local, &UTAInteractionTestLocalPlayer::PromptChanged);
 F.GI->RemoveLocalPlayer(Local);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAInteractionSamplesTest, "TheAwakening.Interaction.SampleAssets",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAInteractionSamplesTest::RunTest(const FString&)
{
 auto* Class = LoadClass<ATAInteractableActor>(nullptr, TEXT("/Game/InteractionSamples/BP_ConditionalDoor.BP_ConditionalDoor_C"));
 if (!TestNotNull(TEXT("Conditional door blueprint loads"), Class)) return false;
 auto* Behavior = Class->GetDefaultObject<AActor>()->FindComponentByClass<UTAInteractionBehaviorComponent>();
 TestTrue(TEXT("Saved component template retains its condition"), Behavior && Behavior->Conditions.RequiredTags.HasTagExact(TAGameplayTags::Story_Prologue_Diary));
 auto* Action = LoadObject<UTASpecialActionDefinition>(nullptr, TEXT("/Game/InteractionSamples/DA_PipeAction.DA_PipeAction"));
 if (!TestNotNull(TEXT("Special action data asset loads"), Action)) return false;
 TestEqual(TEXT("Pipe has animation, mash, direction, animation"), Action->Stages.Num(), 4);
 auto* Move = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_MoveRight.IA_MoveRight"));
 TestTrue(TEXT("Direction reuses existing right movement IA"), Action->Stages.IsValidIndex(2) && Action->Stages[2].Action == Move);
 auto* Diary = LoadObject<UTAItemDefinition>(nullptr, TEXT("/Game/Document/DA_Item_Diary.DA_Item_Diary"));
 TestTrue(TEXT("Original diary grants acquisition state"), Diary && Diary->GrantedStoryTags.HasTagExact(TAGameplayTags::Story_Prologue_Diary));
 TAWorldInteractionTests::FFixture F;
 auto* Actor = F.World->SpawnActor<AActor>();
 auto* Inventory = NewObject<UTAInventoryComponent>(Actor);
 TestEqual(TEXT("Existing diary pickup"), Inventory->TryAddItem(Diary, 1), 1);
 auto Save = Inventory->GetSaveData();
 F.Story->RestoreSaveData(FTAStorySaveData());
 Inventory->RestoreSaveData(Save);
 TestTrue(TEXT("Old inventory save recovers diary acquisition tag"), F.Story->GetWorldStateTags().HasTagExact(TAGameplayTags::Story_Prologue_Diary));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAQTEPipeTest, "TheAwakening.Interaction.QTE.DebugPipe",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAQTEPipeTest::RunTest(const FString&)
{
 TAWorldInteractionTests::FFixture F;
 auto* PCClass=LoadClass<AThe_AwakeningPlayerController>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PawnClass=LoadClass<AThe_AwakeningCharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
 auto* PC=F.World->SpawnActor<AThe_AwakeningPlayerController>(PCClass);
 auto* Pawn=F.World->SpawnActor<AThe_AwakeningCharacter>(PawnClass);
 auto* Local=NewObject<UTAInteractionTestLocalPlayer>(GEngine);
 Local->TestWorld=F.World; Local->PlayerController=PC; PC->Player=Local;
 PC->PlayerInput=NewObject<UTAPlayerInput>(PC);
 F.GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0)); F.World->AddController(PC);
 if(PC->PlayerState) PC->PlayerState->Destroy(); PC->PlayerState=F.World->SpawnActor<ATAPlayerState>();
 PC->Possess(Pawn);
 auto* Input=NewObject<UEnhancedInputComponent>(Pawn); Pawn->InputComponent=Input;
 auto* Action=Pawn->FindComponentByClass<UTASpecialActionComponent>(); Action->BeginPlay(); Action->RandomSeed=37;
 // Supply TimeSubsystem's real frame delta; Tick delta can be slowed/frozen independently.
 auto Tick=[&](float RealDelta,float EffectiveScale=1.f)
 { F.World->DeltaRealTimeSeconds=RealDelta; Action->TickComponent(RealDelta*EffectiveScale,LEVELTICK_All,nullptr); };
 auto* Target=F.World->SpawnActor<ATADebugQTETester>(FVector(100,0,0),FRotator::ZeroRotator);
 auto* Definition=Target->QTESequence->ActionTemplate.LoadSynchronous();
 if(!TestNotNull(TEXT("Debug QTE data loads"),Definition)) { F.GI->RemoveLocalPlayer(Local); return false; }
 auto* Subsystem=Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
 FModifyContextOptions Options; Options.bForceImmediately=true; Options.bIgnoreAllPressedKeysUntilRelease=false;
 auto Rebuild=[&]{Subsystem->RequestRebuildControlMappings(Options,EInputMappingRebuildType::RebuildWithFlush);};
 auto Send=[&](FKey Key,EInputEvent Event)
 {
  PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,Event==IE_Released?0.f:1.f,false,0u));
  PC->PlayerInput->ProcessInputStack({Input},0.016f,false);
 };
 auto Press=[&](FKey Key){Tick(.1f); Send(Key,IE_Pressed); Send(Key,IE_Released);};
 auto* GameplayContext=NewObject<UInputMappingContext>();
 GameplayContext->MapKey(Definition->Stages[1].Action,EKeys::D);
 Subsystem->AddMappingContext(GameplayContext,0,Options); Rebuild();
 PC->RecordHeldInput(EKeys::D,1.f,Local->GetControllerId());
 TestTrue(TEXT("Fixture has live held movement mapping"),PC->ReadHeldAction(Definition->Stages[1].Action).IsNonZero());
 PC->RecordHeldInput(EKeys::D,0.f,Local->GetControllerId());
 Target->OnInteract_Implementation(Pawn); Rebuild();
 TestTrue(TEXT("Interaction starts QTE"),Action->IsSpecialActionActive());
 UInputAction* Selected=Action->GetPromptAction();
 TestTrue(TEXT("One random interaction IA chosen"),Definition->Stages[0].ActionOptions.Contains(Selected));
 FKey CorrectKey;
 for(const auto& Mapping:Definition->MappingContext->GetMappings())
  if(Mapping.Action==Selected && !Mapping.Key.IsGamepadKey()) { CorrectKey=Mapping.Key; break; }
 for(const auto& Mapping:Definition->MappingContext->GetMappings())
  if(Definition->Stages[0].ActionOptions.Contains(Mapping.Action) && Mapping.Action!=Selected && !Mapping.Key.IsGamepadKey()) Press(Mapping.Key);
 TestEqual(TEXT("Other interaction keys do nothing"),Action->GetProgress(),0.f);
 Press(CorrectKey); TestTrue(TEXT("Each press adds fifteen percent"),FMath::IsNearlyEqual(Action->GetProgress(),.15f));
 Send(CorrectKey,IE_Pressed); Send(CorrectKey,IE_Released);
 TestTrue(TEXT("Immediate auto-click is rejected by cooldown"),FMath::IsNearlyEqual(Action->GetProgress(),.15f));
 Tick(.05f);
 const float BeforeTooEarly=Action->GetProgress(); Send(CorrectKey,IE_Pressed); Send(CorrectKey,IE_Released);
 TestEqual(TEXT("Press inside 0.1s window is ignored"),Action->GetProgress(),BeforeTooEarly);
 Tick(.95f);
 TestTrue(TEXT("One second decays five percent"),FMath::IsNearlyEqual(Action->GetProgress(),.10f));
 TestTrue(TEXT("QTE templates opt into TimeSubsystem real-time policy"),Definition->bUseUnpausedRealTime);
 TestTrue(TEXT("Breathing amplitude halved to 0.1"),FMath::IsNearlyEqual(Definition->KeyPulseAmplitude,.1f));
 const float BeforeSlow=Action->GetProgress(), BeforeSlowTime=Action->GetStageElapsed();
 Tick(.2f,.01f);
 TestTrue(TEXT("Slow Tick cannot slow QTE decay"),FMath::IsNearlyEqual(Action->GetProgress(),BeforeSlow-.01f));
 TestTrue(TEXT("Slow Tick cannot slow QTE clock"),FMath::IsNearlyEqual(Action->GetStageElapsed(),BeforeSlowTime+.2f));
 auto* FreezeSource=NewObject<UInputAction>();
 auto* PlayerFreeze=Pawn->FindComponentByClass<UTAFreezeComponent>();
 F.World->GetSubsystem<UTAFreezeSubsystem>()->RegisterParticipant(PlayerFreeze);
 auto* QTETime=F.World->GetSubsystem<UTATimeSubsystem>();
 QTETime->RequestFreeze(FreezeSource,1.f);
 TestTrue(TEXT("Fixture truly freezes the player"),UTAFreezeComponent::IsActorFrozen(Pawn));
 const float BeforeFrozenTime=Action->GetStageElapsed(), BeforeFrozenProgress=Action->GetProgress();
 Tick(.2f,0.f);
 TestTrue(TEXT("Full Freeze neither cancels nor stops QTE clock"),Action->IsSpecialActionActive() && FMath::IsNearlyEqual(Action->GetStageElapsed(),BeforeFrozenTime+.2f));
 TestTrue(TEXT("Full Freeze preserves real-time decay"),FMath::IsNearlyEqual(Action->GetProgress(),BeforeFrozenProgress-.01f));
 Send(CorrectKey,IE_Pressed); Send(CorrectKey,IE_Released);
 TestTrue(TEXT("Enhanced Input QTE still accepts a press during Freeze"),Action->GetProgress()>BeforeFrozenProgress);
 QTETime->ReleaseFreeze(FreezeSource);
 F.World->GetSubsystem<UTAFreezeSubsystem>()->UnregisterParticipant(PlayerFreeze);
 TestFalse(TEXT("QTE blocks moving"),PC->AllowsInput(ETAInputCapability::Move));
 TestFalse(TEXT("QTE blocks parkour"),PC->AllowsInput(ETAInputCapability::Parkour));
 TestFalse(TEXT("QTE blocks inventory"),PC->AllowsInput(ETAInputCapability::InventoryToggle));
 AddExpectedMessage(TEXT("a special action/QTE is active"),EAutomationExpectedErrorFlags::Contains,2,false);
 auto* Save=F.GI->GetSubsystem<UTASaveSubsystem>();
 TestFalse(TEXT("Save refused before writing slot"),Save->SaveGame());
 auto* PauseOwner=NewObject<UInputAction>();
 FTAInputRequest Request; Request.Owner=PauseOwner; Request.Priority=500; Request.bSuspendSpecialAction=true;
 const auto PauseHandle=PC->AcquireInputRequest(Request);
 auto* Time=F.World->GetSubsystem<UTATimeSubsystem>(); Time->RequestPause(PauseOwner);
 TestTrue(TEXT("Pause overlay suspends instead of cancels"),Action->IsSpecialActionActive() && Action->IsSuspended());
 TestFalse(TEXT("Suspended QTE context cannot consume pause menu X/Y or navigation"),Subsystem->HasMappingContext(Definition->MappingContext));
 const float BeforePause=Action->GetProgress(); const float StageTime=Action->GetStageElapsed();
 Tick(10); Press(CorrectKey);
 TestEqual(TEXT("Pause preserves progress"),Action->GetProgress(),BeforePause);
 TestEqual(TEXT("Pause preserves stage clock"),Action->GetStageElapsed(),StageTime);
 TestFalse(TEXT("Saving also refused while QTE is paused"),Save->SaveGame());
 Time->ReleasePause(PauseOwner); PC->ReleaseInputRequest(PauseHandle); Rebuild();
 TestTrue(TEXT("Resume reinstalls QTE mappings"),Subsystem->HasMappingContext(Definition->MappingContext));
 TestTrue(TEXT("Resume restores same selected action"),Action->HasInputPermission() && !Action->IsSuspended() && Action->GetPromptAction()==Selected);
 auto* Widget=CreateWidget<UTAQTEWidget>(PC,UTAQTEWidget::StaticClass()); Widget->Observe(Action);
 TestNotNull(TEXT("QTE native UMG tree constructed without requiring viewport focus"),Widget->GetRootWidget());
 const auto Surface=Widget->TakeWidget();
 FWidgetRenderer* Renderer=GUsingNullRHI?nullptr:new FWidgetRenderer(true);
 if(Renderer)
 {
#if WITH_EDITOR
  FAssetCompilingManager::Get().FinishAllCompilation();
#endif
  if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
  Renderer->DrawWidget(Surface,FVector2D(256,256)); FlushRenderingCommands(); ++GFrameCounter;
 }
 bool bWarmedUp=true;
 auto Capture=[&](const TCHAR* Name)
 {
  if(GUsingNullRHI) return;
#if WITH_EDITOR
  FAssetCompilingManager::Get().FinishAllCompilation();
#endif
  if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
  ++GFrameCounter;
  Widget->NativeTick(FGeometry(),0);
  Widget->InvalidateLayoutAndVolatility();
  Surface->SlatePrepass(1.f);
  if (!bWarmedUp) { Renderer->DrawWidget(Surface,FVector2D(256,256)); FlushRenderingCommands(); ++GFrameCounter; bWarmedUp=true; }
  auto* RT=Renderer->DrawWidget(Surface,FVector2D(256,256));
  FlushRenderingCommands(); TArray<FColor> Pixels; RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
  if (FString(Name) != TEXT("Flip50")) TestTrue(TEXT("QTE frame contains rendered pixels"),Pixels.ContainsByPredicate([](FColor P){return P.R>20 || P.G>20 || P.B>20;}));
  TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(256,256,Pixels,PNG);
  UE_LOG(LogTemp,Display,TEXT("QTE render %s stage=%d progress=%f visibility=%d desired=%s"),Name,Action->GetStageIndex(),Action->GetProgress(),int32(Widget->GetVisibility()),*Widget->GetDesiredSize().ToString());
  Widget->WidgetTree->ForEachWidget([](UWidget* W)
  {
   if(auto* Image=Cast<UImage>(W)) UE_LOG(LogTemp,Display,TEXT("QTE image %s visibility=%d desired=%s material=%s"),*Image->GetName(),int32(Image->GetVisibility()),*Image->GetDesiredSize().ToString(),*GetNameSafe(Image->GetBrush().GetResourceObject()));
  });
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("QTE"); IFileManager::Get().MakeDirectory(*Directory,true);
  TestTrue(TEXT("QTE visual frame exported"),FFileHelper::SaveArrayToFile(PNG,*(Directory/(FString(Name)+TEXT(".png")))));
 };
 Press(CorrectKey); Press(CorrectKey); Capture(TEXT("Mash40"));
 for(int32 I=0;I<8 && Action->GetStageIndex()==0;++I) Press(CorrectKey);
 TestEqual(TEXT("Full ring advances to right direction stage"),Action->GetStageIndex(),1);
 TestFalse(TEXT("Filling mash alone cannot unlock"),Target->bLastSucceeded);
 Send(EKeys::D,IE_Pressed); Send(EKeys::D,IE_Released);
 TestTrue(TEXT("Unseen key cannot complete during flip"),Action->IsSpecialActionActive());
 Tick(Definition->KeyFlipDuration*.25f); Capture(TEXT("Flip25"));
 Tick(Definition->KeyFlipDuration*.25f); Capture(TEXT("Flip50"));
 Tick(Definition->KeyFlipDuration*.25f); Capture(TEXT("Flip75"));
 Tick(Definition->KeyFlipDuration*.3f); Capture(TEXT("RightReady"));
 Press(EKeys::W); Press(EKeys::Left); Press(EKeys::Down);
 TestTrue(TEXT("Other directions do not unlock"),Action->IsSpecialActionActive());
 PC->RecordHeldInput(EKeys::D,1.f,Local->GetControllerId());
 Send(EKeys::D,IE_Pressed);
 TestTrue(TEXT("Success starts exit buffer"),Action->IsExitBufferActive());
 TestFalse(TEXT("Exit buffer still blocks movement"),PC->AllowsInput(ETAInputCapability::Move));
 TestFalse(TEXT("Still-held final D cannot drive held gameplay input"),PC->ReadHeldAction(Definition->Stages[1].Action).IsNonZero());
 Tick(.21f);
 TestFalse(TEXT("Exit buffer expires"),Action->IsExitBufferActive());
 TestTrue(TEXT("Movement permission restored after buffer"),PC->AllowsInput(ETAInputCapability::Move));
 TestFalse(TEXT("Held D remains consumed after buffer expires"),PC->ReadHeldAction(Definition->Stages[1].Action).IsNonZero());
 PC->RecordHeldInput(EKeys::D,0.f,Local->GetControllerId()); Send(EKeys::D,IE_Released);
 PC->RecordHeldInput(EKeys::D,1.f,Local->GetControllerId());
 TestTrue(TEXT("New D press after release drives gameplay normally"),PC->ReadHeldAction(Definition->Stages[1].Action).IsNonZero());
 PC->RecordHeldInput(EKeys::D,0.f,Local->GetControllerId());
 TestTrue(TEXT("Right arrow unlocks the same direction as D"),Target->bLastSucceeded && !Action->IsSpecialActionActive());
 TestEqual(TEXT("Success emitted once"),Target->SuccessfulRuns,1);
 Tick(.21f);
 // Gamepad pool accepts only the selected X/Y action and then the fixed right D-pad direction.
 Target->OnInteract_Implementation(Pawn); Rebuild();
 FKey PadKey, WrongPad;
 for(const auto& Mapping:Definition->MappingContext->GetMappings())
  if(Mapping.Action==Action->GetPromptAction() && Mapping.Key.IsGamepadKey()) { PadKey=Mapping.Key; break; }
 for(const auto& Mapping:Definition->MappingContext->GetMappings())
  if(Definition->Stages[0].ActionOptions.Contains(Mapping.Action) && Mapping.Key.IsGamepadKey() && Mapping.Key!=PadKey) { WrongPad=Mapping.Key; break; }
 Press(WrongPad); TestEqual(TEXT("Unselected gamepad face button is ignored"),Action->GetProgress(),0.f);
 for(int32 I=0;I<9 && Action->GetStageIndex()==0;++I) Press(PadKey);
 TestEqual(TEXT("Selected gamepad face button fills mash"),Action->GetStageIndex(),1);
 Tick(.4f);
 Press(EKeys::Gamepad_DPad_Left); TestTrue(TEXT("Wrong D-pad direction ignored"),Action->IsSpecialActionActive());
 Press(EKeys::Gamepad_DPad_Right); TestTrue(TEXT("D-pad right unlocks"),Target->bLastSucceeded);
 Tick(.21f);
 // Exposed instance tuning overrides a private run copy, keeping the shared asset untouched.
 Target->QTESequence->Steps[0].ProgressPerClick=.2f; Target->QTESequence->Steps[0].DecayPerSecond=.1f; Target->QTESequence->Steps[0].ClickCooldownSeconds=.25f;
 Target->OnInteract_Implementation(Pawn); Rebuild();
 for(const auto& Mapping:Definition->MappingContext->GetMappings())
  if(Mapping.Action==Action->GetPromptAction() && !Mapping.Key.IsGamepadKey()) { CorrectKey=Mapping.Key; break; }
 Send(CorrectKey,IE_Pressed); Send(CorrectKey,IE_Released);
 TestTrue(TEXT("Tuned click gain applies"),FMath::IsNearlyEqual(Action->GetProgress(),.2f));
 Tick(.1f); Send(CorrectKey,IE_Pressed); Send(CorrectKey,IE_Released);
 TestTrue(TEXT("Tuned cooldown rejects early click while tuned decay continues"),FMath::IsNearlyEqual(Action->GetProgress(),.19f));
 Tick(.15f); Send(CorrectKey,IE_Pressed); Send(CorrectKey,IE_Released);
 TestTrue(TEXT("Click at tuned interval is accepted"),FMath::IsNearlyEqual(Action->GetProgress(),.375f));
 TestTrue(TEXT("Shared asset keeps its default gain"),FMath::IsNearlyEqual(Definition->Stages[0].ProgressPerPress,.15f));
 Action->Cancel(); Target->QTESequence->Steps[0].ProgressPerClick=.15f; Target->QTESequence->Steps[0].DecayPerSecond=.05f; Target->QTESequence->Steps[0].ClickCooldownSeconds=.1f;
 // Three stages: fixed up single press, fixed left mash with independent tuning, interaction single press.
 auto* Sequence = Target->QTESequence.Get();
 Sequence->OnQTEEnded.AddDynamic(Local,&UTAInteractionTestLocalPlayer::QTEEnded);
 Sequence->Steps.SetNum(3);
 Sequence->Steps[0].KeyGroup=ETAQTEKeyGroup::Direction; Sequence->Steps[0].Direction=ETAQTEDirection::Up;
 Sequence->Steps[0].PressMode=ETAQTEPressMode::SinglePress;
 Sequence->Steps[0].PromptOffset=FVector2D(70,-35);
 Sequence->Steps[1].KeyGroup=ETAQTEKeyGroup::Direction; Sequence->Steps[1].Direction=ETAQTEDirection::Left;
 Sequence->Steps[1].PressMode=ETAQTEPressMode::Mash; Sequence->Steps[1].ProgressPerClick=.6f;
 Sequence->Steps[1].DecayPerSecond=0.f; Sequence->Steps[1].ClickCooldownSeconds=.3f;
 Sequence->Steps[1].PromptOffset=FVector2D(-40,55);
 Sequence->Steps[2].KeyGroup=ETAQTEKeyGroup::Interaction; Sequence->Steps[2].PressMode=ETAQTEPressMode::SinglePress;
 Target->OnInteract_Implementation(Pawn); Rebuild();
 TestEqual(TEXT("Editable sequence creates all three runtime stages"),Action->GetDefinition()->Stages.Num(),3);
 Widget->Refresh();
 TestTrue(TEXT("First step offsets whole QTE relative to its existing anchor"),Widget->GetRenderTransform().Translation.Equals(FVector2D(70,-35)));
 TestTrue(TEXT("Flip uses 0.2 seconds"),FMath::IsNearlyEqual(Action->GetDefinition()->KeyFlipDuration,.2f));
 Press(EKeys::D); TestEqual(TEXT("Wrong configured direction ignored"),Sequence->GetCurrentStepIndex(),0);
 Press(EKeys::Up); TestEqual(TEXT("Configured up accepts its arrow alias"),Sequence->GetCurrentStepIndex(),1);
 TestTrue(TEXT("Next step replaces rather than accumulates prompt offset"),Widget->GetRenderTransform().Translation.Equals(FVector2D(-40,55)));
 Tick(.21f); Press(EKeys::A);
 TestTrue(TEXT("Scene component exposes current mash progress readonly"),FMath::IsNearlyEqual(Sequence->GetMashProgress(),.6f));
 Press(EKeys::Left); TestEqual(TEXT("Each stage has its own cooldown"),Sequence->GetCurrentStepIndex(),1);
 Tick(.21f); Press(EKeys::Left);
 TestEqual(TEXT("Directional mash advances to interaction press"),Sequence->GetCurrentStepIndex(),2);
 TestTrue(TEXT("Zero offset step returns QTE to anchor"),Widget->GetRenderTransform().Translation.IsNearlyZero());
 Tick(.21f);
 for(const auto& Mapping:Definition->MappingContext->GetMappings())
  if(Mapping.Action==Action->GetPromptAction() && !Mapping.Key.IsGamepadKey()) { CorrectKey=Mapping.Key; break; }
 Press(CorrectKey);
 TestEqual(TEXT("Bindable component end event emitted exactly once"),Local->QTEEndCount,1);
 TestTrue(TEXT("End event reports success"),Local->bLastQTESucceeded);
 Tick(.21f);
 Target->OnInteract_Implementation(Pawn); Action->Cancel();
 TestEqual(TEXT("Cancellation also emits end event once"),Local->QTEEndCount,2);
 TestFalse(TEXT("Cancellation event reports failure"),Local->bLastQTESucceeded);
 Sequence->OnQTEEnded.RemoveDynamic(Local,&UTAInteractionTestLocalPlayer::QTEEnded);
 // The Blueprint-facing function takes private step copies, including per-step Create Event delegates.
 FTAQTEStepCallback Accepted, Completed;
 Accepted.BindDynamic(Local,&UTAInteractionTestLocalPlayer::QTEAccepted);
 Completed.BindDynamic(Local,&UTAInteractionTestLocalPlayer::QTECompleted);
 FTAQTEFinishedCallback Finished; Finished.BindDynamic(Local,&UTAInteractionTestLocalPlayer::QTEEnded);
 Local->CallbackSequence=Sequence;
 TArray<FTAQTESequenceStep> RequestedSteps;
 RequestedSteps.Add(UTAQTESequenceComponent::MakeQTEStep(ETAQTEKeyGroup::Direction,ETAQTEDirection::Right,ETAQTEPressMode::Mash,Accepted,Completed,FTAQTEStepCallback(),.5f,0.f,.2f,false,5.f,FVector2D(80,-20)));
 RequestedSteps.Add(UTAQTESequenceComponent::MakeQTEStep(ETAQTEKeyGroup::Direction,ETAQTEDirection::Left,ETAQTEPressMode::SinglePress,Accepted,Completed,FTAQTEStepCallback()));
 TestTrue(TEXT("Generate QTE starts supplied steps"),Sequence->GenerateQTE(Pawn,RequestedSteps,Finished)); Rebuild();
 TestTrue(TEXT("Make QTE Step offset reaches runtime and widget"),Action->GetCurrentStage()->PromptOffset.Equals(FVector2D(80,-20)) && Widget->GetRenderTransform().Translation.Equals(FVector2D(80,-20)));
 TestEqual(TEXT("Generate does not overwrite editable component defaults"),Sequence->Steps.Num(),3);
 RequestedSteps[0].OnAcceptedInput.Unbind(); RequestedSteps[0].Direction=ETAQTEDirection::Down; RequestedSteps[0].PromptOffset=FVector2D(999,999);
 TestTrue(TEXT("Running offset is isolated from caller edits"),Action->GetCurrentStage()->PromptOffset.Equals(FVector2D(80,-20)));
 TestFalse(TEXT("Rejected new request cannot replace current callbacks"),Sequence->GenerateQTE(Pawn,{},Finished));
 Press(EKeys::Up); TestEqual(TEXT("Wrong key emits no accepted event"),Local->AcceptedSteps.Num(),0);
 Press(EKeys::D);
 TestEqual(TEXT("First accepted click invokes its own callback"),Local->AcceptedSteps.Num(),1);
 TestTrue(TEXT("Callback receives normalized progress"),FMath::IsNearlyEqual(Local->AcceptedProgress[0],.5f));
 Send(EKeys::D,IE_Pressed); Send(EKeys::D,IE_Released);
 TestEqual(TEXT("Cooldown click emits no event"),Local->AcceptedSteps.Num(),1);
 Tick(.21f); Press(EKeys::D);
 TestEqual(TEXT("Mash completion event fires once"),Local->CompletedSteps.Num(),1);
 TestEqual(TEXT("Completion identifies the first step"),Local->CompletedSteps[0],0);
 Tick(.21f); Press(EKeys::Left);
 TestEqual(TEXT("Next key invokes its own completion event"),Local->CompletedSteps.Num(),2);
 TestEqual(TEXT("Completion identifies the second step"),Local->CompletedSteps[1],1);
 TestTrue(TEXT("Final callback succeeds once"),Local->QTEEndCount==3 && Local->bLastQTESucceeded);
 Tick(.21f);
 RequestedSteps.SetNum(1); RequestedSteps[0].Direction=ETAQTEDirection::Right; RequestedSteps[0].OnAcceptedInput=Accepted;
 Local->bCancelOnAccepted=true;
 TestTrue(TEXT("Callback cancellation run starts"),Sequence->GenerateQTE(Pawn,RequestedSteps,Finished)); Rebuild(); Press(EKeys::D);
 TestFalse(TEXT("Accepted callback may safely cancel the action"),Sequence->IsQTEActive());
 TestEqual(TEXT("Cancel from callback never completes the step"),Local->CompletedSteps.Num(),2);
 TestTrue(TEXT("Cancel callback reports failure once"),Local->QTEEndCount==4 && !Local->bLastQTESucceeded);
 Local->bCancelOnAccepted=false; Local->bCancelOnCompleted=true;
 RequestedSteps[0].PressMode=ETAQTEPressMode::SinglePress;
 TestTrue(TEXT("Completion callback cancellation run starts"),Sequence->GenerateQTE(Pawn,RequestedSteps,Finished)); Rebuild(); Press(EKeys::D);
 TestTrue(TEXT("Cancelling in completion prevents final success and stale advancement"),Local->QTEEndCount==5 && !Local->bLastQTESucceeded && !Sequence->IsQTEActive());
 Local->bCancelOnCompleted=false;
 // Per-step optional deadline aborts the whole sequence, with input already cleaned up.
 FTAQTEStepCallback TimedOut; TimedOut.BindDynamic(Local,&UTAInteractionTestLocalPlayer::QTETimedOut);
 const int32 BeforeTimeoutEnds=Local->QTEEndCount, BeforeTimeoutCompletions=Local->CompletedSteps.Num();
 RequestedSteps.SetNum(2);
 RequestedSteps[0]=UTAQTESequenceComponent::MakeQTEStep(ETAQTEKeyGroup::Direction,ETAQTEDirection::Right,ETAQTEPressMode::SinglePress,Accepted,Completed,TimedOut,.15f,.05f,.1f,true,.5f);
 RequestedSteps[1]=RequestedSteps[0]; RequestedSteps[1].Direction=ETAQTEDirection::Left;
 TestTrue(TEXT("Timed sequence starts"),Sequence->GenerateQTE(Pawn,RequestedSteps,Finished)); Rebuild();
 TestTrue(TEXT("Step deadline is copied into runtime action"),FMath::IsNearlyEqual(Action->GetCurrentStage()->Timeout,.5f));
 Tick(.51f);
 TestTrue(TEXT("Timeout stops entire QTE before the next step"),!Sequence->IsQTEActive() && Local->TimedOutSteps.Num()==1 && Local->TimedOutSteps[0]==0);
 TestFalse(TEXT("Timeout callback sees an already stopped action"),Local->bWasActiveDuringTimeout);
 TestEqual(TEXT("Timeout cannot emit step completion"),Local->CompletedSteps.Num(),BeforeTimeoutCompletions);
 TestTrue(TEXT("Timeout reports final failure exactly once"),Local->QTEEndCount==BeforeTimeoutEnds+1 && !Local->bLastQTESucceeded);
 TestTrue(TEXT("Timeout releases its input request"),PC->AllowsInput(ETAInputCapability::Move));
 RequestedSteps[0].bTimeLimited=false; RequestedSteps[0].TimeLimitSeconds=.01f;
 TestTrue(TEXT("Unlimited step ignores its time field"),Sequence->GenerateQTE(Pawn,RequestedSteps,Finished)); Rebuild(); Tick(2.f);
 TestTrue(TEXT("Disabled deadline never times out"),Sequence->IsQTEActive() && Local->TimedOutSteps.Num()==1);
 Action->Cancel();
 RequestedSteps[0].bTimeLimited=true; RequestedSteps[0].TimeLimitSeconds=0.f;
 TestFalse(TEXT("Enabled zero deadline rejects invalid configuration"),Sequence->GenerateQTE(Pawn,RequestedSteps,Finished));
 RequestedSteps[0].bTimeLimited=false;
 TestTrue(TEXT("Second-stage timeout fixture starts"),Sequence->GenerateQTE(Pawn,RequestedSteps,Finished)); Rebuild();
 Press(EKeys::D); TestEqual(TEXT("First completed stage reaches next step"),Sequence->GetCurrentStepIndex(),1);
 Tick(.51f);
 TestTrue(TEXT("Only the current second stage's timeout fires"),Local->TimedOutSteps.Num()==2 && Local->TimedOutSteps[1]==1 && !Sequence->IsQTEActive());
 Sequence->Steps.SetNum(2); Sequence->Steps[0]=FTAQTESequenceStep(); Sequence->Steps[1]=FTAQTESequenceStep();
 Sequence->Steps[1].KeyGroup=ETAQTEKeyGroup::Direction; Sequence->Steps[1].PressMode=ETAQTEPressMode::SinglePress;
 // A repeated interaction rerolls at run start, not at each frame or device swap.
 TSet<UInputAction*> Choices;
 for(int32 Seed=0;Seed<100;++Seed)
 {
  Action->RandomSeed=Seed; Target->OnInteract_Implementation(Pawn); Choices.Add(Action->GetPromptAction()); Action->Cancel();
 }
 TestTrue(TEXT("Random runs can select different interaction actions"),Choices.Num()>1);
 UInputAction* QAction=LoadObject<UInputAction>(nullptr,TEXT("/Game/DeubgItem/QTE/IA_QTE_Q.IA_QTE_Q"));
 TestTrue(TEXT("Q action is in the random pool"),QAction && Definition->Stages[0].ActionOptions.Contains(QAction) && Choices.Contains(QAction));
 for(int32 Seed=0;Seed<100;++Seed)
 {
  Action->RandomSeed=Seed; Target->OnInteract_Implementation(Pawn);
  if(Action->GetPromptAction()==QAction)
  {
   Rebuild(); auto* QIcons=F.GI->GetSubsystem<UTAInputIconSubsystem>();
   QIcons->SetCurrentDeviceType(EInputDeviceType::KeyboardMouse);
   TestTrue(TEXT("Q prompt resolves its actual IMC mapping"),QIcons->GetPromptKeysForAction(QAction,Local).Contains(EKeys::Q));
   TestNotNull(TEXT("Q uses the existing Keys icon"),QIcons->GetIconForActionForPlayer(QAction,Local));
   Press(EKeys::E); TestEqual(TEXT("Q-only stage rejects E"),Action->GetProgress(),0.f);
   Press(EKeys::Q); TestTrue(TEXT("Q progresses via Enhanced Input"),FMath::IsNearlyEqual(Action->GetProgress(),.15f));
   Action->Cancel(); break;
  }
  Action->Cancel();
 }
 // Death must preserve both generic end notifications without emitting either timeout notification.
 RequestedSteps[0].bTimeLimited=true; RequestedSteps[0].TimeLimitSeconds=.5f;
 const int32 BeforeDeathEnds=Local->QTEEndCount, BeforeDeathTimeouts=Local->TimedOutSteps.Num();
 const int32 BeforeDeathCompletions=Local->CompletedSteps.Num();
 Sequence->OnQTEEnded.AddDynamic(Local,&UTAInteractionTestLocalPlayer::QTEBroadcastEnded);
 Sequence->OnStepTimedOut.AddDynamic(Local,&UTAInteractionTestLocalPlayer::QTETimedOut);
 TestTrue(TEXT("Timed death fixture starts"),Sequence->GenerateQTE(Pawn,RequestedSteps,Finished)); Rebuild();
 UTADamageLibrary::ApplyDamage(Pawn,Pawn,100000,ETADamageType::Forced,TAGameplayTags::Damage_Source_Console);
 TestFalse(TEXT("Death cancels QTE without success"),Action->IsSpecialActionActive() || Target->bLastSucceeded);
 TestFalse(TEXT("Death removes QTE context"),Subsystem->HasMappingContext(Definition->MappingContext));
 Tick(1.f); Sequence->CancelQTE();
 TestFalse(TEXT("Death leaves no running sequence"),Sequence->IsQTEActive());
 TestTrue(TEXT("Death sends OnFinished(false) exactly once"),Local->QTEEndCount==BeforeDeathEnds+1 && !Local->bLastQTESucceeded);
 TestTrue(TEXT("Death sends OnQTEEnded(false) exactly once"),Local->QTEBroadcastEndCount==1 && !Local->bLastQTEBroadcastSucceeded);
 TestEqual(TEXT("Death never emits step or component timeout events, even after the deadline"),Local->TimedOutSteps.Num(),BeforeDeathTimeouts);
 TestEqual(TEXT("Death never completes or advances a step"),Local->CompletedSteps.Num(),BeforeDeathCompletions);
 if(Renderer) BeginCleanup(Renderer);
 Widget->NativeDestruct(); F.GI->RemoveLocalPlayer(Local);
 return true;
}
#endif
