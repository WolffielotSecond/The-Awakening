#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Settings/TASettingsTypes.h"
#include "TASettingsPageWidget.generated.h"
class UButton;
class UTextBlock;
class UImage;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTASettingsPageActivated, FName, PageId);

/** Blueprint owns sidebar entry layout; C++ owns its identity and click handling. */
UCLASS(Abstract, Blueprintable)
class THE_AWAKENING_API UTASettingsPageWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void Configure(const FTASettingsPageDefinition& Definition, FText Label, bool bSelected,bool bEnglishLayout=false);
 UPROPERTY(BlueprintAssignable, Category="Settings") FTASettingsPageActivated OnPageActivated;
 UFUNCTION(BlueprintImplementableEvent, Category="Settings") void OnSelectionChanged(bool bSelected);
protected:
 virtual void NativeConstruct() override;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> Button_Page;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Name;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> Image_Highlight;
private:
 void ApplyLanguageLayout(bool bEnglish);
 bool bLayoutDefaultsCaptured=false;
 bool bAuthoredNameWrap=false;
 FMargin AuthoredContentPadding;
 EHorizontalAlignment AuthoredContentHorizontal=HAlign_Center;
 UFUNCTION() void Clicked();
 UPROPERTY(Transient) FTASettingsPageDefinition PageDefinition;
 FLinearColor NormalColor=FLinearColor::White;
 bool bColorInitialized=false;
};
