#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TASettingsTypes.generated.h"

UENUM(BlueprintType)
enum class ETAGameSettingsPage : uint8
{
	Game,
	Display,
	Audio,
	MouseKeyboard,
	Controller,
	Favorites
};

UENUM(BlueprintType)
enum class ETASettingType : uint8
{
	Toggle,
	Choice,
	Slider,
	Submenu
};

UENUM(BlueprintType)
enum class ETASettingSubmenuTarget : uint8
{
	None,
	KeyBindings,
	Brightness
};

UENUM(BlueprintType)
enum class ETASettingValueKind : uint8
{
	Boolean,
	Number,
	Choice
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ETASettingValueKind Kind = ETASettingValueKind::Boolean;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bBoolean = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float Number = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName Choice = NAME_None;

	static FTASettingValue Boolean(bool bValue)
	{
		FTASettingValue Value;
		Value.Kind = ETASettingValueKind::Boolean;
		Value.bBoolean = bValue;
		return Value;
	}

	static FTASettingValue Numeric(float InValue)
	{
		FTASettingValue Value;
		Value.Kind = ETASettingValueKind::Number;
		Value.Number = InValue;
		return Value;
	}

	static FTASettingValue ChoiceValue(FName InValue)
	{
		FTASettingValue Value;
		Value.Kind = ETASettingValueKind::Choice;
		Value.Choice = InValue;
		return Value;
	}
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingChoice
{
	GENERATED_BODY()

	/** Stable machine value stored in settings; never use its localized label as identity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FName Value = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString NameTextId;
};

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FName SettingId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	ETAGameSettingsPage Page = ETAGameSettingsPage::Game;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	ETASettingType Type = ETASettingType::Toggle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FString NameTextId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FString DescriptionTextId;

	/** Keep a value out of its category list when it is edited by a dedicated submenu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	bool bSubmenuOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FTASettingValue DefaultValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Choice", meta = (EditCondition = "Type == ETASettingType::Choice", EditConditionHides))
	TArray<FTASettingChoice> Choices;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Slider", meta = (EditCondition = "Type == ETASettingType::Slider", EditConditionHides))
	float Minimum = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Slider", meta = (EditCondition = "Type == ETASettingType::Slider", EditConditionHides))
	float Maximum = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Slider", meta = (EditCondition = "Type == ETASettingType::Slider", EditConditionHides))
	float Step = 0.1f;

	/** Examples: Number, Percent, Integer, Gamma. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Slider", meta = (EditCondition = "Type == ETASettingType::Slider", EditConditionHides))
	FName DisplayFormat = FName(TEXT("Number"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Submenu", meta = (EditCondition = "Type == ETASettingType::Submenu", EditConditionHides))
	ETASettingSubmenuTarget SubmenuTarget = ETASettingSubmenuTarget::None;

	/** Optional data asset that defines the target menu's contents and behavior. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Submenu", meta = (EditCondition = "Type == ETASettingType::Submenu", EditConditionHides))
	TObjectPtr<class UTASettingsDefinitionAsset> TargetMenuDefinition = nullptr;
};

UCLASS(BlueprintType)
class THE_AWAKENING_API UTASettingsDefinitionAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TArray<FTASettingDefinition> Definitions;
};
