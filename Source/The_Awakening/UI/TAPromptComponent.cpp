#include "UI/TAPromptComponent.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "Interaction/TAInteractionQuery.h"
#include "Interaction/TASpecialActionComponent.h"
#include "Interaction/TAInteractionBehaviorComponent.h"
#include "The_AwakeningCharacter.h"
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

AActor* UTAPromptComponent::ResolveInteractionTarget(bool bRefresh)
{
 const auto* Pawn = OwnerPawn.IsValid() ? OwnerPawn.Get() : Cast<APawn>(GetOwner());
 if (!Pawn) return nullptr;
 if (!bRefresh && QueryFrame == GFrameCounter && QueryOrigin == Pawn->GetActorLocation() && QueryRadius == PromptRadius && !CachedInteractTarget.IsStale()) return CachedInteractTarget.Get();
 AActor* Result = FTAInteractionQuery::FindBestInteractable(Pawn, PromptRadius);
 if (bRefresh) { CachedInteractTarget = Result; QueryFrame = GFrameCounter; QueryOrigin = Pawn->GetActorLocation(); QueryRadius = PromptRadius; }
 return Result;
}
void UTAPromptComponent::CollectInteractCandidate(TMap<ETAPromptType, FTAPromptCandidate>& Best) const
{
	const APawn* Pawn = OwnerPawn.Get();
	const auto* PC = Pawn ? Cast<AThe_AwakeningPlayerController>(Pawn->GetController()) : nullptr;
 if (const auto* Action = Pawn ? Pawn->FindComponentByClass<UTASpecialActionComponent>() : nullptr;
     Action && Action->IsSpecialActionActive() && (Action->HasInputPermission() || Action->IsSuspended()))
 {
  FTAPromptCandidate Candidate;
  Candidate.Source = Action->GetTarget();
  Candidate.Anchor = UTAInteractionPromptAnchor::FindAnchor(Action->GetTarget(), ETAPromptType::Interact);
  Candidate.Action = Action->GetPromptAction();
  Candidate.TextOverride = Action->GetPromptTextId();
  if (Candidate.Anchor.IsValid()) Best.Add(Candidate.Type, Candidate);
  return;
 }
	if (!PC || !PC->AllowsInput(ETAInputCapability::Interact))
	{
		return;
	}
	// 执行时交互键优先交给拾取列表（TryPickupSelected），这时交互提示也让给拾取列表
	if (const auto* Pickup = Pawn->FindComponentByClass<UTANearbyPickupComponent>(); Pickup && Pickup->HasItems())
	{
		return;
	}
	AActor* Target = const_cast<UTAPromptComponent*>(this)->ResolveInteractionTarget(false);
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
 if (const auto* Behavior = Target->FindComponentByClass<UTAInteractionBehaviorComponent>(); Behavior && !Behavior->MeetsConditions(const_cast<APawn*>(Pawn))) Candidate.TextOverride = Behavior->DeniedPromptTextId;
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
	bool Conflict = false;
	const FVector2D Intent = Parkour->ReadDirectionalIntent(Conflict);
	auto* Marker = Parkour->SelectDirectionalMarker(FMath::Sign(Intent.X));
	if (!Marker) return;
	Marker->UpdateLandingTargets(Pawn->GetActorLocation());
	const int32 Side = Marker->GetLandingSide(Pawn->GetActorLocation(), Parkour->HorizontalTolerance);
	const bool Dual = Marker->bEnableUpper && Marker->bEnableLower;
	for (bool Lower : {false, true})
	{
		if (!Marker->HasValidLanding(Lower)) continue;
		FTAPromptCandidate Candidate;
		Candidate.Type = Lower ? ETAPromptType::ParkourDrop : ETAPromptType::ParkourJump;
		Candidate.Source = Marker;
		Candidate.Action = ParkourJumpAction;
		Candidate.Anchor = Marker->GetLandingPrompt(Lower);
		Candidate.HorizontalDirection = Side;
		Candidate.VerticalDirection = Dual ? (Lower ? -1 : 1) : 0;
		if (Candidate.Anchor.IsValid()) Best.Add(Candidate.Type, Candidate);
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
		if (!Old || Old->Anchor != Pair.Value.Anchor || Old->Action != Pair.Value.Action || Old->TextOverride != Pair.Value.TextOverride || Old->HorizontalDirection != Pair.Value.HorizontalDirection || Old->VerticalDirection != Pair.Value.VerticalDirection)
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
 if (auto* Action = OwnerPawn.IsValid() ? OwnerPawn->FindComponentByClass<UTASpecialActionComponent>() : nullptr;
     Action && Action->UsesQTEPresentation() && Candidate.Source.Get() == Action->GetTarget())
 { Anchor->ShowQTE(Action); return; }
	const FString TextId = Candidate.TextOverride.IsEmpty() ? Anchor->ResolveTextId() : Candidate.TextOverride;
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
			if (Candidate.HorizontalDirection)
			{
				auto* Parkour = Pawn ? Pawn->FindComponentByClass<UTAParkourComponent>() : nullptr;
				auto* Local = PC ? PC->GetLocalPlayer() : nullptr;
				TArray<UTexture2D*> Chord;
				TArray<FText> Labels;
				FText DirectionSuffix;
				auto AddAction = [&](UInputAction* Action)
				{
					const auto Keys = Icons->GetPromptKeysForAction(Action, Local);
					Chord.Add(Icons->GetIconForActionForPlayer(Action, Local));
					Labels.Add(Keys.IsEmpty() ? FText::FromString(TEXT("?")) : Keys[0].GetDisplayName());
				};
				if (Parkour && Icons->GetCurrentDeviceType() == EInputDeviceType::KeyboardMouse)
				{
					if (Candidate.VerticalDirection) AddAction(Parkour->GetDirectionAction(0, Candidate.VerticalDirection));
					AddAction(Parkour->GetDirectionAction(Candidate.HorizontalDirection));
				}
				else if (Parkour)
				{
					const auto Keys = Icons->GetPromptKeysForAction(Parkour->GetStickAction(), Local);
					const bool LeftStick = Keys.Contains(EKeys::Gamepad_Left2D) || Keys.Contains(EKeys::Gamepad_LeftX);
					Chord.Add(LeftStick ? Icons->GetStickDirectionIcon(Candidate.HorizontalDirection, Candidate.VerticalDirection) : Icons->GetIconForActionForPlayer(Parkour->GetStickAction(), Local));
					Labels.Add(Keys.IsEmpty() ? FText::FromString(TEXT("?")) : Keys[0].GetDisplayName());
					// Placeholder textures share LS artwork: an explicit arrow keeps diagonal direction readable.
					const TCHAR* Arrow = Candidate.VerticalDirection > 0 ? (Candidate.HorizontalDirection > 0 ? TEXT("↗") : TEXT("↖")) :
						Candidate.VerticalDirection < 0 ? (Candidate.HorizontalDirection > 0 ? TEXT("↘") : TEXT("↙")) :
						(Candidate.HorizontalDirection > 0 ? TEXT("→") : TEXT("←"));
					DirectionSuffix = FText::FromString(Arrow);
				}
				AddAction(Candidate.Action);
				Anchor->ShowPromptChord(Chord, Labels, Text, DirectionSuffix);
				return;
			}
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
