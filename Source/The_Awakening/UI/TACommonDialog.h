#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Core/TAPlayerInputReceiver.h"
#include "TACommonDialog.generated.h"
class UButton;
class UTextBlock;
class UTAActionPromptWidget;
class UInputAction;
class AThe_AwakeningPlayerController;

USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTACommonDialogOption
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName ResultId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString LabelTextId;
 /** Logical IMC button, independent of its visual position. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1",ClampMax="4")) int32 InputSlot=1;
};
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTACommonDialogDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString TitleTextId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString MessageTextId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FTACommonDialogOption> Options;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float TimeoutSeconds=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName TimeoutResultId;
 bool IsValid() const;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTACommonDialogResult,FName,ResultId);

UCLASS(Blueprintable)
class THE_AWAKENING_API UTACommonDialogButtonWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 virtual void NativeOnInitialized() override;
 void Configure(const FTACommonDialogOption& Option,UInputAction* Action);
 UPROPERTY(BlueprintAssignable) FTACommonDialogResult OnChosen;
protected:
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Option;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTAActionPromptWidget> ActionPrompt_Option;
 UPROPERTY(EditDefaultsOnly,Category="Dialog") TSubclassOf<UTAActionPromptWidget> ActionPromptWidgetClass;
private:
 UFUNCTION() void Clicked();
 FName Result;
};

UCLASS(Blueprintable)
class THE_AWAKENING_API UTACommonDialogWidget : public UUserWidget, public ITAPlayerInputReceiver
{
 GENERATED_BODY()
public:
 UTACommonDialogWidget(const FObjectInitializer& O=FObjectInitializer::Get());
 bool Open(const FTACommonDialogDefinition& Definition);
 UFUNCTION(BlueprintCallable,Category="Dialog") void Dismiss();
 UPROPERTY(BlueprintAssignable) FTACommonDialogResult OnResult;
 virtual FTAInputRouter::FHandle GetPlayerInputRequestHandle() const override { return Handle; }
 virtual TOptional<ETAInputCapability> ResolvePlayerInput(FKey Key) const override;
 virtual void ExecutePlayerInput(FKey Key,ETAInputCapability Capability) override;
 virtual bool UsesSharedBackAction() const override { return false; }
 virtual void RemoveFromParent() override;
protected:
 virtual void NativeOnInitialized() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
 virtual FReply NativeOnPreviewKeyDown(const FGeometry&,const FKeyEvent&) override { return FReply::Handled(); }
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Message;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Countdown;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTACommonDialogButtonWidget> DialogOption_1;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTACommonDialogButtonWidget> DialogOption_2;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTACommonDialogButtonWidget> DialogOption_3;
 UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTACommonDialogButtonWidget> DialogOption_4;
 UPROPERTY(EditDefaultsOnly,Category="Dialog") TSubclassOf<UTACommonDialogButtonWidget> OptionWidgetClass;
private:
 #if WITH_DEV_AUTOMATION_TESTS
 friend class FTASettingsDialogTest;
 #endif
 UFUNCTION() void Choose(FName Result);
 UFUNCTION() void RefreshText();
 void ReleaseRequest();
 void Complete(FName Result);
 UPROPERTY(Transient) FTACommonDialogDefinition Data;
 TWeakObjectPtr<AThe_AwakeningPlayerController> Controller;
 FTAInputRouter::FHandle Handle=0;
 double Deadline=0;
 bool bFinished=false;
};

/** Shared by every local-player screen, independent of settings. One modal at a time. */
UCLASS()
class THE_AWAKENING_API UTACommonDialogSubsystem : public ULocalPlayerSubsystem
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="UI|Dialog") UTACommonDialogWidget* ShowDialog(const FTACommonDialogDefinition& Definition);
 UFUNCTION(BlueprintPure,Category="UI|Dialog") UTACommonDialogWidget* GetActiveDialog() const { return Active; }
 void Forget(UTACommonDialogWidget* Widget);
 virtual void Deinitialize() override;
private:
 UPROPERTY(Transient) TObjectPtr<UTACommonDialogWidget> Active;
};
