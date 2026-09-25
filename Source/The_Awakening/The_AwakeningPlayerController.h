// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "The_AwakeningPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UWidget;
class UTAScanningComponent;
struct FInputKeyEventArgs;
/**
 * 输入设备检测器
 */
class FTAInputDeviceDetector : public IInputProcessor
{
public:
	FTAInputDeviceDetector(class AThe_AwakeningPlayerController* InOwner)
		: Owner(InOwner)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SoftApp, TSharedRef<ICursor> Cursor) override;

	virtual bool HandleKeyDownEvent(FSlateApplication& SoftApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SoftApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SoftApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SoftApp, const FPointerEvent& MouseEvent) override;

private:
	AThe_AwakeningPlayerController* Owner = nullptr;
};

UCLASS(abstract)
class AThe_AwakeningPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AThe_AwakeningPlayerController();
	void NotifyRawInputKey(const FKey& Key);
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** 进入/退出对话输入模式（移除/恢复默认移动等映射，供剧情系统调用） */
	void SetDialogueModeActive(bool bActive, UUserWidget* FocusWidget = nullptr);

	/** 所有可交互 UI 共用的输入模式。调用需成对，支持多个 UI 同时打开。 */
	void BeginUIInputMode(UUserWidget* FocusWidget = nullptr);
	void EndUIInputMode();
	void SetUIFocusWidget(UUserWidget* FocusWidget);
	bool IsUIInputModeActive() const { return bUIInputModeActive; }
	void SetVirtualCursorAxis(const FKey& AxisKey, float Value);
	void TickVirtualCursor(float DeltaTime);
	void NotifyApplicationActivationChanged(bool bIsActive);

	/** Scan only requests cursor visibility; it does not enter menu input mode. */
	void BeginScanCursorMode();
	void EndScanCursorMode();
	bool IsScanCursorModeActive() const { return bScanCursorModeActive; }

protected:
	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category = "Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	TObjectPtr<UUserWidget> MobileControlsWidget;

	TSharedPtr<FTAInputDeviceDetector> InputDeviceDetector;
	FDelegateHandle ApplicationActivationHandle;
	int32 ActiveUIModeCount = 0;
	bool bUIInputModeActive = false;
	bool bDialogueModeActive = false;
	bool bPreviousShowMouseCursor = false;
	bool bMouseCursorBeforeScan = false;
	bool bScanCursorModeActive = false;
	FVector2D VirtualCursorAxis = FVector2D::ZeroVector;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scan", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTAScanningComponent> ScanningComponent;
};
