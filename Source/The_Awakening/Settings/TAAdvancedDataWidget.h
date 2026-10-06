#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAAdvancedDataWidget.generated.h"
class UTASettingsSubsystem;
UCLASS(Abstract,Blueprintable)
class THE_AWAKENING_API UTAAdvancedDataWidget : public UUserWidget
{
 GENERATED_BODY()
protected:
 virtual void NativeConstruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
 UPROPERTY(meta=(BindWidget)) TObjectPtr<class UTextBlock> Text_AdvancedData;
 UPROPERTY(EditDefaultsOnly,Category="Settings",meta=(ClampMin="0.1")) float RefreshInterval=0.25f;
private:
 void Refresh();
 UPROPERTY(Transient) TObjectPtr<UTASettingsSubsystem> Settings;
 double LastSample=0;
 int32 Frames=0;
};
