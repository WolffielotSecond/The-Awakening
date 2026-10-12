#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TAVersionLibrary.generated.h"

/** Version metadata captured in the packaged config; no runtime Git dependency. */
UCLASS()
class THE_AWAKENING_API UTAVersionLibrary : public UBlueprintFunctionLibrary
{

	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "The Awakening|Version")
	static FString GetProjectVersion();

	UFUNCTION(BlueprintPure, Category = "The Awakening|Version")
	static FString GetBuildId();

	UFUNCTION(BlueprintPure, Category = "The Awakening|Version")
	static FString GetFullVersion();

	/** Store this in future saves and migrate older formats when loading. */
	UFUNCTION(BlueprintPure, Category = "The Awakening|Version")
	static int32 GetSaveFormatVersion();
};
