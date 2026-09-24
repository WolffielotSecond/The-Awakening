// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Logging/LogMacros.h"
#include "The_AwakeningCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAbilitySystemComponent;
class UTAParkourComponent;
class UTAInventoryComponent;
class UTAPromptComponent;
class UTAInventoryPanelWidget;
class UTADialogueWidget;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AThe_AwakeningCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** 2D角色渲染 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> FlipbookComponent;

	/** PaperZD动画驱动 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperZDAnimationComponent> PaperZDAnimationComponent;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 键盘四方向 */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveForwardAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveBackwardAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveLeftAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveRightAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MouseLookAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* InteractAction;

	/** 扫描输入*/
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ScanAction; //rmb右键

	/** 跑酷输入 */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ParkourJumpAction;   // 空格

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ParkourDropAction;   // Ctrl

	/** 切换背包输入 */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ToggleInventoryAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAParkourComponent> ParkourComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAPromptComponent> PromptComponent;

	/** 相机跟随距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraDistance = 600.f;

	/** 鼠标控制相机位置的灵敏度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraMoveSensitivity = 5.f;

	/** 当前相机位置偏移 */
	FVector CurrentCameraOffset = FVector::ZeroVector;

	/** 相机位置偏移限制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraOffsetLimitX = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraOffsetLimitZ = 150.f;

	/** 相机俯仰角度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraPitchAngle = -30.f;

	/** 相机高度偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float CameraHeightOffset = 80.f;

	/** 是否反转鼠标左右控制相机 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bInvertCameraX = false;

	/** 是否反转鼠标上下控制相机 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bInvertCameraY = false;

	/** 交互检测距离 */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float InteractionDistance = 200.f;

	/** 当前可交互目标 */
	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentInteractTarget;

	/** 尝试进行交互 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void TryInteract();

	/** 更新当前可交互目标并控制提示显示 */
	void UpdateInteractTarget();

	/** 允许走下去的最大落差*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MaxSafeFallHeight = 80.f;

	/** 边缘检测向前探出的距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float EdgeCheckForwardDistance = 60.f;

	bool IsSafeToMoveToward(const FVector& WorldDirection) const;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UTAInventoryPanelWidget> InventoryPanelClass;

	/** Runtime dialogue widget, configured on the player character Blueprint like InventoryPanelClass. */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UTADialogueWidget> DialogueWidgetClass;

	UPROPERTY()
	TObjectPtr<UTAInventoryPanelWidget> InventoryPanelInstance;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool IsInventoryOpen() const { return InventoryPanelInstance != nullptr; }
	
	//2D的角色朝向问题

	/** 2D角色当前视觉朝向，只有 -1 和 1 */
	float SpriteFacingDirection = 1.f;

	/** 更新Sprite视觉朝向 */
	void UpdateSpriteFacing();
	
public:
	AThe_AwakeningCharacter();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	void InitAbilityActorInfo();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	bool bMoveForward = false;
	bool bMoveBackward = false;
	bool bMoveLeft = false;
	bool bMoveRight = false;

	void OnMoveForward(const FInputActionValue& Value);
	void OnMoveBackward(const FInputActionValue& Value);
	void OnMoveLeft(const FInputActionValue& Value);
	void OnMoveRight(const FInputActionValue& Value);

	void OnMoveForwardReleased(const FInputActionValue& Value);
	void OnMoveBackwardReleased(const FInputActionValue& Value);
	void OnMoveLeftReleased(const FInputActionValue& Value);
	void OnMoveRightReleased(const FInputActionValue& Value);

	void UpdateMovementInput();

	void OnParkourJump(const FInputActionValue& Value);
	void OnParkourDrop(const FInputActionValue& Value);

	void OnScanStarted(const FInputActionValue& Value);
	void OnScanEnded(const FInputActionValue& Value);

	UFUNCTION()
	void OnPromptRelatedSettingsChanged();

public:
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE TSubclassOf<UTADialogueWidget> GetDialogueWidgetClass() const { return DialogueWidgetClass; }
};
