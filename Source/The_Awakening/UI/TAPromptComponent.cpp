#include "UI/TAPromptComponent.h"
#include "UI/TAPromptTypes.h"
#include "Interaction/TAInteractable.h"
#include "Interaction/TAInteractableActor.h"
#include "Movement/TAParkourComponent.h"
#include "Movement/TAParkourMarker.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Inventory/TAWorldItem.h"

UTAPromptComponent::UTAPromptComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTAPromptComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerPawn = Cast<APawn>(GetOwner());
	if (GetOwner()) AddTickPrerequisiteActor(GetOwner());
}

void UTAPromptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePrompts();
}

void UTAPromptComponent::UpdatePrompts()
{
	if (!OwnerPawn.IsValid())
	{
		return;
	}

	TArray<FTAPromptCandidate> Candidates;
	CollectCandidates(Candidates);

	TMap<ETAPromptType, FTAPromptCandidate> Best;
	for (const FTAPromptCandidate& C : Candidates)
	{
		if (const FTAPromptCandidate* Existing = Best.Find(C.Type))
		{
			if (C.DistanceSq < Existing->DistanceSq)
			{
				Best.Add(C.Type, C);
			}
		}
		else
		{
			Best.Add(C.Type, C);
		}
	}

	ApplyWinners(Best);
}

void UTAPromptComponent::CollectCandidates(TArray<FTAPromptCandidate>& OutCandidates)
{
	CollectInteractCandidates(OutCandidates);
	CollectParkourCandidates(OutCandidates);
}

void UTAPromptComponent::CollectInteractCandidates(TArray<FTAPromptCandidate>& OutCandidates)
{
	UWorld* World = GetWorld();
	if (!World || !OwnerPawn.IsValid())
	{
		return;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PromptInteract), false, OwnerPawn.Get());

	const bool bHit = World->OverlapMultiByChannel(
		Overlaps,
		OwnerPawn->GetActorLocation(),
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(PromptRadius),
		Params);

	if (!bHit)
	{
		return;
	}

	const FVector PawnLoc = OwnerPawn->GetActorLocation();

	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Actor = Result.GetActor();
		if (!Actor || !Actor->Implements<UTAInteractable>())
		{
			continue;
		}
		if (!ITAInteractable::Execute_CanInteract(Actor, OwnerPawn.Get()))
		{
			continue;
		}

		FTAPromptCandidate C;
		C.Type = ETAPromptType::Interact;
		C.Source = Actor;
		C.WorldLocation = Actor->GetActorLocation();
		C.DistanceSq = FVector::DistSquared(PawnLoc, C.WorldLocation);
		C.Action = InteractAction;

		if (ATAWorldItem* WorldItem = Cast<ATAWorldItem>(Actor))
		{
			C.TextId = WorldItem->GetPromptTextId();
			C.PromptWidget = WorldItem->GetPromptWidgetComponent();
		}
		else if (ATAInteractableActor* IA = Cast<ATAInteractableActor>(Actor))
		{
			C.TextId = IA->GetPromptTextId();
			C.PromptWidget = IA->GetPromptWidgetComponent();
		}
		else
		{
			C.TextId = InteractTextId;
		}

		if (C.TextId.IsEmpty())
		{
			C.TextId = InteractTextId;
		}

		if (C.PromptWidget.IsValid())
		{
			OutCandidates.Add(C);
		}
	}
}

void UTAPromptComponent::CollectParkourCandidates(TArray<FTAPromptCandidate>& OutCandidates)
{
	if (!OwnerPawn.IsValid())
	{
		return;
	}

	UTAParkourComponent* Parkour = OwnerPawn->FindComponentByClass<UTAParkourComponent>();
	if (!Parkour)
	{
		return;
	}

	const FVector PawnLoc = OwnerPawn->GetActorLocation();

	// 需要 ParkourComponent 提供 OverlappingMarkers 访问，见下方「配套修改」
	for (ATAParkourMarker* Marker : Parkour->GetOverlappingMarkers())
	{
		if (!Parkour->CanPlayerParkourToMarker(Marker))
		{
			continue;
		}

		FTAPromptCandidate C;
		C.Source = Marker;
		C.WorldLocation = Marker->GetActorLocation();
		C.DistanceSq = FVector::DistSquared(PawnLoc, C.WorldLocation);
		C.PromptWidget = Marker->GetPromptWidgetComponent();

		switch (Marker->MarkerType)
		{
		case ETAParkourMarkerType::JumpToPoint:
			C.Type = ETAPromptType::ParkourJump;
			C.Action = ParkourJumpAction;
			C.TextId = ParkourJumpTextId;
			break;
		case ETAParkourMarkerType::DropDown:
			C.Type = ETAPromptType::ParkourDrop;
			C.Action = ParkourDropAction;
			C.TextId = ParkourDropTextId;
			break;
		default:
			continue;
		}

		if (C.PromptWidget.IsValid())
		{
			OutCandidates.Add(C);
		}
	}
}

