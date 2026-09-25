#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Scan/TAScanTypes.h"
#include "TAScanInfoWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UTALocalizeSubsystem;

UCLASS(Blueprintable)
class THE_AWAKENING_API UTAScanInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "Scan")
	void SetTargetInfo(const FTAScanTargetInfo& TargetInfo);

	UFUNCTION(BlueprintImplementableEvent, Category = "Scan", meta=(DisplayName="Scan Target Info Updated"))
	void BP_OnTargetInfoUpdated(const FTAScanTargetInfo& TargetInfo);

protected:
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Root;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_TargetName;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> Text_TargetDescription;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TargetType;

private:
	UFUNCTION()
	void HandleLanguageChanged();

	void RefreshLocalizedText();
	FString GetTargetTypeTextId() const;

	UPROPERTY(Transient)
	TObjectPtr<UTALocalizeSubsystem> LocalizeSubsystem;

	UPROPERTY(Transient)
	FTAScanTargetInfo CachedTargetInfo;
};
