#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TASettingsSectionWidget.generated.h"
class UTextBlock;
/** Noninteractive group heading. Blueprint owns all layout and style. */
UCLASS(Abstract,Blueprintable)
class THE_AWAKENING_API UTASettingsSectionWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void SetTitle(FText Title);
protected:
 UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Title;
};
