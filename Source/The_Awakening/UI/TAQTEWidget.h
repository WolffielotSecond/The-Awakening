#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TAQTEWidget.generated.h"
class UImage;
class UTexture2D;
class UMaterialInstanceDynamic;
class UTASpecialActionComponent;
/** Presentation only. No focus, key handling, or independent input bindings. */
UCLASS()
class THE_AWAKENING_API UTAQTEWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void Observe(UTASpecialActionComponent* Component);
 virtual bool Initialize() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
 UFUNCTION() void Refresh();
 float GetDisplayedFlip() const { return DisplayedFlip; }
 float GetDisplayedPulse() const { return DisplayedPulse; }
 static constexpr const TCHAR* RingMaterialPath = TEXT("/Game/UI/QTE/M_QTERing.M_QTERing");
 static constexpr const TCHAR* KeyMaterialPath = TEXT("/Game/UI/QTE/M_QTEKeyFlip.M_QTEKeyFlip");
private:
 UTexture2D* ResolveIcon() const;
 void SetFaces(UTexture2D* Front, UTexture2D* Back);
 TWeakObjectPtr<UTASpecialActionComponent> Action;
 UPROPERTY(Transient) TObjectPtr<UImage> Ring;
 UPROPERTY(Transient) TObjectPtr<UImage> Key;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> KeyMaterial;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> DisplayedIcon;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> IncomingIcon;
 int32 ObservedStage = INDEX_NONE;
 float FlipStartedAt = 0.f;
 float DisplayedFlip = 0.f;
 float DisplayedPulse = 1.f;
 bool bFlipping = false;
};
