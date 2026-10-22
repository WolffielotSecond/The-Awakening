#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputTriggers.h"
#include "GameplayTagContainer.h"
#include "TASpecialActionDefinition.generated.h"
class UInputAction;
class UInputMappingContext;
UENUM(BlueprintType)
enum class ETAQTEKind : uint8 { Press, Mash, Hold, Direction, Animation };
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAQTEStage
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETAQTEKind Kind = ETAQTEKind::Press;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UInputAction> Action;
 /** Choose one IA once per run. Only that IA may progress this stage. Empty uses Action. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UInputAction>> ActionOptions;
 /** Normalized gain for each discrete press; zero preserves the original 1/Required behavior. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float ProgressPerPress = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float DecayPerSecond = 0.f;
 /** Minimum gameplay seconds between accepted mash presses; zero disables the rate limit. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", Units="s")) float ClickCooldownSeconds = 0.1f;
 /** Press/Mash normally Started. Hold normally Triggered; respect IA trigger semantics. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETriggerEvent InputEvent = ETriggerEvent::Started;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString PromptTextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float Required = 1.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0")) float Timeout = 15.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D Direction = FVector2D(1,0);
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0",ClampMax="1.0")) float DirectionThreshold = 0.7f;
 /** Animation stages reuse CharacterActionComponent's acceptance/keyframe/fallback contract. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag AnimationTag;
};
UCLASS(BlueprintType)
class THE_AWAKENING_API UTASpecialActionDefinition : public UDataAsset
{
 GENERATED_BODY()
public:
 /** Optional dedicated IMC; existing mapped IA can be used without adding another IMC. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UInputMappingContext> MappingContext;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MappingPriority = 75;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTAQTEStage> Stages;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAlign = true;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform RelativeAlignment;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float MaxDistance = 250.f;
 /** Sent only after all stages succeed, not when the interaction is selected. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CompletionEvent;
 /** Circular UMG prompt, breathing icon and perspective flip. Uses the existing prompt anchor. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bUseQTEPresentation = false;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float KeyPulseAmplitude = 0.2f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float KeyPulseFrequency = 4.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05")) float KeyFlipDuration = 0.2f;
 /** Gameplay permission remains blocked briefly after the prompt closes. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="s")) float ExitInputBufferSeconds = 0.2f;
};
