#include "Debug/TADebugQTETester.h"
#include "Interaction/TASpecialActionComponent.h"
#include "Interaction/TAQTESequenceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UI/TAInteractionPromptAnchor.h"
#include "UObject/ConstructorHelpers.h"
ATADebugQTETester::ATADebugQTETester()
{
 QTESequence = CreateDefaultSubobject<UTAQTESequenceComponent>(TEXT("QTESequence"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 MeshComponent->SetStaticMesh(Cube.Object);
 MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
 MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
 Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
 Label->SetupAttachment(RootComponent); Label->SetRelativeLocation(FVector(0,0,80));
 Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(16);
 Label->SetText(FText::FromString(TEXT("TA_Debug_QTE_Tester")));
 InteractPromptComponent->SetRelativeLocation(FVector(0,0,160));
}
bool ATADebugQTETester::CanInteract_Implementation(AActor* Interactor) const
{
 return Super::CanInteract_Implementation(Interactor) && !RunningAction.IsValid() && !(Interactor && Interactor->FindComponentByClass<UTASpecialActionComponent>() && Interactor->FindComponentByClass<UTASpecialActionComponent>()->IsExitBufferActive());
}
void ATADebugQTETester::OnInteract_Implementation(AActor* Interactor)
{
 if (!CanInteract_Implementation(Interactor)) return;
 auto* Action = Interactor->FindComponentByClass<UTASpecialActionComponent>();
 if (!Action) return;
 RunningAction = Action;
 Action->OnEnded.AddUniqueDynamic(this, &ATADebugQTETester::HandleEnded);
 bLastSucceeded = false;
 if (!QTESequence->StartQTE(Interactor))
 { Action->OnEnded.RemoveDynamic(this, &ATADebugQTETester::HandleEnded); RunningAction.Reset(); return; }
 Label->SetText(FText::FromString(TEXT("QTE RUNNING"))); Label->SetTextRenderColor(FColor::Yellow);
}
void ATADebugQTETester::HandleEnded(AActor* Target, bool bSuccess)
{
 if (Target != this) return;
 if (RunningAction.IsValid()) RunningAction->OnEnded.RemoveDynamic(this, &ATADebugQTETester::HandleEnded);
 RunningAction.Reset(); bLastSucceeded = bSuccess;
 if (bSuccess) ++SuccessfulRuns;
 Label->SetText(FText::FromString(bSuccess ? TEXT("QTE SUCCESS - repeat interaction") : TEXT("QTE CANCELLED - retry interaction")));
 Label->SetTextRenderColor(bSuccess ? FColor::Green : FColor::Red);
 OnTestEnded.Broadcast(bSuccess);
}
void ATADebugQTETester::EndPlay(const EEndPlayReason::Type Reason)
{
 if (RunningAction.IsValid()) { RunningAction->OnEnded.RemoveDynamic(this, &ATADebugQTETester::HandleEnded); RunningAction->Cancel(); }
 RunningAction.Reset(); Super::EndPlay(Reason);
}
