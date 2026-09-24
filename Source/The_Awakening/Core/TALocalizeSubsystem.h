#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TALocalizeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLanguageChanged);

UCLASS()

class THE_AWAKENING_API UTALocalizeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 获取本地化文本 */
	UFUNCTION(BlueprintCallable, Category = "Localization")
	FText GetText(const FString& TextId) const;

	/** 设置当前语言 e.g."zh-CN" */
	UFUNCTION(BlueprintCallable, Category = "Localization")
	bool SetLanguage(const FString& LanguageCode);

	/** 获取当前语言代码 */
	UFUNCTION(BlueprintCallable, Category = "Localization")
	FString GetCurrentLanguage() const { return CurrentLanguage; }

	/** 获取支持的语言列表 */
	UFUNCTION(BlueprintCallable, Category = "Localization")
	TArray<FString> GetAvailableLanguages() const;

	/** Shared by runtime loading and the project localization editor. */
	FString GetLocalizationDirectory() const;

	/** 语言切换时触发 */
	UPROPERTY(BlueprintAssignable, Category = "Localization")
	FOnLanguageChanged OnLanguageChanged;

protected:
	/** 加载指定语言的 JSON 文件 */
	bool LoadLanguageFile(const FString& LanguageCode);

	/** 当前语言 */
	UPROPERTY()
	FString CurrentLanguage;

	/** 当前语言的文本映射 ID → Text */
	UPROPERTY()
	TMap<FString, FString> TextMap;

	/** 默认语言 */
	UPROPERTY(EditDefaultsOnly, Category = "Localization")
	FString DefaultLanguage = TEXT("zh-CN");

	/** JSON 文件所在相对路径 */
	UPROPERTY(EditDefaultsOnly, Category = "Localization")
	FString LocalizationFolder = TEXT("Localization");
};
