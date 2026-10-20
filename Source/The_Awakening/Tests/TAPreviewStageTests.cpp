#include "Document/TAPreviewStage.h"
#include "Core/TATimeSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Tests/TAHealthTestListener.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace TAPreviewStageTests
{
	struct FFixture
	{
		UWorld* World = nullptr;

		FFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPreviewStageDisplayTest, "TheAwakening.Document.PreviewStage.Display",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPreviewStageDisplayTest::RunTest(const FString&)
{
	TAPreviewStageTests::FFixture F;
	ATAPreviewStage* Stage0 = ATAPreviewStage::SpawnPreviewStage(F.World, nullptr, 0);
	ATAPreviewStage* Stage1 = ATAPreviewStage::SpawnPreviewStage(F.World, nullptr, 1);
	if (!TestNotNull(TEXT("Stage spawned"), Stage0) || !TestNotNull(TEXT("Second stage spawned"), Stage1)) return false;
	TestTrue(TEXT("Stages are far below the level"), Stage0->GetActorLocation().Z < -10000.f);
	TestTrue(TEXT("Slots do not overlap"), FVector::Dist(Stage0->GetActorLocation(), Stage1->GetActorLocation()) >= ATAPreviewStage::StageSpacing);

	// 模型缺失：占位立方体 + Warning
	AddExpectedMessage(TEXT("no mesh to display, showing placeholder"), EAutomationExpectedErrorFlags::Contains, 1, false);
	Stage0->SetDisplayMesh(nullptr, FTransform::Identity);
	TestTrue(TEXT("Placeholder flagged"), Stage0->IsShowingPlaceholder());
	TestTrue(TEXT("Placeholder mesh shown"), Stage0->GetMeshComponent()->GetStaticMesh() == Stage0->PlaceholderMesh && Stage0->PlaceholderMesh != nullptr);

	// 真实模型：包围球中心落在旋转中心 + DisplayTransform 位移上
	UStaticMesh* Cube = Stage0->PlaceholderMesh;
	const FTransform Display(FRotator(0.f, 30.f, 0.f), FVector(0.f, 0.f, 10.f), FVector(2.f));
	Stage0->SetDisplayMesh(Cube, Display);
	TestFalse(TEXT("Real mesh is not a placeholder"), Stage0->IsShowingPlaceholder());
	UStaticMeshComponent* Mesh = Stage0->GetMeshComponent();
	Mesh->UpdateComponentToWorld();
	const FVector Center = Mesh->CalcBounds(Mesh->GetComponentTransform()).Origin;
	const FVector Expected = Stage0->GetPivotComponent()->GetComponentLocation() + FVector(0.f, 0.f, 10.f);
	TestTrue(TEXT("Mesh centered on pivot plus offset"), Center.Equals(Expected, 0.5f));

	// 取景：相机到中心的距离足够装下包围球
	USceneCaptureComponent2D* Capture = Stage0->GetCaptureComponent();
	const float Radius = Cube->GetBounds().SphereRadius * 2.f;
	const float Distance = Capture->GetRelativeLocation().Size();
	TestTrue(TEXT("Camera far enough to fit the mesh"), Distance * FMath::Sin(FMath::DegreesToRadians(Capture->FOVAngle * 0.5f)) >= Radius - 0.1f);
	TestTrue(TEXT("Only the stage is captured"), Capture->PrimitiveRenderMode == ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList
		&& Capture->ShowOnlyComponents.Contains(Mesh));
	TestFalse(TEXT("Mesh does not use the level lighting channel"), Mesh->LightingChannels.bChannel0);

	// 渲染目标
	TestFalse(TEXT("Capture off by default"), Stage0->IsCaptureEnabled());
	Stage0->SetCaptureEnabled(true);
	UTextureRenderTarget2D* RT = Stage0->GetRenderTarget();
	TestTrue(TEXT("Render target created and bound"), RT && Capture->TextureTarget == RT);
	TestTrue(TEXT("Render target size"), RT && RT->SizeX == Stage0->RenderTargetSize.X && RT->SizeY == Stage0->RenderTargetSize.Y);
	TestTrue(TEXT("Same render target on later calls"), Stage0->GetRenderTarget() == RT);

	// 透明背景（默认）：SceneColorHDR + 浮点 RT，UI 用显示材质还原透明度
	TestTrue(TEXT("Transparent mode captures SceneColorHDR"), Capture->CaptureSource == ESceneCaptureSource::SCS_SceneColorHDR);
	TestTrue(TEXT("Transparent mode uses a float render target"), RT && RT->GetFormat() == PF_FloatRGBA);
	UMaterialInstanceDynamic* DisplayMID = Stage0->GetDisplayMaterial();
	if (TestNotNull(TEXT("Display material asset exists (run -run=TAPreviewMaterial)"), DisplayMID))
	{
		UTexture* Bound = nullptr;
		DisplayMID->GetTextureParameterValue(FHashedMaterialParameterInfo(ATAPreviewStage::DisplayTextureParameter), Bound);
		TestTrue(TEXT("Display material samples the render target"), Bound == RT);
		TestTrue(TEXT("Display material is cached"), Stage0->GetDisplayMaterial() == DisplayMID);
	}

	// 不透明模式：LDR，不需要显示材质
	ATAPreviewStage* Opaque = ATAPreviewStage::SpawnPreviewStage(F.World, nullptr, 2);
	Opaque->bTransparentBackground = false;
	Opaque->GetRenderTarget();
	TestTrue(TEXT("Opaque mode captures final LDR color"), Opaque->GetCaptureComponent()->CaptureSource == ESceneCaptureSource::SCS_FinalColorLDR);
	TestNull(TEXT("Opaque mode has no display material"), Opaque->GetDisplayMaterial());

	Stage0->ClearDisplay();
	TestFalse(TEXT("Clear hides the mesh"), Mesh->IsVisible());
	TestFalse(TEXT("Clear stops capturing"), Stage0->IsCaptureEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPreviewStageRotationTest, "TheAwakening.Document.PreviewStage.Rotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPreviewStageRotationTest::RunTest(const FString&)
{
	TAPreviewStageTests::FFixture F;
	ATAPreviewStage* Stage = ATAPreviewStage::SpawnPreviewStage(F.World, nullptr, 0);
	if (!TestNotNull(TEXT("Stage spawned"), Stage)) return false;
	USceneComponent* Pivot = Stage->GetPivotComponent();

	Stage->Tick(1.f);
	TestTrue(TEXT("No rotation without a mesh"), Pivot->GetRelativeRotation().IsNearlyZero());

	Stage->SetDisplayMesh(Stage->PlaceholderMesh, FTransform::Identity);
	Stage->RotationSpeed = 45.f;
	Stage->Tick(1.f);
	TestTrue(TEXT("Auto rotates by speed"), FMath::IsNearlyEqual(Pivot->GetRelativeRotation().Yaw, 45.f, 0.01f));

	Stage->SetAutoRotate(false);
	Stage->Tick(1.f);
	TestTrue(TEXT("Auto rotate off"), FMath::IsNearlyEqual(Pivot->GetRelativeRotation().Yaw, 45.f, 0.01f));
	Stage->AddManualRotation(10.f, 0.f);
	TestTrue(TEXT("Manual rotation"), FMath::IsNearlyEqual(Pivot->GetRelativeRotation().Yaw, 55.f, 0.01f));

	Stage->SetDisplayMesh(Stage->PlaceholderMesh, FTransform::Identity);
	TestTrue(TEXT("New mesh resets rotation"), Pivot->GetRelativeRotation().IsNearlyZero());

	// 时停只冻结挂了冻结组件的 Actor：预览台不受影响
	UTATimeSubsystem* Time = F.World->GetSubsystem<UTATimeSubsystem>();
	UObject* FreezeSource = NewObject<UTAHealthTestListener>();
	if (TestNotNull(TEXT("Time subsystem"), Time))
	{
		Time->RequestFreeze(FreezeSource, 1.f);
		TestEqual(TEXT("Stage keeps normal time dilation while frozen"), Stage->CustomTimeDilation, 1.f);
		Time->ReleaseFreeze(FreezeSource);
	}
	return true;
}

#endif
