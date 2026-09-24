#include "Core/TALocalizeSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"

void UTALocalizeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 这里可以之后改成从 GameUserSettings 读取玩家保存的语言
	SetLanguage(DefaultLanguage);
}

bool UTALocalizeSubsystem::SetLanguage(const FString& LanguageCode)
{
	if (LoadLanguageFile(LanguageCode))
	{
		CurrentLanguage = LanguageCode;
		OnLanguageChanged.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("Localization language set to: %s"), *LanguageCode);
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("Failed to load language: %s"), *LanguageCode);
	return false;
}

bool UTALocalizeSubsystem::LoadLanguageFile(const FString& LanguageCode)
{
	const FString FilePath = GetLocalizationDirectory() / (LanguageCode + TEXT(".json"));

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot load localization file: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse localization JSON: %s"), *FilePath);
		return false;
	}

	TextMap.Empty();

	for (const auto& Pair : JsonObject->Values)
	{
		FString Value;
		if (Pair.Value->TryGetString(Value))
		{
			TextMap.Add(Pair.Key, Value);
		}
	}

	return true;
}

FText UTALocalizeSubsystem::GetText(const FString& TextId) const
{
	if (const FString* Found = TextMap.Find(TextId))
	{
		return FText::FromString(Found->IsEmpty() ? TextId : *Found);
	}

	// 找不到时返回 ID 本身
	return FText::FromString(TextId);
}

TArray<FString> UTALocalizeSubsystem::GetAvailableLanguages() const
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(GetLocalizationDirectory() / TEXT("*.json")), true, false);
	TArray<FString> Languages;
	for (const FString& File : Files) { Languages.Add(FPaths::GetBaseFilename(File)); }
	Languages.Sort();
	for (const FString& Preferred : { FString(TEXT("en")), FString(TEXT("zh-TW")), FString(TEXT("zh-CN")) })
	{
		if (Languages.Remove(Preferred)) { Languages.Insert(Preferred, 0); }
	}
	return Languages;
}

FString UTALocalizeSubsystem::GetLocalizationDirectory() const
{
	return FPaths::ProjectContentDir() / LocalizationFolder;
}
