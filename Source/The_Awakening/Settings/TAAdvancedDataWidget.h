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
public:
 /** 完整模式附加项：发行平台（读构建配置）与“HH:MM:SS(倍率X)”格式的游戏时间。 */
 static FString GetPlatformText(const TFunctionRef<FString(const FString&)>& T);
 FString GetGameTimeText(const class UTALocalizeSubsystem* Loc) const;
 static FString FormatGameTime(double Seconds,float Scale,const FText& Pattern);
private:
 void Refresh();
 UPROPERTY(Transient) TObjectPtr<UTASettingsSubsystem> Settings;
 double LastSample=0;
 int32 Frames=0;
};
