#pragma once

#include "CoreMinimal.h"

struct FTAStoryData;

namespace TAStoryGraphValidation
{
	/** Require every output path, including disconnected authoring branches, to terminate at End. */
	bool ValidateEndPaths(const FTAStoryData& Story, FText& OutError, FString& OutNodeId);
}
