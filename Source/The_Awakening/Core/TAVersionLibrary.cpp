#include "Core/TAVersionLibrary.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	FString ReadVersionString(const TCHAR* Section, const TCHAR* Key, const TCHAR* Fallback)
	{
		FString Value;
		if (GConfig)
		{
			GConfig->GetString(Section, Key, Value, GGameIni);
		}
		return Value.IsEmpty() ? FString(Fallback) : Value;
	}
}

FString UTAVersionLibrary::GetProjectVersion()
{
	return ReadVersionString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), TEXT("unknown"));
}

FString UTAVersionLibrary::GetBuildId()
{
	return ReadVersionString(TEXT("TheAwakening.Build"), TEXT("BuildId"), TEXT("local"));
}

FString UTAVersionLibrary::GetFullVersion()
{
	return GetProjectVersion() + TEXT("+") + GetBuildId();
}

int32 UTAVersionLibrary::GetSaveFormatVersion()
{
	int32 Version = 1;
	if (GConfig)
	{
		GConfig->GetInt(TEXT("TheAwakening.Build"), TEXT("SaveFormatVersion"), Version, GGameIni);
	}
	return FMath::Max(Version, 1);
}
