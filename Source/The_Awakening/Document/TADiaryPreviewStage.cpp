#include "Document/TADiaryPreviewStage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Document/TADiaryDocumentComponent.h"
#include "Document/TADocumentDefinition.h"
#include "Components/StaticMeshComponent.h"

ATADiaryPreviewStage::ATADiaryPreviewStage()
{
	bAutoRotate = false;
	CameraPitch = -65.f;
	FramingPadding = 1.35f;
	RenderTargetSize = FIntPoint(1536, 1024);
	BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Models/Diary/SK_Journal_Body.SK_Journal_Body")));
	PageMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Models/Diary/SK_TurningPage.SK_TurningPage")));
	OpenAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Models/Diary/Book_Open.Book_Open")));
	CloseAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Models/Diary/Book_Close.Book_Close")));
	ForwardAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Models/Diary/Page_Turn_Forward.Page_Turn_Forward")));
	BackwardAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/Models/Diary/Page_Turn_Backward.Page_Turn_Backward")));
	TurningPageComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TurningPage"));
	TurningPageComponent->SetupAttachment(PivotComponent);
	TurningPageComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TurningPageComponent->SetCastShadow(false);
	TurningPageComponent->LightingChannels.bChannel0 = false;
	TurningPageComponent->LightingChannels.bChannel1 = true;
	TurningPageComponent->SetVisibility(false);
	LeftDocument = CreateDefaultSubobject<UTADiaryDocumentComponent>(TEXT("LeftDocument"));
	RightDocument = CreateDefaultSubobject<UTADiaryDocumentComponent>(TEXT("RightDocument"));
	MovingDocument = CreateDefaultSubobject<UTADiaryDocumentComponent>(TEXT("MovingDocument"));
	for (auto* Document : {LeftDocument.Get(), RightDocument.Get(), MovingDocument.Get()})
		Document->SetupAttachment(SceneRoot);
}

ATADiaryPreviewStage* ATADiaryPreviewStage::SpawnDiaryStage(UObject* WorldContext)
{
	auto* Stage = Cast<ATADiaryPreviewStage>(SpawnPreviewStage(WorldContext, StaticClass(), 1));
	if (Stage && !Stage->InitializeDiary())
	{
		Stage->Destroy();
		return nullptr;
	}
	return Stage;
}

bool ATADiaryPreviewStage::InitializeDiary()
{
	if (bReady) return true;
	USkeletalMesh* Body = BodyMesh.LoadSynchronous();
	USkeletalMesh* Page = PageMesh.LoadSynchronous();
	if (!Body || !Page || !Body->GetSkeleton() || Page->GetSkeleton() != Body->GetSkeleton()) return false;
	LoadedAnimations.Reset();
	for (UAnimSequence* Animation : {OpenAnimation.LoadSynchronous(), CloseAnimation.LoadSynchronous(),
		ForwardAnimation.LoadSynchronous(), BackwardAnimation.LoadSynchronous()})
	{
		if (!Animation || Animation->GetSkeleton() != Body->GetSkeleton() || Animation->GetPlayLength() <= 0.f)
		{
			UE_LOG(LogTemp, Error, TEXT("Diary preview: missing animation or incompatible skeleton."));
			return false;
		}
		LoadedAnimations.Add(Animation);
	}
	SetAutoRotate(false);
	// Frame once from the open reference mesh; never refit the camera during animation.
	SetDisplaySkeletalMesh(Body, BookTransform);
	SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// The actor samples the pose explicitly so completion, UI and bones use one clock.
	SkeletalMeshComponent->SetComponentTickEnabled(false);
	TurningPageComponent->SetSkeletalMeshAsset(Page);
	TurningPageComponent->SetRelativeTransform(SkeletalMeshComponent->GetRelativeTransform());
	TurningPageComponent->SetLeaderPoseComponent(SkeletalMeshComponent);
	CaptureComponent->ShowOnlyComponent(TurningPageComponent);
	for (auto* Document : {LeftDocument.Get(), RightDocument.Get(), MovingDocument.Get()})
	{
		Document->RegisterMeshes();
		CaptureComponent->ShowOnlyComponent(Document->GetStaticMeshComponent());
		CaptureComponent->ShowOnlyComponent(Document->GetSkeletalMeshComponent());
	}
	InitializeDocumentFrames();
	bReady = true;
	InitializationFrame = GFrameCounter;
	SetOpenImmediate(false);
	return true;
}

