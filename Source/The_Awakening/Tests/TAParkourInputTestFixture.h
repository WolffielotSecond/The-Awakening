#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "The_AwakeningPlayerController.h"
#include "Core/TAPlayerInput.h"
#include "Movement/TAParkourComponent.h"
#include "EnhancedInputSubsystemInterface.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"

/** Scoped input setup for isolated parkour tests, with real resolved mappings. */
struct FTAParkourInputTestFixture
{
	struct FMappingSubsystem : IEnhancedInputSubsystemInterface
	{
		UEnhancedPlayerInput* Input = nullptr;
		TMap<TObjectPtr<const UInputAction>, FInjectedInput> Injected;
		virtual UEnhancedPlayerInput* GetPlayerInput() const override { return Input; }
		virtual TMap<TObjectPtr<const UInputAction>, FInjectedInput>& GetContinuouslyInjectedInputs() override { return Injected; }
	} Mappings;
	AThe_AwakeningPlayerController* PC = nullptr;
	UInputAction* Jump = nullptr;
	UInputAction* Drop = nullptr;
	UInputMappingContext* Context = nullptr;
	UTAParkourComponent* Parkour = nullptr;
	int32 User = 0;

	FTAParkourInputTestFixture(ACharacter* Pawn, UTAParkourComponent* InParkour) : Parkour(InParkour)
	{
		auto* World = Pawn->GetWorld();
		auto* Class = LoadClass<AThe_AwakeningPlayerController>(nullptr,
			TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
		PC = Class ? World->SpawnActor<AThe_AwakeningPlayerController>(Class) : nullptr;
		if (!PC) return;
		auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
		User = Local->GetControllerId();
		World->AddController(PC); PC->Possess(Pawn);
		PC->PlayerInput = NewObject<UTAPlayerInput>(PC);
		Mappings.Input = CastChecked<UTAPlayerInput>(PC->PlayerInput);
		Jump = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_ParkourJump.IA_ParkourJump"));
		Drop = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_ParkourDrop.IA_ParkourDrop"));
		Context = NewObject<UInputMappingContext>(PC);
		Context->MapKey(Jump, EKeys::SpaceBar); Context->MapKey(Jump, EKeys::Gamepad_FaceButton_Right);
		Context->MapKey(Drop, EKeys::LeftControl); Context->MapKey(Drop, EKeys::Gamepad_FaceButton_Bottom);
		FModifyContextOptions Options; Options.bForceImmediately = true;
		Mappings.AddMappingContext(Context, 0, Options);
	}
	void Observe(FKey Key, float Value) { PC->RecordHeldInput(Key, Value, User); }
	void Poll() { Parkour->UpdatePlayerHeldRequests(PC, Jump, Drop); }
	void Update(bool bJump, bool bDrop)
	{
		Observe(EKeys::SpaceBar, bJump ? 1.f : 0.f);
		Observe(EKeys::LeftControl, bDrop ? 1.f : 0.f);
		Poll();
	}
};
#endif
