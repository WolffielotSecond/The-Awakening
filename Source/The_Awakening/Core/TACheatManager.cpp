#include "Core/TACheatManager.h"
#include "Core/TAGameplayTags.h"
#include "Health/TADamageLibrary.h"
#include "Health/TAHealthComponent.h"
#include "Health/TARespawnSubsystem.h"
#include "Health/TACheckpoint.h"
#include "AbilitySystem/TAAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogTACheat, Log, All);

UAbilitySystemComponent* UTACheatManager::GetPlayerASC() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	return PC ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
}

UTAHealthComponent* UTACheatManager::GetPlayerHealth() const
{
	const APlayerController* PC = GetOuterAPlayerController();
	return PC ? UTAHealthComponent::FindHealthComponent(PC->GetPawn()) : nullptr;
}

void UTACheatManager::ApplyConsoleDamage(float Amount, ETADamageType Type, const TCHAR* Command)
{
	APlayerController* PC = GetOuterAPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UAbilitySystemComponent* ASC = GetPlayerASC();
	if (!Pawn || !ASC)
	{
		UE_LOG(LogTACheat, Warning, TEXT("%s: no controlled pawn with an AbilitySystemComponent."), Command);
		return;
	}
	const float Before = ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute());
	const bool bApplied = UTADamageLibrary::ApplyDamage(Pawn, Pawn, Amount, Type, TAGameplayTags::Damage_Source_Console);
	const float After = ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute());
	UE_LOG(LogTACheat, Log, TEXT("%s %.2f: applied=%d Health %.2f -> %.2f"), Command, Amount, bApplied ? 1 : 0, Before, After);
}

void UTACheatManager::TADamage(float Amount)
{
	ApplyConsoleDamage(Amount, ETADamageType::Normal, TEXT("TADamage"));
}

void UTACheatManager::TATrueDamage(float Amount)
{
	ApplyConsoleDamage(Amount, ETADamageType::TrueDamage, TEXT("TATrueDamage"));
}

void UTACheatManager::TAKill()
{
	const UAbilitySystemComponent* ASC = GetPlayerASC();
	const float Health = ASC ? ASC->GetNumericAttribute(UTAAttributeSet::GetHealthAttribute()) : 0.f;
	ApplyConsoleDamage(Health, ETADamageType::Forced, TEXT("TAKill"));
}

void UTACheatManager::TACheckpoint(FName CheckpointId)
{
	UTARespawnSubsystem* Respawn = UTARespawnSubsystem::Get(GetOuterAPlayerController());
	ATACheckpoint* Checkpoint = Respawn ? Respawn->FindCheckpoint(CheckpointId) : nullptr;
	if (!Checkpoint)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TACheckpoint: no checkpoint with id '%s'."), *CheckpointId.ToString());
		return;
	}
	Respawn->ActivateCheckpoint(Checkpoint);
	UE_LOG(LogTACheat, Log, TEXT("TACheckpoint: active checkpoint is now %s (%s)."), *CheckpointId.ToString(), *Checkpoint->GetName());
}

void UTACheatManager::TAGod()
{
	if (GodProtection.IsValid())
	{
		if (UTAHealthComponent* OldHealth = GodHealth.Get())
		{
			OldHealth->RemoveProtection(GodProtection);
		}
		GodProtection = FTAHealthProtectionHandle();
		GodHealth.Reset();
		UE_LOG(LogTACheat, Log, TEXT("TAGod: off"));
		return;
	}

	UTAHealthComponent* Health = GetPlayerHealth();
	if (!Health)
	{
		UE_LOG(LogTACheat, Warning, TEXT("TAGod: no controlled pawn with a health component."));
		return;
	}
	GodProtection = Health->AddProtection(ETAHealthProtection::Invulnerable, this, FGameplayTagContainer());
	GodHealth = Health;
	UE_LOG(LogTACheat, Log, TEXT("TAGod: on"));
}
