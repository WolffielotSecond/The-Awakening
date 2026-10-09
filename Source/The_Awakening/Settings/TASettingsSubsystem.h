#pragma once
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Settings/TASettingsTypes.h"
#include "TASettingsSubsystem.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTASettingValueChanged, FName, SettingId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTASettingsFavoritesChanged);
UCLASS()
class THE_AWAKENING_API UTASettingsSubsystem : public ULocalPlayerSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 bool UseDefinitionAsset(const UTASettingsDefinitionAsset* Asset);
 UFUNCTION(BlueprintPure, Category="Settings") const TArray<FTASettingDefinition>& GetDefinitions() const { return Definitions; }
 const TArray<FTASettingsPageDefinition>& GetPages() const { return Pages; }
 const FTASettingDefinition* FindDefinition(FName Id) const;
 UFUNCTION(BlueprintPure, Category="Settings") bool GetValue(FName Id, int32& Out) const;
 UFUNCTION(BlueprintPure, Category="Settings") int32 GetInteger(FName Id,int32 Fallback=0) const;
 UFUNCTION(BlueprintCallable, Category="Settings") bool SetValue(FName Id, int32 Value);
 UFUNCTION(BlueprintCallable, Category="Settings") bool AdjustValue(FName Id,int32 Direction);
 UFUNCTION(BlueprintPure, Category="Settings") bool IsFavorite(FName Id) const { return FavoriteSettingIds.Contains(Id); }
 UFUNCTION(BlueprintCallable, Category="Settings") void SetFavorite(FName Id,bool bFavorite);
 UFUNCTION(BlueprintPure, Category="Settings") TArray<FName> GetFavoriteSettingIds() const { return FavoriteSettingIds.Array(); }
 UFUNCTION(BlueprintCallable, Category="Settings") void RestoreDefaults();
 UFUNCTION(BlueprintCallable, Category="Settings") void RestoreSettingDefault(FName Id);
 UFUNCTION(BlueprintCallable, Category="Settings") void ConfirmVideoMode();
 UFUNCTION(BlueprintCallable, Category="Settings") void RevertVideoMode();
 UFUNCTION(BlueprintPure, Category="Settings") bool HasPendingVideoMode() const { return VideoDeadline > 0; }
 void CheckVideoModeTimeout();
 float GetNumber(FName Id,float Fallback=1) const;
 bool GetBoolean(FName Id,bool Fallback=false) const;
 FName GetChoice(FName Id,FName Fallback=NAME_None) const;
 FText FormatValue(const FTASettingDefinition& D) const;
 UPROPERTY(BlueprintAssignable) FTASettingValueChanged OnSettingValueChanged;
 UPROPERTY(BlueprintAssignable) FTASettingsFavoritesChanged OnFavoritesChanged;
private:
#if WITH_DEV_AUTOMATION_TESTS
 friend class FTASettingsStateTest;
 friend class FTASettingsMenuInputTest;
#endif
 void LoadSavedValues();
 void SaveValues() const;
 void NotifyVideoValues();
 UPROPERTY(Transient) TArray<FTASettingsPageDefinition> Pages;
 UPROPERTY(Transient) TArray<FTASettingDefinition> Definitions;
 UPROPERTY(Transient) TMap<FName,int32> CurrentValues;
 UPROPERTY(Transient) TSet<FName> FavoriteSettingIds;
 FIntPoint PreviousResolution;
 int32 PreviousWindowMode = 0;
 double VideoDeadline = 0;
};
