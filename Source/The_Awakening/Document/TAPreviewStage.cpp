#include "Document/TAPreviewStage.h"
#include "Inventory/TAItemSettings.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTAPreviewStage, Log, All);

const FVector ATAPreviewStage::StageOrigin(0.f, 0.f, -50000.f);
const FString ATAPreviewStage::DisplayMaterialPackage(TEXT("/Game/UI/Preview/M_UI_PreviewStage"));
const FName ATAPreviewStage::DisplayTextureParameter(TEXT("PreviewTexture"));
const FName ATAPreviewStage::DisplayExposureParameter(TEXT("Exposure"));

ATAPreviewStage::ATAPreviewStage()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PivotComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	PivotComponent->SetupAttachment(SceneRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(PivotComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
	UsePreviewLightingChannel(MeshComponent);

	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMeshComponent->SetupAttachment(PivotComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshComponent->SetCastShadow(false);
	SkeletalMeshComponent->SetVisibility(false);
	// 预览台远离玩家，默认的“不可见时不更新”会让模型停在错误姿势
	SkeletalMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	UsePreviewLightingChannel(SkeletalMeshComponent);

	BackdropComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Backdrop"));
	BackdropComponent->SetupAttachment(SceneRoot);
	BackdropComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackdropComponent->SetCastShadow(false);
	UsePreviewLightingChannel(BackdropComponent);

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComponent->SetupAttachment(SceneRoot);
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	// 实际捕获源在创建 RenderTarget 时按 bTransparentBackground 决定
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	// 只拍预览台自己：关掉天空、雾、天光和全局光照，曝光固定
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetVolumetricFog(false);
	CaptureComponent->ShowFlags.SetSkyLighting(false);
	CaptureComponent->ShowFlags.SetMotionBlur(false);
	FPostProcessSettings& PP = CaptureComponent->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_DynamicGlobalIlluminationMethod = true;
	PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PP.bOverride_ReflectionMethod = true;
	PP.ReflectionMethod = EReflectionMethod::None;

	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	for (UPointLightComponent* Light : { KeyLight.Get(), FillLight.Get() })
	{
		Light->SetCastShadows(false);
		Light->bUseInverseSquaredFalloff = false;
		Light->LightFalloffExponent = 1.f;
		Light->LightingChannels.bChannel0 = false;
		Light->LightingChannels.bChannel1 = true;
	}
	KeyLight->SetIntensity(8.f);
	FillLight->SetIntensity(3.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PlaceholderMesh = CubeAsset.Object;
	DisplayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DisplayMaterialPackage + TEXT(".M_UI_PreviewStage")));
}

void ATAPreviewStage::UsePreviewLightingChannel(UPrimitiveComponent* Primitive)
{
	// 通道 0 是关卡默认通道：关掉它，关卡灯光照不到预览模型，预览灯光也照不到关卡
	Primitive->LightingChannels.bChannel0 = false;
	Primitive->LightingChannels.bChannel1 = true;
}

ATAPreviewStage* ATAPreviewStage::SpawnPreviewStage(UObject* WorldContext, TSubclassOf<ATAPreviewStage> StageClass, int32 Slot)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location = StageOrigin + FVector(Slot * StageSpacing, 0.f, 0.f);
	return World->SpawnActor<ATAPreviewStage>(StageClass ? StageClass.Get() : StaticClass(), Location, FRotator::ZeroRotator, Params);
}

void ATAPreviewStage::SetDisplayMesh(UStaticMesh* Mesh, const FTransform& DisplayTransform)
{
	bShowingPlaceholder = Mesh == nullptr;
	if (bShowingPlaceholder)
	{
		UE_LOG(LogTAPreviewStage, Warning, TEXT("%s: no mesh to display, showing placeholder."), *GetName());
		Mesh = PlaceholderMesh;
	}
	ApplyMesh(Mesh, nullptr, DisplayTransform);
}