void ATADiaryPreviewStage::SamplePose(UAnimSequence* Animation, float Time)
{
	if (!Animation) return;
	if (SkeletalMeshComponent->GetSingleNodeInstance()->GetCurrentAsset() != Animation)
		SkeletalMeshComponent->SetAnimation(Animation);
	SkeletalMeshComponent->SetPosition(Time, false);
	SkeletalMeshComponent->GetSingleNodeInstance()->SetPlaying(false);
	SkeletalMeshComponent->TickAnimation(0.f, false);
	SkeletalMeshComponent->RefreshBoneTransforms();
	SkeletalMeshComponent->UpdateBounds();
	SkeletalMeshComponent->MarkRenderDynamicDataDirty();
	TurningPageComponent->UpdateBounds();
	if (bReady) UpdateDocumentFrames();
}

void ATADiaryPreviewStage::SetOpenImmediate(bool bOpen)
{
	if (!bReady) return;
	if (bReading) EndReading();
	if (bTurningDocuments)
	{
		bTurningDocuments = false;
		SetPages(CurrentLeft, CurrentRight);
	}
	ActiveAnimation = nullptr;
	MotionTime = 0.f;
	Motion = bOpen ? ETADiaryMotion::Open : ETADiaryMotion::Closed;
	TurningPageComponent->SetVisibility(false);
	SamplePose(OpenAnimation.Get(), bOpen ? OpenAnimation.Get()->GetPlayLength() : 0.f);
}

bool ATADiaryPreviewStage::StartMotion(ETADiaryMotion NewMotion, UAnimSequence* Animation)
{
	if (!bReady || !Animation) return false;
	Motion = NewMotion;
	ActiveAnimation = Animation;
	MotionTime = 0.f;
	TurningPageComponent->SetVisibility(NewMotion == ETADiaryMotion::TurningForward || NewMotion == ETADiaryMotion::TurningBackward);
	SamplePose(Animation, 0.f);
	return true;
}

bool ATADiaryPreviewStage::OpenBook()
{
	return Motion == ETADiaryMotion::Closed && StartMotion(ETADiaryMotion::Opening, OpenAnimation.Get());
}

float ATADiaryPreviewStage::GetMotionProgress() const
{
	return ActiveAnimation ? FMath::Clamp(MotionTime / ActiveAnimation->GetPlayLength(), 0.f, 1.f) : 1.f;
}

bool ATADiaryPreviewStage::CloseBook()
{
	return !bReading && Motion == ETADiaryMotion::Open && StartMotion(ETADiaryMotion::Closing, CloseAnimation.Get());
}

bool ATADiaryPreviewStage::TurnPage(bool bForward)
{
	return !bReading && Motion == ETADiaryMotion::Open && StartMotion(bForward ? ETADiaryMotion::TurningForward : ETADiaryMotion::TurningBackward,
		bForward ? ForwardAnimation.Get() : BackwardAnimation.Get());
}

void ATADiaryPreviewStage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!ActiveAnimation)
	{
		// A skipped opening reaches this endpoint before the first render resources are ready.
		// Keep publishing its pose, including while the shared reader controls the right document.
		if (Motion == ETADiaryMotion::Open)
		{
			SamplePose(OpenAnimation.Get(), OpenAnimation.Get()->GetPlayLength());
			bFirstPosePublished |= GFrameCounter != InitializationFrame;
		}
		return;
	}
	const float Rate = FMath::IsFinite(PlaybackRate) ? FMath::Max(0.01f, PlaybackRate) : 1.f;
	MotionTime = FMath::Min(MotionTime + FMath::Max(0.f, DeltaSeconds) * Rate, ActiveAnimation->GetPlayLength());
	SamplePose(ActiveAnimation, MotionTime);
	bFirstPosePublished |= GFrameCounter != InitializationFrame;
	if (MotionTime >= ActiveAnimation->GetPlayLength())
	{
		const ETADiaryMotion Completed = Motion;
		if (bTurningDocuments)
		{
			bTurningDocuments = false;
			SetPages(PendingLeft, PendingRight);
		}
		// Hide before resetting the reusable page, so the rewind is never rendered.
		SetOpenImmediate(Completed != ETADiaryMotion::Closing);
		OnMotionFinished.Broadcast(Completed);
	}
}

void ATADiaryPreviewStage::InitializeDocumentFrames()
{
	SamplePose(OpenAnimation.Get(), OpenAnimation.Get()->GetPlayLength());
	const FTransform Body = SkeletalMeshComponent->GetComponentTransform();
	const FTransform RightSeat = FTransform(RightDocumentPosition) * Body;
	RightSeatInBone = RightSeat.GetRelativeTransform(SkeletalMeshComponent->GetSocketTransform(TEXT("Visible_Right")));
	MovingSeatInBone = RightSeat.GetRelativeTransform(SkeletalMeshComponent->GetSocketTransform(TEXT("Document_Attach")));
	SamplePose(ForwardAnimation.Get(), ForwardAnimation.Get()->GetPlayLength());
	const FTransform LeftSeat = MovingSeatInBone * SkeletalMeshComponent->GetSocketTransform(TEXT("Document_Attach"));
	LeftSeatInBone = LeftSeat.GetRelativeTransform(SkeletalMeshComponent->GetSocketTransform(TEXT("Visible_Left")));
}

