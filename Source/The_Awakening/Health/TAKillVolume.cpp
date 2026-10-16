#include "Health/TAKillVolume.h"
#include "Health/TADamageLibrary.h"
#include "Health/TAHealthComponent.h"
#include "Core/TAGameplayTags.h"
#include "Components/BoxComponent.h"

ATAKillVolume::ATAKillVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	DamageSource = TAGameplayTags::Damage_Source_Environment;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	RootComponent = Volume;
	Volume->InitBoxExtent(FVector(500.f, 500.f, 100.f));
	Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
	Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Volume->SetGenerateOverlapEvents(true);
	Volume->ShapeColor = FColor::Red;
	Volume->OnComponentBeginOverlap.AddDynamic(this, &ATAKillVolume::HandleBeginOverlap);
}

void ATAKillVolume::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	// 只处理能死亡复活的对象；其他物体掉进来不管
	if (UTAHealthComponent::FindHealthComponent(OtherActor))
	{
		UTADamageLibrary::ApplyDamage(this, OtherActor, Damage, ETADamageType::Forced, DamageSource);
	}
}
