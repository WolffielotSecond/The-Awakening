#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TAPreviewStage.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UTextureRenderTarget2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTADocumentDefinition;

/**
 * 3D 预览台：把一个模型渲染到 RenderTarget，供 UI 显示（拾取提示、日记本页面、以后的检视界面）。
 *
 * - 独立 Actor，放在远离关卡的位置（SpawnPreviewStage 按槽位错开），只拍自己的模型和背景板。
 * - 灯光和模型只用光照通道 1，不照亮关卡，也不被关卡灯光影响。
 * - 不挂冻结组件：扫描、背包、阅读等时停期间照常旋转（和扫描特效同一做法）。全局暂停时停止。
 * - 模型为空时显示占位立方体并打 Warning。
 * - 模型按包围球自动居中、自动取景；DisplayTransform 在此基础上再做偏移 / 旋转 / 缩放。
 * - 透明背景（默认）：以 SceneColorHDR 捕获，A 通道为反向不透明度，UI 必须用 GetDisplayMaterial() 的材质显示，
 *   材质会还原成透明背景；直接显示 RenderTarget 会看到反的透明度。关掉 bTransparentBackground 则输出不透明的 LDR 画面。
 * 外观参数（灯光、背景、取景、旋转速度）都在蓝图子类里调。
 */
UCLASS(Blueprintable)
class THE_AWAKENING_API ATAPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	ATAPreviewStage();

	/** 生成一个预览台。Slot 用于同时存在多个预览台时错开位置（例如拾取提示 0、日记本 1）。 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage", meta = (WorldContext = "WorldContext", DeterminesOutputType = "StageClass"))
	static ATAPreviewStage* SpawnPreviewStage(UObject* WorldContext, TSubclassOf<ATAPreviewStage> StageClass, int32 Slot = 0);

	/** 换模型。Mesh 为空时显示占位模型。会重置手动旋转并重新取景。 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	void SetDisplayMesh(UStaticMesh* Mesh, const FTransform& DisplayTransform);

	/** 换成骨骼网格体（保持参考姿势）。为空时显示占位模型。 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	void SetDisplaySkeletalMesh(USkeletalMesh* Mesh, const FTransform& DisplayTransform);

	/** 按资源类型选用静态 / 骨骼网格体；其他类型或为空时显示占位模型 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	void SetDisplayModel(UObject* Model, const FTransform& DisplayTransform);

	/** Configure the shared document presentation; framing remains fixed while it folds. */
	void SetDocument(UTADocumentDefinition* Definition, const FTransform& Adjustment, bool bOpened);
	FTransform GetDiaryModelTransform(const UTADocumentDefinition* Definition) const;
	void SetPaperOpenness(float Value);
	float GetPaperFoldDuration() const;
	void SetModelPresentationTransform(const FTransform& Transform);
	FQuat GetManualRotation() const { return PivotRotation; }
	void SetManualRotation(const FQuat& Rotation);

	/** 清空模型（不显示占位），并停止渲染 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	void ClearDisplay();

	/** 开关逐帧渲染。界面隐藏时关掉以省性能。 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	void SetCaptureEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Preview Stage")
	bool IsCaptureEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	void SetAutoRotate(bool bEnabled) { bAutoRotate = bEnabled; }

	/**
	 * 手动旋转（检视界面拖动用），叠加在当前朝向上。轴固定在预览台坐标系：
	 * DeltaYaw 绕竖直轴（正值从上往下看顺时针），DeltaPitch 绕镜头水平轴（正值模型顶部转离镜头），可无限旋转。
	 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	void AddManualRotation(float DeltaYaw, float DeltaPitch);

	/** 渲染结果，首次调用时按 RenderTargetSize 创建 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	UTextureRenderTarget2D* GetRenderTarget();

	/**
	 * UI 显示用的材质实例（已绑定 RenderTarget 和曝光），WBP 的 Image 用它做画刷。
	 * 不透明模式或材质资源缺失时返回 nullptr（缺失会打 Warning），此时直接用 GetRenderTarget()。
	 */
	UFUNCTION(BlueprintCallable, Category = "Preview Stage")
	UMaterialInstanceDynamic* GetDisplayMaterial();

	UFUNCTION(BlueprintPure, Category = "Preview Stage")
	bool IsShowingPlaceholder() const { return bShowingPlaceholder; }

	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }
	USceneComponent* GetPivotComponent() const { return PivotComponent; }
	USceneCaptureComponent2D* GetCaptureComponent() const { return CaptureComponent; }

	virtual void Tick(float DeltaSeconds) override;

	// ==================== 参数（蓝图子类里调） ====================

	/** 自动旋转速度（度 / 秒，绕竖直轴） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage")
	float RotationSpeed = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage")
	bool bAutoRotate = true;

	/** 相机俯仰角（负值为俯视） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage|Framing")
	float CameraPitch = -15.f;

	/** 取景留白：1 = 包围球刚好贴边，越大模型越小 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage|Framing", meta = (ClampMin = "1.0"))
	float FramingPadding = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage|Framing", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage|Render")
	FIntPoint RenderTargetSize = FIntPoint(512, 512);

	/** 曝光补偿（EV，手动曝光，不随关卡亮度变化） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage|Render")
	float ExposureCompensation = 0.f;

	/** 透明背景：模型以外的区域完全透明，露出 UI 底图。需在首次 GetRenderTarget 之前设置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage|Render")
	bool bTransparentBackground = true;

	/** 透明模式下的 UI 显示材质（由 -run=TAPreviewMaterial 生成） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage|Render")
	TSoftObjectPtr<UMaterialInterface> DisplayMaterial;

	static const FString DisplayMaterialPackage;
	static const FName DisplayTextureParameter;
	static const FName DisplayExposureParameter;

	/** 模型为空时显示的占位模型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview Stage")
	TObjectPtr<UStaticMesh> PlaceholderMesh;

	/** 多个预览台的基准位置和间隔 */
	static const FVector StageOrigin;
	static constexpr float StageSpacing = 5000.f;

	/** 预览台使用的光照通道（关卡默认用通道 0） */
	static constexpr int32 PreviewLightingChannel = 1;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 模型绕它旋转 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> PivotComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	/** 背景板：默认无模型（纯色背景由 UI 负责）。蓝图子类可以给它模型和材质。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BackdropComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> FillLight;

private:
	/** 两种网格体最多显示一个；都为空时清空 */
	void ApplyMesh(UStaticMesh* StaticMesh, USkeletalMesh* SkeletalMesh, const FTransform& DisplayTransform);
	bool HasModel() const;
	void UpdateFraming();
	static void UsePreviewLightingChannel(UPrimitiveComponent* Primitive);

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DisplayMaterialInstance;

	bool bShowingPlaceholder = false;
	/** 模型朝向的唯一来源（四元数累计，避免欧拉角在 ±90° 俯仰处翻转） */
	FQuat PivotRotation = FQuat::Identity;
	/** 当前模型在旋转中心坐标系下的包围球半径 */
	float ModelRadius = 50.f;
};