void ATAPreviewStage::SetDisplaySkeletalMesh(USkeletalMesh* Mesh, const FTransform& DisplayTransform)
{
	if (!Mesh)
	{
		SetDisplayMesh(nullptr, DisplayTransform);
		return;
	}
	bShowingPlaceholder = false;
	ApplyMesh(nullptr, Mesh, DisplayTransform);
}

void ATAPreviewStage::SetDisplayModel(UObject* Model, const FTransform& DisplayTransform)
{
	if (USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Model))
	{
		SetDisplaySkeletalMesh(Skeletal, DisplayTransform);
		return;
	}
	if (Model && !Cast<UStaticMesh>(Model))
	{
		UE_LOG(LogTAPreviewStage, Warning, TEXT("%s: '%s' is a %s, not a static or skeletal mesh."),
			*GetName(), *Model->GetName(), *Model->GetClass()->GetName());
	}
	SetDisplayMesh(Cast<UStaticMesh>(Model), DisplayTransform);
}

void ATAPreviewStage::ClearDisplay()
{
	bShowingPlaceholder = false;
	ApplyMesh(nullptr, nullptr, FTransform::Identity);
	SetCaptureEnabled(false);
}

bool ATAPreviewStage::HasModel() const
{
	return MeshComponent->GetStaticMesh() || SkeletalMeshComponent->GetSkeletalMeshAsset();
}

void ATAPreviewStage::ApplyMesh(UStaticMesh* StaticMesh, USkeletalMesh* SkeletalMesh, const FTransform& DisplayTransform)
{
	PivotRotation = FQuat::Identity;
	PivotComponent->SetRelativeRotation(PivotRotation);
	MeshComponent->SetStaticMesh(StaticMesh);
	MeshComponent->SetVisibility(StaticMesh != nullptr);
	SkeletalMeshComponent->SetSkeletalMeshAsset(SkeletalMesh);
	SkeletalMeshComponent->SetVisibility(SkeletalMesh != nullptr);

	// 预览台的组件会反复换模型：先清掉上一个模型留下的材质覆盖，再给丢失的材质补兜底材质
	MeshComponent->EmptyOverrideMaterials();
	SkeletalMeshComponent->EmptyOverrideMaterials();
	UTAItemSettings::ApplyMissingMaterialFallback(MeshComponent);
	UTAItemSettings::ApplyMissingMaterialFallback(SkeletalMeshComponent);
	UPrimitiveComponent* Active = StaticMesh ? static_cast<UPrimitiveComponent*>(MeshComponent) : SkeletalMesh ? SkeletalMeshComponent.Get() : nullptr;
	ModelRadius = 50.f;
	if (Active)
	{
		// 先把模型包围球中心移到旋转中心，DisplayTransform 的位移在此基础上再偏移
		const FBoxSphereBounds Bounds = StaticMesh ? StaticMesh->GetBounds() : SkeletalMesh->GetBounds();
		FTransform Relative = DisplayTransform;
		Relative.SetLocation(DisplayTransform.GetLocation() - DisplayTransform.TransformVector(Bounds.Origin));
		Active->SetRelativeTransform(Relative);
		ModelRadius = FMath::Max(1.f, Bounds.SphereRadius * DisplayTransform.GetScale3D().GetAbsMax());
	}

	CaptureComponent->ShowOnlyComponents.Reset();
	CaptureComponent->ShowOnlyComponent(MeshComponent);
	CaptureComponent->ShowOnlyComponent(SkeletalMeshComponent);
	CaptureComponent->ShowOnlyComponent(BackdropComponent);
	UpdateFraming();
}

