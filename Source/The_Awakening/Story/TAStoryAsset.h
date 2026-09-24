// Source/The_Awakening/Story/TAStoryAsset.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Story/TADialogueTypes.h"
#include "TAStoryAsset.generated.h"

class UEdGraph;

/** Authored dialogue content stored as a native Unreal asset. */
UCLASS(BlueprintType)
class THE_AWAKENING_API UTAStoryAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override
	{
		Super::PostLoad();
		if (bHasCompiledData)
		{
			CompiledStory.RebuildIndex();
		}
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FString StoryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Story")
	FText DisplayName;

	/** Compiled runtime representation generated from the editor graph. */
	const FTAStoryData* GetCompiledStory() const
	{
		return bHasCompiledData ? &CompiledStory : nullptr;
	}

#if WITH_EDITORONLY_DATA
	/** Authoring graph. The editor module owns its node and schema classes. */
	UPROPERTY()
	TObjectPtr<UEdGraph> StoryGraph;
#endif

protected:
	UPROPERTY()
	FTAStoryData CompiledStory;

	UPROPERTY()
	bool bHasCompiledData = false;

#if WITH_EDITOR
public:
	void SetCompiledStory(const FTAStoryData& InStory)
	{
		CompiledStory = InStory;
		CompiledStory.RebuildIndex();
		bHasCompiledData = true;
	}

	void InvalidateCompiledStory()
	{
		bHasCompiledData = false;
	}
#endif
};
