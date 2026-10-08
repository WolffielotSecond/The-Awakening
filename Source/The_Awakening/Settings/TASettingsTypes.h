#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TASettingsTypes.generated.h"

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingsSectionDefinition
{
 GENERATED_BODY()
 /** Identifier and localization key are the same. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SectionId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SortOrder = 0;
};
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingsPageDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PageId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SortOrder = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTASettingsSectionDefinition> Sections;
};
/** SortOrder determines sidebar order; equal orders retain the array order. */
UCLASS(BlueprintType)
class THE_AWAKENING_API UTASettingsPageDefinitionAsset : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTASettingsPageDefinition> Pages;
 UFUNCTION(CallInEditor, Category="Settings") void PopulateDefaultPages();
};
UENUM(BlueprintType)
enum class ETASettingType : uint8 { Toggle, Choice, Slider, Submenu };
UENUM(BlueprintType)
enum class ETASettingSubmenuTarget : uint8 { None, KeyBindings, Brightness, ControllerKeyBindings };
UENUM(BlueprintType)
enum class ETASettingDisplayFormat : uint8 { Number, Integer, Percent, Multiplier, Gamma };
UENUM(BlueprintType)
enum class ETASettingApplyPolicy : uint8 { Immediate, NextDialogue, VideoMode };
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingLocation
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PageId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SectionId;
 bool operator==(const FTASettingLocation& Other) const { return PageId==Other.PageId && SectionId==Other.SectionId; }
};
/** Edited with a two-level page/section checkbox picker in the editor module. */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingLocations
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTASettingLocation> Items;
};
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingChoice
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Value;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FString NameTextId;
};
UENUM(BlueprintType)
enum class ETASettingDescriptionBlockType : uint8 { Text, Image };
/** Ordered localized text and illustration blocks; presentation belongs to the WBP. */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingDescriptionBlock
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingDescriptionBlockType Type = ETASettingDescriptionBlockType::Text;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="Type == ETASettingDescriptionBlockType::Text",EditConditionHides)) FString TextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(EditCondition="Type == ETASettingDescriptionBlockType::Image",EditConditionHides)) TObjectPtr<class UTexture2D> Image;
};
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTASettingDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SettingId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FTASettingLocations Locations;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingType Type = ETASettingType::Toggle;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString NameTextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString DescriptionTextId;
 /** Empty uses DescriptionTextId. Otherwise blocks replace the plain description. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTASettingDescriptionBlock> DescriptionBlocks;
 /** Toggle: 0/1; choice: zero-based index; slider: 1..100; submenu: unused. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 DefaultValue = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ApplyHandlerId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingApplyPolicy ApplyPolicy = ETASettingApplyPolicy::Immediate;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bSubmenuOnly = false;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCanFavorite = true;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTASettingChoice> Choices;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ChoiceProviderId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Minimum = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Maximum = 1;
 /** Step in stored integer slider positions. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1",ClampMax="99")) int32 Step = 1;
 /** Optional rounding of the applied physical value; zero disables rounding. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float PhysicalStep = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) ETASettingDisplayFormat DisplayFormat = ETASettingDisplayFormat::Number;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 DecimalPlaces = 1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) FString UnitTextId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<class UTASettingsMenuWidget> TargetMenuWidgetClass;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class UTASettingsMenuDefinitionAsset> TargetMenuDefinition;
 bool Normalize(int32 Input, int32& Output) const;
 float ToSliderNumber(int32 StoredValue) const;
 int32 FromSliderNumber(float Number) const;
 FName GetChoiceValue(int32 Index) const;
 int32 FindChoiceIndex(FName Value) const;
 bool IsValidDefinition() const;
 bool BelongsTo(FName PageId,FName SectionId=NAME_None) const;
};
UCLASS(BlueprintType)
class THE_AWAKENING_API UTASettingsDefinitionAsset : public UDataAsset
{
 GENERATED_BODY()
public:
 /** Shared source for runtime navigation and the editor location picker. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTASettingsPageDefinitionAsset> PageDefinitionAsset;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTASettingDefinition> Definitions;
 /** Populates editable defaults. Assign submenu WBP classes afterwards. */
 UFUNCTION(CallInEditor, Category="Settings") void PopulateDefaultDefinitions();
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
struct THE_AWAKENING_API FTASettingsViewGroup
{
 FName HeadingId;
 TArray<FName> SettingIds;
};
struct THE_AWAKENING_API FTASettingsCatalog
{
 static FName FavoritesPageId();
 static TArray<FTASettingsPageDefinition> BuildPages();
 static void SortPages(TArray<FTASettingsPageDefinition>& Pages);
 static bool ValidatePages(const TArray<FTASettingsPageDefinition>& Pages,FString& Error);
 static bool ValidateLocations(const FTASettingDefinition& Definition,const TArray<FTASettingsPageDefinition>& Pages);
 static TArray<FTASettingsPageDefinition> BuildNavigationPages(const TArray<FTASettingsPageDefinition>& Pages);
 static TArray<FTASettingsViewGroup> BuildViewGroups(const TArray<FTASettingsPageDefinition>& Pages,const TArray<FTASettingDefinition>& Definitions,FName PageId,const TSet<FName>& Favorites);
 static TArray<FTASettingDefinition> Build(UObject* Outer);
 static void RefreshCandidates(FTASettingDefinition& Definition);
};