void UTAPromptComponent::ApplyWinners(const TMap<ETAPromptType, FTAPromptCandidate>& Best)
{
	for (const TPair<ETAPromptType, FTAPromptCandidate>& Pair : ActivePrompts)
	{
		const FTAPromptCandidate* NewBest = Best.Find(Pair.Key);
		const bool bStillWinner = NewBest
			&& NewBest->Source.Get() == Pair.Value.Source.Get();

		if (!bStillWinner)
		{
			HideCandidate(Pair.Value);
		}
	}

	TMap<ETAPromptType, FTAPromptCandidate> NewActive;

	for (const TPair<ETAPromptType, FTAPromptCandidate>& Pair : Best)
	{
		const FTAPromptCandidate* Old = ActivePrompts.Find(Pair.Key);
		const bool bSame = Old && Old->Source.Get() == Pair.Value.Source.Get();

		if (!bSame)
		{
			ShowCandidate(Pair.Value);
		}
		else
		{
			// 同一目标也可定期刷新（可选）
			RefreshCandidate(Pair.Value);
		}
		NewActive.Add(Pair.Key, Pair.Value);
	}

	ActivePrompts = MoveTemp(NewActive);
	static const ETAPromptType StackOrder[] = {
		ETAPromptType::Interact,
		ETAPromptType::ParkourJump,
		ETAPromptType::ParkourDrop
	};

	int32 StackIndex = 0;
	float BaseZ = 100.f;
	constexpr float Spacing = 55.f;

	for (ETAPromptType Type : StackOrder)
	{
		if (FTAPromptCandidate* C = ActivePrompts.Find(Type))
		{
			if (UWidgetComponent* WC = C->PromptWidget.Get())
			{

				if (StackIndex == 0)
				{

					//根据交互？跑酷？类型不同设置相对位置
					switch (Type)
					{
					case ETAPromptType::ParkourJump:
					case ETAPromptType::ParkourDrop:
						BaseZ = ParkourPromptZ;
						break;

					case ETAPromptType::Interact:
						BaseZ = InteractPromptZ;
						break;

					default:
						BaseZ = InteractPromptZ;
						break;
					}
				}


				WC->SetRelativeLocation(FVector(0.f, 0.f, BaseZ + StackIndex * Spacing));
				++StackIndex;
			}
		}
	}
}

void UTAPromptComponent::ShowCandidate(const FTAPromptCandidate& Candidate)
{
	if (UWidgetComponent* WidgetComp = Candidate.PromptWidget.Get())
	{
		WidgetComp->SetVisibility(true);
		if (!WidgetComp->GetUserWidgetObject() && WidgetComp->GetWidgetClass())
		{
			WidgetComp->InitWidget();
		}
	}
	RefreshCandidate(Candidate);
}

void UTAPromptComponent::HideCandidate(const FTAPromptCandidate& Candidate)
{
	if (UWidgetComponent* Widget = Candidate.PromptWidget.Get())
	{
		Widget->SetVisibility(false);
	}
}

void UTAPromptComponent::RefreshCandidate(const FTAPromptCandidate& Candidate)
{
	AActor* Source = Candidate.Source.Get();
	if (!Source)
	{
		return;
	}

	FText PromptText = FText::FromString(Candidate.TextId);
	UTexture2D* Icon = nullptr;

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
			{
				PromptText = Loc->GetText(Candidate.TextId);
			}
			if (UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
			{
				if (Candidate.Action)
				{
					Icon = Icons->GetIconForAction(Candidate.Action);
				}
			}
		}
	}

	if (ATAWorldItem* WorldItem = Cast<ATAWorldItem>(Source))
	{
		WorldItem->RefreshPrompt(Icon, PromptText);
		return;
	}

	if (ATAInteractableActor* IA = Cast<ATAInteractableActor>(Source))
	{
		IA->RefreshPrompt(Icon, PromptText);
		return;
	}

	if (ATAParkourMarker* Marker = Cast<ATAParkourMarker>(Source))
	{
		Marker->RefreshPrompt(Icon, PromptText);
	}
}

void UTAPromptComponent::RefreshVisiblePrompts()
{
	for (const TPair<ETAPromptType, FTAPromptCandidate>& Pair : ActivePrompts)
	{
		RefreshCandidate(Pair.Value);
	}
}

FString UTAPromptComponent::GetDefaultTextId(ETAPromptType Type) const
{
	switch (Type)
	{
	case ETAPromptType::ParkourJump: return ParkourJumpTextId;
	case ETAPromptType::ParkourDrop: return ParkourDropTextId;
	default: return InteractTextId;
	}
}

UInputAction* UTAPromptComponent::GetActionForType(ETAPromptType Type) const
{
	switch (Type)
	{
	case ETAPromptType::ParkourJump: return ParkourJumpAction;
	case ETAPromptType::ParkourDrop: return ParkourDropAction;
	default: return InteractAction;
	}
}