void ATAPreviewStage::UpdateFraming()
{
	const float Radius = ModelRadius;

	// 球体刚好塞进视野：距离 = 半径 / sin(半视角)
	const float HalfFov = FMath::DegreesToRadians(FMath::Clamp(FieldOfView, 5.f, 170.f) * 0.5f);
	const float Distance = Radius * FramingPadding / FMath::Sin(HalfFov);
	const FRotator ViewRotation(CameraPitch, 0.f, 0.f);
	CaptureComponent->FOVAngle = FieldOfView;
	CaptureComponent->SetRelativeLocationAndRotation(-ViewRotation.Vector() * Distance, ViewRotation);
	CaptureComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CaptureComponent->PostProcessSettings.AutoExposureBias = ExposureCompensation;
	// 透明模式（SceneColorHDR）不走后处理，曝光在显示材质里乘
	if (DisplayMaterialInstance)
	{
		DisplayMaterialInstance->SetScalarParameterValue(DisplayExposureParameter, FMath::Pow(2.f, ExposureCompensation));
	}

	// 主光在相机左上方，补光在右侧，衰减半径覆盖整个模型
	KeyLight->SetRelativeLocation(FVector(-Distance * 0.8f, -Distance * 0.6f, Distance * 0.8f));
	FillLight->SetRelativeLocation(FVector(-Distance * 0.6f, Distance * 0.8f, 0.f));
	KeyLight->SetAttenuationRadius(Distance * 4.f);
	FillLight->SetAttenuationRadius(Distance * 4.f);
}

void ATAPreviewStage::SetCaptureEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		GetRenderTarget();
	}
	CaptureComponent->bCaptureEveryFrame = bEnabled;
}

bool ATAPreviewStage::IsCaptureEnabled() const
{
	return CaptureComponent->bCaptureEveryFrame;
}

void ATAPreviewStage::AddManualRotation(float DeltaYaw, float DeltaPitch)
{
	// 两个轴都在预览台坐标系里固定：左右绕竖直轴，上下绕镜头的水平轴（预览台 Y 轴），
	// 不随模型当前朝向变化；用四元数累计，上下可以无限翻转，不会在 ±90° 卡住
	const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(DeltaYaw));
	const FQuat Pitch(FVector::RightVector, FMath::DegreesToRadians(DeltaPitch));
	PivotRotation = (Yaw * Pitch * PivotRotation).GetNormalized();
	PivotComponent->SetRelativeRotation(PivotRotation);
}

UTextureRenderTarget2D* ATAPreviewStage::GetRenderTarget()
{
	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("PreviewRenderTarget"));
		const int32 SizeX = FMath::Max(1, RenderTargetSize.X);
		const int32 SizeY = FMath::Max(1, RenderTargetSize.Y);
		if (bTransparentBackground)
		{
			// 反向不透明度：清屏 A = 1 即完全透明
			RenderTarget->ClearColor = FLinearColor(0.f, 0.f, 0.f, 1.f);
			RenderTarget->InitCustomFormat(SizeX, SizeY, PF_FloatRGBA, true);
			CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
		}
		else
		{
			RenderTarget->ClearColor = FLinearColor::Black;
			RenderTarget->InitCustomFormat(SizeX, SizeY, PF_B8G8R8A8, false);
			CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		}
		CaptureComponent->TextureTarget = RenderTarget;
	}
	return RenderTarget;
}

void ATAPreviewStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// 世界时间：时停只冻结挂了冻结组件的 Actor，这里照常旋转
	if (bAutoRotate && HasModel())
	{
		AddManualRotation(RotationSpeed * DeltaSeconds, 0.f);
	}
}

UMaterialInstanceDynamic* ATAPreviewStage::GetDisplayMaterial()
{
	if (!bTransparentBackground)
	{
		return nullptr;
	}
	if (!DisplayMaterialInstance)
	{
		UMaterialInterface* Base = DisplayMaterial.LoadSynchronous();
		if (!Base)
		{
			UE_LOG(LogTAPreviewStage, Warning, TEXT("%s: display material '%s' missing; run -run=TAPreviewMaterial."),
				*GetName(), *DisplayMaterial.ToString());
			return nullptr;
		}
		DisplayMaterialInstance = UMaterialInstanceDynamic::Create(Base, this);
		DisplayMaterialInstance->SetTextureParameterValue(DisplayTextureParameter, GetRenderTarget());
		DisplayMaterialInstance->SetScalarParameterValue(DisplayExposureParameter, FMath::Pow(2.f, ExposureCompensation));
	}
	return DisplayMaterialInstance;
}
