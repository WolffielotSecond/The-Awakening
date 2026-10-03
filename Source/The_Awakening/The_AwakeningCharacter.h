// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interaction/TADialogueParticipant.h"
#include "Logging/LogMacros.h"
#include "The_AwakeningCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAbilitySystemComponent;
class UTAParkourComponent;
class UTAInventoryComponent;
class UTAPromptComponent;
class UTAFreezeComponent;
class UTAInventoryPanelWidget;
class UTADialogueWidget;
class UTADialoguePortraitLayerWidget;
class UTADialogueHistoryWidget;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AThe_AwakeningCharacter : public ACharacter, public IAbilitySystemInterface, public ITADialogueParticipant
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTAFreezeComponent> FreezeComponent;

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

	/** Separate portrait-layer WBP, displayed behind DialogueWidgetClass. */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UTADialoguePortraitLayerWidget> DialoguePortraitLayerClass;

	/** Standalone dialogue history WBP, displayed above the dialogue UI. */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UTADialogueHistoryWidget> DialogueHistoryWidgetClass;

	/** 手柄左摇杆控制菜单鼠标光标的移动速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float MenuCursorSpeed = 1100.0f;

	UPROPERTY()
	TObjectPtr<UTAInventoryPanelWidget> InventoryPanelInstance;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleInventory();

	/** 临时调试入口：G 打开路径小游戏的测试 WBP。 */
	void DebugOpenPathPuzzle();

	UFUNCTION(BlueprintCallable, Category = "UI")
	bool IsInventoryOpen() const { return InventoryPanelInstance != nullptr; }
	
	//2D的角色朝向问题

	/** 2D角色当前视觉朝向，只有 -1 和 1 */
	float SpriteFacingDirection = 1.f;

	/** 更新Sprite视觉朝向 */
	void UpdateSpriteFacing();
	
public:
	AThe_AwakeningCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	bool IsLandingMomentumSafe(const FVector& Direction) const { return IsSafeToMoveToward(Direction); }
	void ClearMovementInput();
	void SubmitPlayerLook(float Yaw, float Pitch);
	virtual bool CanParticipateInDialogue_Implementation() const override;
	/** Explicit gameplay stop for normal movement/landing, never a permission side effect. */
	UFUNCTION(BlueprintCallable, Category="Movement")
	void StopCurrentMovement();
	float GetMenuCursorSpeed() const { return MenuCursorSpeed; }

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	void InitAbilityActorInfo();

	void Look(const FInputActionValue& Value);
	void MouseLook(const FInputActionValue& Value);

	bool bMoveForward = false;
	bool bMoveBackward = false;
	bool bMoveLeft = false;
	bool bMoveRight = false;



	void UpdateMovementInput();
	void UpdateHeldGameplayInput();
	bool bParkourJumpHeld = false;
	bool bParkourDropHeld = false;
	void TryPlayerInteract();

	void OnScanStarted(const FInputActionValue& Value);
	void OnScanEnded(const FInputActionValue& Value);
	void OnScanCanceled(const FInputActionValue& Value);

	UFUNCTION()
	void OnPromptRelatedSettingsChanged();

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
		meta = (ClampMin = "0.0"))
	float WalkSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
		meta = (ClampMin = "0.0"))
	float SprintSpeed = 750.f;

	// 避免摇杆回中漂移；设为 0 可取消此死区
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement",
		meta = (ClampMin = "0.0", ClampMax = "0.49"))
	float StickDeadZone = 0.1f;

	bool bSprintHeld = false;
	FVector2D StickInput = FVector2D::ZeroVector;
	// Coordinate basis of the accepted locomotion command, updated only with Move permission.
	float MovementCommandYaw = 0.f;
	void ExecuteMovementCommand(float Right, float Forward, float WorldYaw);


public:
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE TSubclassOf<UTADialogueWidget> GetDialogueWidgetClass() const { return DialogueWidgetClass; }
	FORCEINLINE TSubclassOf<UTADialoguePortraitLayerWidget> GetDialoguePortraitLayerClass() const { return DialoguePortraitLayerClass; }
	FORCEINLINE TSubclassOf<UTADialogueHistoryWidget> GetDialogueHistoryWidgetClass() const { return DialogueHistoryWidgetClass; }
};
