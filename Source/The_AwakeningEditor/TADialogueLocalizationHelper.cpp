// Source/The_AwakeningEditor/TADialogueLocalizationHelper.cpp
#include "TADialogueLocalizationHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

const TArray<FString>& FTADialogueLocalizationHelper::GetLanguageCodes()
{
	static const TArray<FString> Codes = { TEXT("zh-CN"), TEXT("zh-TW"), TEXT("en") };
	return Codes;
}

FString FTADialogueLocalizationHelper::GetLocalizationFilePath(const FString& LanguageCode)
{
	return FPaths::ProjectContentDir() / TEXT("Localization") / (LanguageCode + TEXT(".json"));
}

bool FTADialogueLocalizationHelper::LoadInto(const FString& LanguageCode, TMap<FString, TMap<FString, FString>>& Cache)
{
	TMap<FString, FString>& Map = Cache.FindOrAdd(LanguageCode);
	Map.Reset();

	FString JsonString;
	const FString FilePath = GetLocalizationFilePath(LanguageCode);
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DialogueEditor] 找不到本地化文件: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[DialogueEditor] 本地化 JSON 解析失败: %s"), *FilePath);
		return false;
	}

	for (const auto& Pair : Root->Values)
	{
		FString Value;
		if (Pair.Value->TryGetString(Value))
		{
			Map.Add(Pair.Key, Value);
		}
	}

	return true;
}

FString FTADialogueLocalizationHelper::GetText(const FString& Key, const FString& LanguageCode, TMap<FString, TMap<FString, FString>>& Cache)
{
	if (!Cache.Contains(LanguageCode))
	{
		LoadInto(LanguageCode, Cache);
	}

	const TMap<FString, FString>& Map = Cache[LanguageCode];
	const FString* Found = Map.Find(Key);
	return Found ? *Found : FString();
}

void FTADialogueLocalizationHelper::SetText(const FString& Key, const FString& LanguageCode, const FString& Text, TMap<FString, TMap<FString, FString>>& Cache)
{
	if (!Cache.Contains(LanguageCode))
	{
		LoadInto(LanguageCode, Cache);
	}

	TMap<FString, FString>& Map = Cache.FindOrAdd(LanguageCode);
	Map.Add(Key, Text);

	// 写回文件（美化格式）
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	for (const auto& Pair : Map)
	{
		Root->SetStringField(Pair.Key, Pair.Value);
	}

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString);
	if (FJsonSerializer::Serialize(Root, Writer, true))
	{
		FFileHelper::SaveStringToFile(JsonString, *GetLocalizationFilePath(LanguageCode), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

void FTADialogueLocalizationHelper::EnsureKey(const FString& Key, const FString& ZhCnText, TMap<FString, TMap<FString, FString>>& Cache)
{
	if (Key.IsEmpty())
	{
		return;
	}

	FString ZhText = ZhCnText;
	if (ZhText.IsEmpty())
	{
		ZhText = Key;
	}

	for (const FString& Lang : GetLanguageCodes())
	{
		if (!Cache.Contains(Lang))
		{
			LoadInto(Lang, Cache);
		}

		TMap<FString, FString>& Map = Cache.FindOrAdd(Lang);
		if (!Map.Contains(Key))
		{
			// zh-CN 填给定文本；其他语言先填中文占位，待翻译
			Map.Add(Key, ZhText);
			SetText(Key, Lang, ZhText, Cache);
		}
	}
}
