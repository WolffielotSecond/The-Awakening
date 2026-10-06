#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TASettingsTypes.generated.h"

UENUM(BlueprintType)
enum class ETAGameSettingsPage : uint8 { Game, Display, Audio, MouseKeyboard, Controller, Favorites };
UENUM(BlueprintType)
enum class ETASettingType : uint8 { Toggle, Choice, Slider, Submenu };
UENUM(BlueprintType)
enum class ETASettingSubmenuTarget : uint8 { None, KeyBindings, Brightness, ControllerKeyBindings };
UENUM(BlueprintType)
enum class ETASettingValueKind : uint8 { None, Boolean, Number, Choice };
UENUM(BlueprintType)
enum class ETASettingDisplayFormat : uint8 { Number, Integer, Percent, Multiplier, Gamma };
UENUM(BlueprintType)
enum class ETASettingApplyPolicy : uint8 { Immediate, NextDialogue, VideoMode };
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingValue
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) ETASettingValueKind Kind = ETASettingValueKind::None;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBoolean = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float Number = 0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Choice;
 static FTASettingValue Boolean(bool B) { FTASettingValue V; V.Kind=ETASettingValueKind::Boolean; V.bBoolean=B; return V; }
 static FTASettingValue Numeric(float N) { FTASettingValue V; V.Kind=ETASettingValueKind::Number; V.Number=N; return V; }
 static FTASettingValue ChoiceValue(FName C) { FTASettingValue V; V.Kind=ETASettingValueKind::Choice; V.Choice=C; return V; }
};
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingChoice
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Value;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FString NameTextId;
};
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SettingId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETAGameSettingsPage Page = ETAGameSettingsPage::Game;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingType Type = ETASettingType::Toggle;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString NameTextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DescriptionTextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FTASettingValue DefaultValue;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ApplyHandlerId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingApplyPolicy ApplyPolicy = ETASettingApplyPolicy::Immediate;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bSubmenuOnly = false;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCanFavorite = true;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTASettingChoice> Choices;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ChoiceProviderId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Minimum = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Maximum = 1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Step = 0.1f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingDisplayFormat DisplayFormat = ETASettingDisplayFormat::Number;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 DecimalPlaces = 1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString UnitTextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingSubmenuTarget SubmenuTarget = ETASettingSubmenuTarget::None;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class UTASettingsMenuDefinitionAsset> TargetMenuDefinition;
 bool Normalize(const FTASettingValue& Input, FTASettingValue& Output) const;
 bool IsValidDefinition() const;
};
UCLASS(BlueprintType)
class THE_AWAKENING_API UTASettingsDefinitionAsset : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTASettingDefinition> Definitions;
};
UCLASS(BlueprintType)
class THE_AWAKENING_API UTASettingsMenuDefinitionAsset : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MenuId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString TitleTextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingSubmenuTarget MenuKind = ETASettingSubmenuTarget::None;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> SettingIds;
};
/** Definition building and hardware candidates have no side effects. */
struct THE_AWAKENING_API FTASettingsCatalog
{
 static TArray<FTASettingDefinition> Build(UObject* Outer);
 static void RefreshCandidates(FTASettingDefinition& Definition);
};
