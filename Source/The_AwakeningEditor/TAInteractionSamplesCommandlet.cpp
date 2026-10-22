#include "TAInteractionSamplesCommandlet.h"
#include "Interaction/TASpecialActionDefinition.h"
#include "Interaction/TAInteractableActor.h"
#include "Interaction/TAInteractionBehaviorComponent.h"
#include "Story/TAWorldEventDefinition.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TAGameplayTags.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/Blueprint.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
namespace
{
 bool Save(UObject* Asset)
 {
  FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
  const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
  return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
 }
 template<typename T> T* Make(const TCHAR* Name, TFunctionRef<void(T*)> Configure)
 {
  const FString Path = FString(TEXT("/Game/InteractionSamples/")) + Name;
  if (T* Existing = LoadObject<T>(nullptr, *(Path + TEXT(".") + Name))) return Existing;
  auto* Package = CreatePackage(*Path);
  auto* Asset = NewObject<T>(Package, Name, RF_Public | RF_Standalone);
  Configure(Asset); FAssetRegistryModule::AssetCreated(Asset);
  if (!Save(Asset)) return nullptr;
  return Asset;
 }
}
UTAInteractionSamplesCommandlet::UTAInteractionSamplesCommandlet()
{
 IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UTAInteractionSamplesCommandlet::Main(const FString&)
{
 auto* Mash = Make<UInputAction>(TEXT("IA_QTEPrimary"), [](UInputAction* A) { A->ValueType = EInputActionValueType::Boolean; });
 auto* Move = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
 if (!Mash || !Move) return 1;
 auto* IMC = Make<UInputMappingContext>(TEXT("IMC_SpecialAction"), [&](UInputMappingContext* C)
 {
  C->MapKey(Mash, EKeys::LeftMouseButton); C->MapKey(Mash, EKeys::Gamepad_FaceButton_Bottom);
  // Physical keys live exclusively in mapping configuration. Direction stage uses existing Move IA.
  C->MapKey(Move, EKeys::D); C->MapKey(Move, EKeys::Gamepad_Left2D);
 });
 if (!IMC) return 1;
 auto* Pipe = Make<UTASpecialActionDefinition>(TEXT("DA_PipeAction"), [&](UTASpecialActionDefinition* A)
 {
  A->MappingContext = IMC; A->RelativeAlignment.SetLocation(FVector(-80,0,0));
  A->CompletionEvent = TEXT("Chapter1.PipeAcquired");
  FTAQTEStage S; S.Kind = ETAQTEKind::Animation; S.AnimationTag = TAGameplayTags::Anim_GrabPipe;
  S.PromptTextId = TEXT("Interact_Default"); A->Stages.Add(S);
  S = FTAQTEStage(); S.Kind = ETAQTEKind::Mash; S.Action = Mash; S.Required = 8;
  S.PromptTextId = TEXT("Interact_Default"); A->Stages.Add(S);
  S = FTAQTEStage(); S.Kind = ETAQTEKind::Direction; S.Action = Move; S.InputEvent = ETriggerEvent::Triggered;
  S.PromptTextId = TEXT("Interact_Default"); A->Stages.Add(S);
  S = FTAQTEStage(); S.Kind = ETAQTEKind::Animation; S.AnimationTag = TAGameplayTags::Anim_DetachPipe;
  S.PromptTextId = TEXT("Interact_Default"); A->Stages.Add(S);
 });
 bool bOk = Pipe != nullptr;
 bOk &= Make<UTAWorldEventDefinition>(TEXT("DA_PoolEvent"), [](UTAWorldEventDefinition* D)
 { D->EventId = TEXT("Prologue.Pool"); D->CompletionTags.AddTag(TAGameplayTags::Story_Prologue_ChaseComplete); }) != nullptr;
 bOk &= Make<UTAWorldEventDefinition>(TEXT("DA_PipeAcquiredEvent"), [](UTAWorldEventDefinition* D)
 { D->EventId = TEXT("Chapter1.PipeAcquired"); D->CompletionTags.AddTag(TAGameplayTags::Story_Chapter1_PipeAcquired); }) != nullptr;
 bOk &= Make<UTAWorldEventDefinition>(TEXT("DA_AttackEvent"), [](UTAWorldEventDefinition* D)
 {
  D->EventId = TEXT("Chapter1.Attack"); D->ExclusiveGroup = TEXT("Chapter1.MonsterOutcome");
  D->Preconditions.RequiredTags.AddTag(TAGameplayTags::Story_Chapter1_PipeAcquired);
  D->CompletionTags.AddTag(TAGameplayTags::Story_Chapter1_Attacked); D->CompletionFlagDeltas.Add(TEXT("Morality"), -1);
 }) != nullptr;
 bOk &= Make<UTAWorldEventDefinition>(TEXT("DA_CraneEvent"), [](UTAWorldEventDefinition* D)
 {
  D->EventId = TEXT("Chapter1.Crane"); D->ExclusiveGroup = TEXT("Chapter1.MonsterOutcome");
  D->CompletionTags.AddTag(TAGameplayTags::Story_Chapter1_Crane); D->CompletionFlagDeltas.Add(TEXT("Morality"), 1);
 }) != nullptr;
 auto MakeActor = [&](const TCHAR* Name, TFunctionRef<void(UTAInteractionBehaviorComponent*)> Configure)
 {
  const FString Path = FString(TEXT("/Game/InteractionSamples/")) + Name;
  if (LoadObject<UBlueprint>(nullptr, *(Path + TEXT(".") + Name))) return true;
  auto* BP = FKismetEditorUtilities::CreateBlueprint(ATAInteractableActor::StaticClass(), CreatePackage(*Path), Name,
   BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
  auto* CDO = BP->GeneratedClass->GetDefaultObject<AActor>();
  auto* Mesh = CDO->FindComponentByClass<UStaticMeshComponent>();
  Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
  Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
  auto* Behavior = CDO->FindComponentByClass<UTAInteractionBehaviorComponent>();
  Behavior->bExecuteConfiguredBehavior = true; Configure(Behavior);
  FAssetRegistryModule::AssetCreated(BP); return Save(BP);
 };
 bOk &= MakeActor(TEXT("BP_ConditionalDoor"), [](UTAInteractionBehaviorComponent* C)
 { C->Conditions.RequiredTags.AddTag(TAGameplayTags::Story_Prologue_Diary); C->DeniedPromptTextId = TEXT("Interact_Default"); });
 bOk &= MakeActor(TEXT("BP_PoolEntrance"), [](UTAInteractionBehaviorComponent* C) { C->EventId = TEXT("Prologue.Pool"); });
 bOk &= MakeActor(TEXT("BP_PipeEntrance"), [&](UTAInteractionBehaviorComponent* C) { C->SpecialAction = Pipe; });
 // The original diary stays in its existing story slot and document flow; only add the acquisition tag.
 if (auto* Diary = LoadObject<UTAItemDefinition>(nullptr, TEXT("/Game/Document/DA_Item_Diary.DA_Item_Diary")))
 {
  if (!Diary->GrantedStoryTags.HasTagExact(TAGameplayTags::Story_Prologue_Diary))
  { Diary->GrantedStoryTags.AddTag(TAGameplayTags::Story_Prologue_Diary); bOk &= Save(Diary); }
 }
 UE_LOG(LogTemp, Display, TEXT("Interaction samples: %s. Configure a WorldEventDirector and ExecuteEvent/Finish in the test level."), bOk ? TEXT("created") : TEXT("failed"));
 return bOk ? 0 : 1;
}