void ATADiaryPreviewStage::SetPages(UTADocumentDefinition* Left, UTADocumentDefinition* Right)
{
	CurrentLeft = Left;
	CurrentRight = Right;
	LeftDocument->Configure(Left, DocumentSize);
	RightDocument->Configure(Right, DocumentSize);
	MovingDocument->Configure(nullptr, DocumentSize);
	UpdateDocumentFrames();
}

bool ATADiaryPreviewStage::BeginPageTurn(bool bForward, UTADocumentDefinition* TargetLeft, UTADocumentDefinition* TargetRight)
{
	if (!bReady || bReading || Motion != ETADiaryMotion::Open) return false;
	PendingLeft = TargetLeft;
	PendingRight = TargetRight;
	MovingDocument->Configure(bForward ? CurrentRight.Get() : CurrentLeft.Get(), DocumentSize);
	if (bForward) RightDocument->Configure(TargetRight, DocumentSize);
	else LeftDocument->Configure(TargetLeft, DocumentSize);
	bTurningDocuments = true;
	return TurnPage(bForward);
}

void ATADiaryPreviewStage::UpdateDocumentFrames()
{
	FTransform RightPose = RightSeatInBone * SkeletalMeshComponent->GetSocketTransform(TEXT("Visible_Right"));
	FTransform LeftPose = LeftSeatInBone * SkeletalMeshComponent->GetSocketTransform(TEXT("Visible_Left"));
	if (bTurningDocuments)
	{
		const float Progress = GetMotionProgress();
		const FVector Clearance = SkeletalMeshComponent->GetComponentTransform().TransformVector(FVector(0.f, 0.f, 0.65f));
		// The incoming sheet is already underneath; the old destination sheet is covered as we land.
		const float Incoming = 1.f - FMath::SmoothStep(0.15f, 0.65f, Progress);
		const float Covered = FMath::SmoothStep(0.5f, 1.f, Progress);
		RightPose.AddToTranslation(-Clearance * (Motion == ETADiaryMotion::TurningForward ? Incoming : Covered));
		LeftPose.AddToTranslation(-Clearance * (Motion == ETADiaryMotion::TurningForward ? Covered : Incoming));
	}
	if (!bReading)
		RightDocument->SetWorldTransform(RightPose);
	LeftDocument->SetWorldTransform(LeftPose);
	bool bMovingFrontFacing = false;
	if (bTurningDocuments)
	{
		const FTransform Pose = MovingSeatInBone * SkeletalMeshComponent->GetSocketTransform(TEXT("Document_Attach"));
		// Keep the document on the original face: after the turn it belongs under the left page.
		// Do not let double-sided paper materials reveal it through the back of the turning sheet.
		const FVector ToCamera = CaptureComponent->GetComponentLocation() - Pose.GetLocation();
		bMovingFrontFacing = FVector::DotProduct(Pose.GetUnitAxis(EAxis::Z), ToCamera) > 0.f;
		MovingDocument->SetWorldTransform(Pose);
	}
	const bool bVisible = Motion != ETADiaryMotion::Closed && Motion != ETADiaryMotion::Closing;
	// Only the current (right) document is displayed at rest; keep left data for backward page motion.
	LeftDocument->SetVisibility(false, true);
	RightDocument->SetVisibility(bVisible && RightDocument->GetDefinition(), true);
	MovingDocument->SetVisibility(bVisible && bTurningDocuments && bMovingFrontFacing && MovingDocument->GetDefinition(), true);
	// Visibility propagation must not enable the unused mesh type.
	for (auto* Document : {LeftDocument.Get(), RightDocument.Get(), MovingDocument.Get()})
	{
		Document->GetStaticMeshComponent()->SetVisibility(Document->IsVisible() && Document->GetStaticMeshComponent()->GetStaticMesh());
		Document->GetSkeletalMeshComponent()->SetVisibility(Document->IsVisible() && Document->GetSkeletalMeshComponent()->GetSkeletalMeshAsset());
	}
}

bool ATADiaryPreviewStage::BeginReading()
{
	if (!bReady || bReading || Motion != ETADiaryMotion::Open || !CurrentRight) return false;
	bReading = true;
	DocumentRotation = FQuat::Identity;
	ReadStart = RightDocument->GetComponentTransform();
	const FTransform Camera = CaptureComponent->GetComponentTransform();
	const FQuat Model = FQuat(FRotator(-90.f, 0.f, 0.f)) * RightDocument->GetFacingRotation();
	const FQuat Facing = Camera.GetRotation() * CurrentRight->GetPreviewTransform(CurrentRight->ReaderTransform).GetRotation() * Model.Inverse();
	ReadEnd = FTransform(Facing, Camera.TransformPosition(FVector(40.f, 0.f, 0.f)), FVector(0.8f));
	UpdateReading(0.f, 0.f, 0.f);
	return true;
}

