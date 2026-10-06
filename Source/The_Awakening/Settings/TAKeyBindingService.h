#pragma once
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputCoreTypes.h"
#include "TAKeyBindingService.generated.h"
class UInputAction;
class UInputMappingContext;
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAKeyBindingDefinition
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FName BindingId;
 UPROPERTY(BlueprintReadOnly) FName MappingName;
 UPROPERTY(BlueprintReadOnly) FString NameTextId;
 UPROPERTY(BlueprintReadOnly) TObjectPtr<const UInputAction> Action;
 UPROPERTY(BlueprintReadOnly) bool bController = false;
 UPROPERTY(BlueprintReadOnly) FKey DefaultKey;
 UPROPERTY(BlueprintReadOnly) FName ConflictGroup;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTAKeyBindingsChanged);
UCLASS()
class THE_AWAKENING_API UTAKeyBindingService : public ULocalPlayerSubsystem
{
 GENERATED_BODY()
public:
 UInputMappingContext* PrepareContext(UInputMappingContext* Source);
 const TArray<FTAKeyBindingDefinition>& GetBindings() const { return Bindings; }
 FKey GetKey(FName Id) const;
 bool Rebind(FName Id,FKey Key,FString& ErrorTextId);
 UFUNCTION(BlueprintCallable,Category="Settings") void RestoreDeviceDefaults(bool bController);
 UPROPERTY(BlueprintAssignable) FTAKeyBindingsChanged OnBindingsChanged;
 static bool GroupsOverlap(FName A,FName B);
 static UInputMappingContext* BuildRuntimeContext(UInputMappingContext* Source,UObject* Outer,TArray<FTAKeyBindingDefinition>& OutBindings);
private:
 void RebuildAndSave();
 UPROPERTY(Transient) TArray<FTAKeyBindingDefinition> Bindings;
 UPROPERTY(Transient) TMap<TObjectPtr<UInputMappingContext>,TObjectPtr<UInputMappingContext>> RuntimeContexts;
};
