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
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"

/** Scoped input setup for isolated parkour tests, with real resolved mappings. */
struct FTAParkourInputTestFixture
{
	static void Register(UTAParkourComponent* Component, ATAParkourMarker* Marker)
	{
		auto* Pawn = CastChecked<ACharacter>(Component->GetOwner());
		Marker->TriggerBox->BeginComponentOverlap(FOverlapInfo(Pawn->GetCapsuleComponent(), INDEX_NONE), false);
		Component->RegisterMarker(Marker);
	}
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
	UInputAction* Forward = nullptr;
	UInputAction* Backward = nullptr;
	UInputAction* Left = nullptr;
	UInputAction* Right = nullptr;
	UInputAction* Stick = nullptr;
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
		Context->MapKey(Jump, EKeys::SpaceBar); Context->MapKey(Jump, EKeys::Gamepad_FaceButton_Bottom);
		Forward = NewObject<UInputAction>(PC); Backward = NewObject<UInputAction>(PC);
		Left = NewObject<UInputAction>(PC); Right = NewObject<UInputAction>(PC);
		Stick = NewObject<UInputAction>(PC); Stick->ValueType = EInputActionValueType::Axis2D;
		Context->MapKey(Forward, EKeys::W); Context->MapKey(Backward, EKeys::S);
		Context->MapKey(Left, EKeys::A); Context->MapKey(Right, EKeys::D);
		Context->MapKey(Stick, EKeys::Gamepad_Left2D);
		Parkour->ConfigureDirectionActions(Forward, Backward, Left, Right, Stick);
		FModifyContextOptions Options; Options.bForceImmediately = true;
		Mappings.AddMappingContext(Context, 0, Options);
	}
	void Observe(FKey Key, float Value) { PC->RecordHeldInput(Key, Value, User); }
	void Poll() { Parkour->UpdatePlayerHeldRequests(PC, Jump, Drop); }
	void Update(bool bJump, bool bDrop)
	{
		Observe(EKeys::D, 1.f);
		Observe(EKeys::SpaceBar, bJump || bDrop ? 1.f : 0.f);
		Poll();
	}
};
#endif
