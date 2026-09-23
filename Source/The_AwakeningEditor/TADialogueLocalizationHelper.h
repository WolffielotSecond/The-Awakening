// Source/The_AwakeningEditor/TADialogueLocalizationHelper.h
// 本地化 JSON 读写助手（编辑器用）
#pragma once

#include "CoreMinimal.h"

/**
 * 直接读写 Content/Localization/{语言代码}.json（与 UTALocalizeSubsystem 同一套文件）。
 * 语言列表与 UTALocalizeSubsystem::GetAvailableLanguages() 保持一致。
 */
class FTADialogueLocalizationHelper
{
public:
	static const TArray<FString>& GetLanguageCodes();

	static FString GetLocalizationFilePath(const FString& LanguageCode);

	/** 加载语言文件到缓存（文件不存在则清空对应缓存项并返回 false） */
	static bool LoadInto(const FString& LanguageCode, TMap<FString, TMap<FString, FString>>& Cache);

	/** 取文本（自动按需加载缓存） */
	static FString GetText(const FString& Key, const FString& LanguageCode, TMap<FString, TMap<FString, FString>>& Cache);

	/** 设置文本并立即保存到文件 */
	static void SetText(const FString& Key, const FString& LanguageCode, const FString& Text, TMap<FString, TMap<FString, FString>>& Cache);

	/** 确保 Key 存在于三语文件：zh-CN 填给定文本，其他语言用中文占位（待翻译） */
	static void EnsureKey(const FString& Key, const FString& ZhCnText, TMap<FString, TMap<FString, FString>>& Cache);
};
