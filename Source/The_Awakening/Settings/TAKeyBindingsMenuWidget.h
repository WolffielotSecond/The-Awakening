#pragma once
#include "CoreMinimal.h"
#include "Settings/TASettingsMenuWidget.h"
#include "TAKeyBindingsMenuWidget.generated.h"
class UTAKeyBindingService;
UCLASS(Abstract,Blueprintable)
class THE_AWAKENING_API UTAKeyBindingsMenuWidget : public UTASettingsMenuWidget
{
 GENERATED_BODY()
public:
 virtual bool IsCapturingPlayerInput() const override { return !CapturingId.IsNone(); }
 virtual bool CapturePlayerInput(FKey Key,bool bRepeat) override;
 virtual bool HandleMenuBackRequested() override;
protected:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void BuildSettingRows() override;
 virtual void RefreshRows() override;
 virtual void ConfirmSelection() override;
 virtual void RestoreCurrentDefaults() override;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UTextBlock> Text_CaptureStatus;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<class UButton> Button_CancelCapture;
private:
 bool IsControllerMenu() const;
 UFUNCTION() void CancelCapture();
 UFUNCTION() void BindingsChanged();
 UPROPERTY(Transient) TObjectPtr<UTAKeyBindingService> BindingService;
 FName CapturingId;
 FString StatusTextId;
 FDelegateHandle OwnershipLostHandle;
};