void ATADiaryPreviewStage::UpdateReading(float Travel, float Openness, float Orientation)
{
	if (!bReading) return;
	ReadPose.Blend(ReadStart, ReadEnd, FMath::SmoothStep(0.f, 1.f, Travel));
	ReadPose.SetRotation(FQuat::Slerp(ReadStart.GetRotation(), ReadEnd.GetRotation(), FMath::SmoothStep(0.f, 1.f, Orientation)));
	FTransform Pose = ReadPose;
	Pose.SetRotation(DocumentRotation * Pose.GetRotation());
	RightDocument->SetWorldTransform(Pose);
	RightDocument->SetOpenness(Openness);
}

void ATADiaryPreviewStage::EndReading()
{
	if (!bReading) return;
	bReading = false;
	DocumentRotation = FQuat::Identity;
	RightDocument->SetOpenness(0.f);
	UpdateDocumentFrames();
}

float ATADiaryPreviewStage::GetDocumentFoldDuration() const { return RightDocument->GetFoldDuration(); }

void ATADiaryPreviewStage::AddManualRotation(float DeltaYaw, float DeltaPitch)
{
	if (!bReading) return;
	const FTransform Camera = CaptureComponent->GetComponentTransform();
	SetManualRotation(FQuat(Camera.GetUnitAxis(EAxis::Z), FMath::DegreesToRadians(DeltaYaw))
		* FQuat(Camera.GetUnitAxis(EAxis::Y), FMath::DegreesToRadians(DeltaPitch)) * DocumentRotation);
}

void ATADiaryPreviewStage::SetManualRotation(const FQuat& Rotation)
{
	if (!bReading) return;
	DocumentRotation = Rotation.GetNormalized();
	FTransform Pose = ReadPose;
	Pose.SetRotation(DocumentRotation * Pose.GetRotation());
	RightDocument->SetWorldTransform(Pose);
}

FBox2D ATADiaryPreviewStage::GetRightDocumentScreenBounds() const
{
	FBox2D Result(ForceInit);
	if (!CurrentRight || Motion != ETADiaryMotion::Open || bReading) return Result;
	const FBox Bounds = RightDocument->GetWorldBox();
	if (!Bounds.IsValid) return Result;
	const float Tan = FMath::Tan(FMath::DegreesToRadians(CaptureComponent->FOVAngle * 0.5f));
	const float Aspect = float(RenderTargetSize.X) / FMath::Max(1, RenderTargetSize.Y);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector World((Corner & 1) ? Bounds.Max.X : Bounds.Min.X,
			(Corner & 2) ? Bounds.Max.Y : Bounds.Min.Y, (Corner & 4) ? Bounds.Max.Z : Bounds.Min.Z);
		const FVector View = CaptureComponent->GetComponentTransform().InverseTransformPosition(World);
		if (View.X <= 0.01f) return FBox2D(ForceInit);
		Result += FVector2D(0.5f + View.Y / (2.f * View.X * Tan), 0.5f - View.Z * Aspect / (2.f * View.X * Tan));
	}
	return Result;
}

FBox2D ATADiaryPreviewStage::GetBookSurfaceScreenBounds() const
{
	FBox2D Bounds(ForceInit);
	const FTransform Camera = CaptureComponent->GetComponentTransform();
	const float Tan = FMath::Tan(FMath::DegreesToRadians(CaptureComponent->FOVAngle * 0.5f));
	const float Aspect = float(RenderTargetSize.X) / FMath::Max(1, RenderTargetSize.Y);
	for (int32 Side : {-1, 1})
	{
		const FTransform Surface = SkeletalMeshComponent->GetSocketTransform(Side < 0 ? TEXT("Visible_Left") : TEXT("Visible_Right"));
		// Journal mesh dimensions relative to the inner-edge surface bones.
		for (float X : {0.f, Side * 14.8f})
			for (float Y : {-10.5f, 10.5f})
			{
				const FVector View = Camera.InverseTransformPosition(Surface.TransformPosition(FVector(X, Y, 0.f)));
				if (View.X <= 0.01f) return FBox2D(ForceInit);
				Bounds += FVector2D(0.5f + View.Y / (2.f * View.X * Tan), 0.5f - View.Z * Aspect / (2.f * View.X * Tan));
			}
	}
	return Bounds;
}
