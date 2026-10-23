#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "The_AwakeningCharacter.h"
#include "The_AwakeningPlayerController.h"
#include "Settings/TASettingsSubsystem.h"
#include "Core/TAPlayerInput.h"
#include "EnhancedInputSubsystemInterface.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTASprintModeTest,"TheAwakening.Input.SprintModes",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTASprintModeTest::RunTest(const FString&)
{
 const FString Filename=FPaths::ProjectSavedDir()/TEXT("Automation/SprintModes-Test.ini");
 FFileHelper::SaveStringToFile(TEXT(""),*Filename); GConfig->LoadFile(Filename);
 TGuardValue<FString> ConfigFile(GGameUserSettingsIni,Filename);
 ON_SCOPE_EXIT { GConfig->UnloadFile(Filename); IFileManager::Get().Delete(*Filename); };
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 ON_SCOPE_EXIT { World->DestroyWorld(false); };
 auto* PawnClass=LoadClass<AThe_AwakeningCharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
 auto* Context=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/Input/IMC_Default.IMC_Default"));
 auto* PCClass=LoadClass<AThe_AwakeningPlayerController>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 if (!TestNotNull(TEXT("Character blueprint"),PawnClass) || !TestNotNull(TEXT("Mappings"),Context) || !TestNotNull(TEXT("Controller blueprint"),PCClass)) return false;
 auto* PC=World->SpawnActor<AThe_AwakeningPlayerController>(PCClass);
 auto* Pawn=World->SpawnActor<AThe_AwakeningCharacter>(PawnClass);
 if (!TestNotNull(TEXT("Spawn controller"),PC) || !TestNotNull(TEXT("Spawn character"),Pawn)) return false;
 auto* Local=NewObject<ULocalPlayer>(GEngine); Local->PlayerController=PC; PC->Player=Local;
 PC->PlayerInput=NewObject<UTAPlayerInput>(PC); PC->Possess(Pawn);
 struct FMappings : IEnhancedInputSubsystemInterface
 {
  UEnhancedPlayerInput* Input=nullptr;
  TMap<TObjectPtr<const UInputAction>,FInjectedInput> Injected;
  UEnhancedPlayerInput* GetPlayerInput() const override { return Input; }
  TMap<TObjectPtr<const UInputAction>,FInjectedInput>& GetContinuouslyInjectedInputs() override { return Injected; }
 } Mappings;
 Mappings.Input=Cast<UTAPlayerInput>(PC->PlayerInput);
 FModifyContextOptions Options; Options.bForceImmediately=true; Mappings.AddMappingContext(Context,0,Options);
 auto* Settings=NewObject<UTASettingsSubsystem>(Local);
 auto* Definitions=NewObject<UTASettingsDefinitionAsset>(); Definitions->PopulateDefaultDefinitions();
 TestTrue(TEXT("Use settings definitions"),Settings->UseDefinitionAsset(Definitions));
 const FName Id(TEXT("Game.ToggleSprint"));
 TestFalse(TEXT("Hold is default"),Settings->GetBoolean(Id));
 auto* Movement=Pawn->GetCharacterMovement(); Movement->SetMovementMode(MOVE_Flying);
 const int32 User=Local->GetControllerId();
 auto Key=[&](FKey K,bool Down){PC->RecordHeldInput(K,Down?1.f:0.f,User);};
 auto Tick=[&](){static_cast<AActor*>(Pawn)->Tick(.016f); Pawn->ConsumeMovementInputVector();};
 auto Speed=[&](const TCHAR* Message,float Expected){TestEqual(Message,Movement->MaxWalkSpeed,Expected);};
 Key(EKeys::W,true); Key(EKeys::LeftShift,true); Tick(); Speed(TEXT("Hold sprints"),750.f);
 FTAInputRequest Menu; Menu.Owner=Settings; Menu.Priority=500;
 Menu.Allowed={ETAInputCapability::Navigate};
 auto MenuHandle=PC->AcquireInputRequest(Menu);
 Settings->BeginEditing(); Settings->SetDraftValue(Id,1);
 TestFalse(TEXT("Draft does not change applied mode"),Settings->GetBoolean(Id));
 TestTrue(TEXT("Apply Hold to Toggle during menu without character ticking"),Settings->ApplyDraft());
 Key(EKeys::LeftShift,false); PC->ReleaseInputRequest(MenuHandle); Tick();
 Speed(TEXT("Hold to Toggle retains sprint after release"),750.f);
 Key(EKeys::LeftShift,true); Tick(); Speed(TEXT("Second press cancels toggle"),500.f);
 Tick(); Speed(TEXT("Holding does not toggle repeatedly"),500.f);
 Key(EKeys::LeftShift,false); Tick(); Key(EKeys::Gamepad_LeftThumbstick,true); Tick();
 Speed(TEXT("L3 toggles sprint"),750.f); Key(EKeys::Gamepad_LeftThumbstick,false); Tick();
 Key(EKeys::W,false); Movement->Velocity=FVector(750,0,999); Tick();
 Speed(TEXT("At threshold retains sprint"),750.f);
 Key(EKeys::W,true); Tick(); Speed(TEXT("Resume before slowing retains sprint"),750.f);
 Key(EKeys::W,false); Movement->Velocity=FVector(750,0,0); Tick();
 Movement->Velocity=FVector(749,0,999); Key(EKeys::W,true); Tick();
 Speed(TEXT("First resumed frame below threshold walks; vertical speed ignored"),500.f);
 Key(EKeys::LeftShift,true); Tick(); Key(EKeys::LeftShift,false); Tick();
 Key(EKeys::W,false); Movement->Velocity=FVector(700,0,0); Tick();
 Key(EKeys::W,true); Tick(); Speed(TEXT("Below threshold during release clears latch"),500.f);
 Key(EKeys::LeftShift,true); Tick(); Speed(TEXT("Toggle on again"),750.f);
 MenuHandle=PC->AcquireInputRequest(Menu); Settings->SetDraftValue(Id,0); Settings->ApplyDraft();
 PC->ReleaseInputRequest(MenuHandle); Tick(); Speed(TEXT("Toggle to Hold suppresses already held button"),500.f);
 Key(EKeys::LeftShift,false); Key(EKeys::LeftShift,true); Tick(); Speed(TEXT("Fresh hold restores sprint"),750.f);
 Key(EKeys::LeftShift,false); Tick(); Speed(TEXT("Hold release walks"),500.f);
 Settings->SetDraftValue(Id,1); Settings->ApplyDraft();
 MenuHandle=PC->AcquireInputRequest(Menu); Key(EKeys::LeftShift,true);
 PC->ReleaseInputRequest(MenuHandle); Tick(); Speed(TEXT("Menu press cannot toggle after closing paused menu"),500.f);
 Key(EKeys::LeftShift,false); Tick(); Key(EKeys::LeftShift,true); Tick(); Speed(TEXT("Fresh gameplay press toggles"),750.f);
 PC->NotifyApplicationActivationChanged(false); PC->NotifyApplicationActivationChanged(true); Tick();
 Speed(TEXT("Focus loss clears toggle"),500.f);
 Settings->SetDraftValue(Id,0); Settings->DiscardDraft(); Tick();
 TestTrue(TEXT("Discard keeps applied toggle mode"),Settings->GetBoolean(Id));
 Settings->RestoreSettingDefault(Id); Tick(); TestFalse(TEXT("Restore defaults returns to Hold"),Settings->GetBoolean(Id));
 Settings->SetValue(Id,1); Key(EKeys::W,true);
 Key(EKeys::LeftShift,true); Key(EKeys::LeftShift,false); Tick();
 Speed(TEXT("Fast tap released between ticks toggles sprint"),750.f);
 Key(EKeys::LeftShift,true); Key(EKeys::LeftShift,false);
 Key(EKeys::LeftShift,true); Key(EKeys::LeftShift,false); Tick();
 Speed(TEXT("Two taps between ticks preserve toggle parity"),750.f);
 Key(EKeys::LeftShift,true); Key(EKeys::LeftShift,false); Tick();
 Speed(TEXT("Fast second tap cancels sprint"),500.f);
 return true;
}
#endif
