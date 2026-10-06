#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Settings/TASettingsTypes.h"
#include "TASettingsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTASettingValueChanged, FName, SettingId);

/** Owns live values and applying/persistence. Definitions and widgets live elsewhere. */
UCLASS()
class THE_AWAKENING_API UTASettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void UseDefinitionAsset(const UTASettingsDefinitionAsset* DefinitionAsset);

	UFUNCTION(BlueprintPure, Category = "Settings")
	const TArray<FTASettingDefinition>& GetDefinitions() const { return Definitions; }

	UFUNCTION(BlueprintPure, Category = "Settings")
	bool GetValue(FName SettingId, FTASettingValue& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool SetValue(FName SettingId, const FTASettingValue& Value);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool AdjustValue(FName SettingId, int32 Direction);

	UFUNCTION(BlueprintPure, Category = "Settings")
	bool IsFavorite(FName SettingId) const { return FavoriteSettingIds.Contains(SettingId); }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetFavorite(FName SettingId, bool bFavorite);

	UFUNCTION(BlueprintPure, Category = "Settings")
	TArray<FName> GetFavoriteSettingIds() const { return FavoriteSettingIds.Array(); }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void RestoreDefaults();

	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FTASettingValueChanged OnSettingValueChanged;

private:
	void BuildBuiltInDefinitions();
	void LoadSavedValues();
	void SaveValues() const;
	void ApplyValue(const FTASettingDefinition& Definition, const FTASettingValue& Value);
	const FTASettingDefinition* FindDefinition(FName SettingId) const;

	UPROPERTY(Transient)
	TArray<FTASettingDefinition> Definitions;

	UPROPERTY(Transient)
	TMap<FName, FTASettingValue> CurrentValues;

	UPROPERTY(Transient)
	TSet<FName> FavoriteSettingIds;
};
