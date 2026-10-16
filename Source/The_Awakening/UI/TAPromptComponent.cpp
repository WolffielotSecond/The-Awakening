#include "UI/TAPromptComponent.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "Interaction/TAInteractionQuery.h"
#include "Movement/TAParkourComponent.h"
#include "Movement/TAParkourMarker.h"
#include "Inventory/TANearbyPickupComponent.h"
#include "Core/TAFreezeComponent.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "The_AwakeningPlayerController.h"

UTAPromptComponent::UTAPromptComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 冻结时也要继续 Tick，才能把提示隐藏掉（否则会停在冻结前的状态）
	ComponentTags.AddUnique(TEXT("FreezeExempt"));
}

void UTAPromptComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerPawn = Cast<APawn>(GetOwner());
	if (GetOwner()) AddTickPrerequisiteActor(GetOwner());
}

void UTAPromptComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideAll();
	Super::EndPlay(EndPlayReason);
}

void UTAPromptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePrompts();
}

void UTAPromptComponent::UpdatePrompts()
{
	TMap<ETAPromptType, FTAPromptCandidate> Best;
	// 完全冻结时执行会被拒绝，提示同样不显示
	if (OwnerPawn.IsValid() && !UTAFreezeComponent::IsActorFrozen(OwnerPawn.Get()))
	{
		CollectInteractCandidate(Best);
		CollectParkourCandidates(Best);
	}
	ApplyWinners(Best);
}

void UTAPromptComponent::CollectInteractCandidate(TMap<ETAPromptType, FTAPromptCandidate>& Best) const
{
	const APawn* Pawn = OwnerPawn.Get();
	const auto* PC = Pawn ? Cast<AThe_AwakeningPlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->AllowsInput(ETAInputCapability::Interact))
	{
		return;
	}
	// 执行时交互键优先交给拾取列表（TryPickupSelected），这时交互提示也让给拾取列表
	if (const auto* Pickup = Pawn->FindComponentByClass<UTANearbyPickupComponent>(); Pickup && Pickup->HasItems())
	{
		return;
	}
	AActor* Target = FTAInteractionQuery::FindBestInteractable(Pawn, PromptRadius);
	UTAInteractionPromptAnchor* Anchor = UTAInteractionPromptAnchor::FindAnchor(Target, ETAPromptType::Interact);
	if (!Anchor)
	{
		return;
	}
	FTAPromptCandidate Candidate;
	Candidate.Type = ETAPromptType::Interact;
	Candidate.Source = Target;
	Candidate.Action = InteractAction;
	Candidate.Anchor = Anchor;
	Best.Add(Candidate.Type, Candidate);
}

void UTAPromptComponent::CollectParkourCandidates(TMap<ETAPromptType, FTAPromptCandidate>& Best) const
{
	const APawn* Pawn = OwnerPawn.Get();
	UTAParkourComponent* Parkour = Pawn ? Pawn->FindComponentByClass<UTAParkourComponent>() : nullptr;
	if (!Parkour)
	{
		return;
	}
	const FVector PawnLocation = Pawn->GetActorLocation();
	// 跨越与跳下分别处理：各自取离角色最近、且与执行同一套条件（CanPlayerParkourToMarker）判定可跑酷的标记
	for (ATAParkourMarker* Marker : Parkour->GetOverlappingMarkers())
	{
		if (!IsValid(Marker) || !Parkour->CanPlayerParkourToMarker(Marker))
		{
			continue;
		}
		const ETAPromptType Type = Marker->GetPromptType();
		UTAInteractionPromptAnchor* Anchor = Marker->GetPromptAnchor();
		if (!Anchor)
		{
			continue;
		}
		const float DistanceSq = FVector::DistSquared(PawnLocation, Marker->GetActorLocation());
		if (const FTAPromptCandidate* Existing = Best.Find(Type); Existing && Existing->DistanceSq <= DistanceSq)
		{
			continue;
		}
		FTAPromptCandidate Candidate;
		Candidate.Type = Type;
		Candidate.Source = Marker;
		Candidate.DistanceSq = DistanceSq;
		Candidate.Action = Type == ETAPromptType::ParkourDrop ? ParkourDropAction : ParkourJumpAction;
		Candidate.Anchor = Anchor;
		Best.Add(Type, Candidate);
	}
}

void UTAPromptComponent::ApplyWinners(const TMap<ETAPromptType, FTAPromptCandidate>& Best)
{
	for (const TPair<ETAPromptType, FTAPromptCandidate>& Pair : ActivePrompts)
	{
		const FTAPromptCandidate* NewBest = Best.Find(Pair.Key);
		if (!NewBest || NewBest->Anchor != Pair.Value.Anchor)
		{
			if (UTAInteractionPromptAnchor* Anchor = Pair.Value.Anchor.Get()) Anchor->HidePrompt();
		}
	}
	TMap<ETAPromptType, FTAPromptCandidate> NewActive;
	for (const TPair<ETAPromptType, FTAPromptCandidate>& Pair : Best)
	{
		const FTAPromptCandidate* Old = ActivePrompts.Find(Pair.Key);
		// 只在新出现时刷新内容；同一目标持续显示不重复查图标和本地化
		if (!Old || Old->Anchor != Pair.Value.Anchor)
		{
			ShowCandidate(Pair.Value);
		}
		NewActive.Add(Pair.Key, Pair.Value);
	}
	ActivePrompts = MoveTemp(NewActive);
}

void UTAPromptComponent::ShowCandidate(const FTAPromptCandidate& Candidate)
{
	UTAInteractionPromptAnchor* Anchor = Candidate.Anchor.Get();
	if (!Anchor)
	{
		return;
	}
	const FString TextId = Anchor->ResolveTextId();
	FText Text = FText::FromString(TextId);
	UTexture2D* Icon = nullptr;
	if (const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Text = Loc->GetText(TextId);
		}
		if (const UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>(); Icons && Candidate.Action)
		{
			// 读实际键位绑定，随输入设备变化
			const APawn* Pawn = OwnerPawn.Get();
			const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
			Icon = Icons->GetIconForActionForPlayer(Candidate.Action, PC ? PC->GetLocalPlayer() : nullptr);
		}
	}
	Anchor->ShowPrompt(Icon, Text);
}

void UTAPromptComponent::RefreshVisiblePrompts()
{
	for (const TPair<ETAPromptType, FTAPromptCandidate>& Pair : ActivePrompts)
	{
		ShowCandidate(Pair.Value);
	}
}

void UTAPromptComponent::HideAll()
{
	for (const TPair<ETAPromptType, FTAPromptCandidate>& Pair : ActivePrompts)
	{
		if (UTAInteractionPromptAnchor* Anchor = Pair.Value.Anchor.Get()) Anchor->HidePrompt();
	}
	ActivePrompts.Reset();
}

AActor* UTAPromptComponent::GetShownSource(ETAPromptType Type) const
{
	const FTAPromptCandidate* Candidate = ActivePrompts.Find(Type);
	return Candidate ? Candidate->Source.Get() : nullptr;
}
