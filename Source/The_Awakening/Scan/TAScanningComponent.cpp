// Fill out your copyright notice in the Description page of Project Settings.


#include "Scan/TAScanningComponent.h"

#include "Scan/TAScanningActor.h"
#include "Scan/TA_HighlightPPActor.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Curves/CurveFloat.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "GameFramework/PlayerController.h"
#include "The_AwakeningPlayerController.h"
#include "Scan/TAScannableComponent.h"
#include "Scan/TAScanInfoWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Core/TAInputIconSubsystem.h"

// Sets default values for this component's properties
UTAScanningComponent::UTAScanningComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	//找Material Parameter Collection
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> MPCRef(
		TEXT("/Game/Materials/Collections/MPC_Scan.MPC_Scan")
	);

	//找到了喵
	if (MPCRef.Succeeded())
	{
		ScanParameterCollection = MPCRef.Object;
	}

	// Blend Curve
	static ConstructorHelpers::FObjectFinder<UCurveFloat> BlendCurveRef(
		TEXT("/Game/Materials/Scan/Curves/CRV_Scan_Blend.CRV_Scan_Blend")
	);

	if (BlendCurveRef.Succeeded())
	{
		BlendCurve = BlendCurveRef.Object;
	}

	// Post Process Material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PostProcessMaterialRef(
		TEXT("/Game/Materials/Scan/MI_Scan_PostProcess.MI_Scan_PostProcess")
	);

	if (PostProcessMaterialRef.Succeeded())
	{
		PostProcessMaterial = PostProcessMaterialRef.Object;
	}

}


// Called when the game starts
void UTAScanningComponent::BeginPlay()
{
	Super::BeginPlay();

	PlayerController = Cast<APlayerController>(GetOwner());

	if (PlayerController)
	{
		UpdateScanState(ETAScanState::FadedOut, true);

		UKismetMaterialLibrary::SetScalarParameterValue(
			GetWorld(),
			ScanParameterCollection,
			FName("GridCellSize"),
			GridCellSize
		);

		UKismetMaterialLibrary::SetScalarParameterValue(
			GetWorld(),
			ScanParameterCollection,
			FName("Range"),
			Range
		);

		UKismetMaterialLibrary::SetScalarParameterValue(
			GetWorld(),
			ScanParameterCollection,
			FName("Highlight"),
			0.0f
		);
	}
	else
	{
		UpdateScanState(ETAScanState::Invalid, true);
	}
	
}


void UTAScanningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsScanning())
	{
		StopScan(ETAScanEndReason::ComponentDestroyed, false);
	}
	ClearHighlightedTargets();
	SetHoveredTarget(nullptr);
	DestroyHighlightPPActor();
	DestroyScanPPActor();
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void UTAScanningComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateHighlight(DeltaTime);
	UpdateScanTime(DeltaTime);
	UpdateScanDuration(DeltaTime);
	if (IsScanning())
	{
		TargetRefreshAccumulator += DeltaTime;
		if (TargetRefreshAccumulator >= FMath::Max(TargetRefreshInterval, 0.02f))
		{
			TargetRefreshAccumulator = 0.0f;
			RefreshScanTargets();
		}
		UpdateHoveredTarget();
		UpdateScanInfoWidgetPosition();
	}
	/*
	//debug print time
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.0f,
			FColor::Green,
			FString::Printf(TEXT("ScanNormalizedTime: %.3f"), ScanNormalizedTime)
		);
	}
	*/
	switch (ScanState)
	{
		case ETAScanState::FadeIn:
		case ETAScanState::FadeOut:
			UpdateScanPPBlendWeight();
			break;

		case ETAScanState::FadedIn:
		case ETAScanState::FadedOut:
			break;


		case ETAScanState::Invalid:
			return;
	}

	if (!PlayerController)
	{
		return;
	}

	APawn* ControlledPawn = PlayerController->GetPawn();

	if (!ControlledPawn)
	{
		return;
	}

	const FVector PlayerLocation = ControlledPawn->GetActorLocation();

	UKismetMaterialLibrary::SetVectorParameterValue(
		GetWorld(),
		ScanParameterCollection,
		FName("Location"),
		FLinearColor(
			PlayerLocation.X,
			PlayerLocation.Y,
			PlayerLocation.Z,
			1.0f
		)
	);
	const FVector SnappedLocation = PlayerLocation.GridSnap(GridCellSize);

	// 更新 Niagara 位置
	if (IsValid(ScanActor))
	{
		UNiagaraComponent* ScanNiagara = ScanActor->GetNiagaraComponent();

		if (IsValid(ScanNiagara))
		{
			ScanNiagara->SetWorldLocation(
				SnappedLocation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics
			);
		}
	}
}

ETAScanState UTAScanningComponent::GetScanState() const
{
	return ScanState;
}

bool UTAScanningComponent::UpdateScanState(ETAScanState NewState, bool bForce)
{
	if (NewState == ScanState && !bForce)
	{
		return false;
	}
	//离开旧状态
	switch (ScanState)
	{
		case ETAScanState::FadedIn:
			UKismetMaterialLibrary::SetScalarParameterValue(
				GetWorld(),
				ScanParameterCollection,
				FName("bFadedIn"),
				0.0f
			);
			break;

		case ETAScanState::FadedOut:
			UKismetMaterialLibrary::SetScalarParameterValue(
				GetWorld(),
				ScanParameterCollection,
				FName("bFadedOut"),
				0.0f
			);
			SetComponentTickEnabled(true);

			if (UMaterialParameterCollectionInstance* MPCInstance =
				GetWorld()->GetParameterCollectionInstance(ScanParameterCollection))
			{
				float UpgradedValue = 0.0f;

				if (MPCInstance->GetScalarParameterValue(
					FName("bUpgraded"),
					UpgradedValue))
				{
					if (ATAScanningActor* CurrentScanActor = GetScanPPActor())
					{
						if (UNiagaraComponent* ScanNiagara =
							CurrentScanActor->GetNiagaraComponent())
						{
							ScanNiagara->SetVisibility(UpgradedValue > 0.0f);
						}
					}
				}
			}

			break;
		
		default:
			break;
	}
	//更新状态
	ScanState = NewState;

	//进入新状态
	switch (ScanState)
	{
		case ETAScanState::FadedIn:
			UKismetMaterialLibrary::SetScalarParameterValue(
				GetWorld(),
				ScanParameterCollection,
				FName("bFadedIn"),
				1.0f
			);

			UpdateScanPPBlendWeight();
			break;

		case ETAScanState::FadedOut:
			UKismetMaterialLibrary::SetScalarParameterValue(
				GetWorld(),
				ScanParameterCollection,
				FName("bFadedOut"),
				1.0f
			);

			UpdateScanPPBlendWeight();

			if (ATAScanningActor* CurrentScanActor = GetScanPPActor())
			{
				if (UNiagaraComponent* ScanNiagara = CurrentScanActor->GetNiagaraComponent())
				{
					ScanNiagara->SetVisibility(false);
				}
			}

			DestroyScanPPActor();

			if (HighlightShowProgress <= 0.0f)
			{
				SetComponentTickEnabled(false);
			}
			break;
		/*
		case ETAScanState::Invalid:
			SetComponentTickEnabled(false);
			DestroyScanPPActor();
			break;
*/
		default:
			break;
	}

	return true;

}

bool UTAScanningComponent::UpdateScanTime(float DeltaTime)
{
	switch (ScanState)
	{
	case ETAScanState::FadeIn:
		ScanNormalizedTime += DeltaTime * FadeInSpeed;

		if (ScanNormalizedTime >= 1.0f)
		{
			ScanNormalizedTime = 1.0f;
			UpdateScanState(ETAScanState::FadedIn, false);
		}

		break;


	case ETAScanState::FadeOut:
		ScanNormalizedTime -= DeltaTime * FadeOutSpeed;

		if (ScanNormalizedTime <= 0.0f)
		{
			ScanNormalizedTime = 0.0f;
			UpdateScanState(ETAScanState::FadedOut, false);
		}

		break;


	case ETAScanState::FadedIn:
	case ETAScanState::FadedOut:
	case ETAScanState::Invalid:
		return true;

	default:
		break;
	}


	UKismetMaterialLibrary::SetScalarParameterValue(
		GetWorld(),
		ScanParameterCollection,
		FName("NormalizedTime"),
		ScanNormalizedTime
	);

	return true;
}

ATAScanningActor* UTAScanningComponent::GetScanPPActor()
{
	if (IsValid(ScanActor))
	{
		return ScanActor;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	// Spawn Scan Actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();

	ScanActor = GetWorld()->SpawnActor<ATAScanningActor>(
		ATAScanningActor::StaticClass(),
		FTransform::Identity,
		SpawnParams
	);

	if (!IsValid(ScanActor))
	{
		return nullptr;
	}

	// 如果还没有 MID，就根据 PostProcessMaterial 创建
	if (!IsValid(PostProcessMID))
	{
		if (!IsValid(PostProcessMaterial))
		{
			return ScanActor;
		}

		PostProcessMID = UMaterialInstanceDynamic::Create(
			PostProcessMaterial,
			this
		);
	}

	if (!IsValid(PostProcessMID))
	{
		return ScanActor;
	}

	// 获取 Scan Actor 里的 Post Process Component
	UPostProcessComponent* PostProcessComponent =
		ScanActor->GetPostProcessComponent();

	if (!IsValid(PostProcessComponent))
	{
		return ScanActor;
	}

	// 对应蓝图：
	// Make Weighted Blendable
	// Weight = 1
	// Object = PPMID
	FWeightedBlendable WeightedBlendable;
	WeightedBlendable.Weight = 1.0f;
	WeightedBlendable.Object = PostProcessMID;

	// 对应 Set Members in Post Process Settings
	PostProcessComponent->Settings.WeightedBlendables.Array.Empty();
	PostProcessComponent->Settings.WeightedBlendables.Array.Add(WeightedBlendable);

	return ScanActor;
}

void UTAScanningComponent::DestroyScanPPActor()
{
	if (IsValid(ScanActor) && bDestroyActor)
	{
		ScanActor->Destroy();
		ScanActor = nullptr;
	}
}

void UTAScanningComponent::UpdateScanPPBlendWeight()
{
	float BlendWeight = ScanNormalizedTime;

	if (IsValid(BlendCurve))
	{
		BlendWeight = BlendCurve->GetFloatValue(ScanNormalizedTime);
	}

	ATAScanningActor* CurrentScanActor = GetScanPPActor();

	if (!IsValid(CurrentScanActor))
	{
		return;
	}

	UPostProcessComponent* PostProcessComponent =
		CurrentScanActor->GetPostProcessComponent();

	if (!IsValid(PostProcessComponent))
	{
		return;
	}

	PostProcessComponent->BlendWeight = BlendWeight;
}


void UTAScanningComponent::UpdateHighlight(float DeltaTime)
{
	// --------------------------------------------------
	// Highlight Actor 已经存在：
	// 处理 FadeIn / FadeOut
	// --------------------------------------------------
	if (IsValid(HighlightPPActor))
	{
		if (HighlightFadeTime <= 0.0f)
		{
			if (bHighlightFadingIn)
			{
				HighlightValue = 1.0f;
				bHighlightFadingIn = false;
			}
			else if (bHighlightFadingOut)
			{
				HighlightValue = 0.0f;
				bHighlightFadingOut = false;

				UKismetMaterialLibrary::SetScalarParameterValue(
					GetWorld(),
					ScanParameterCollection,
					FName("Highlight"),
					HighlightValue
				);

				DestroyHighlightPPActor();
				return;
			}
		}
		else
		{
			const float FadeSpeed = 1.0f / HighlightFadeTime;

			if (bHighlightFadingIn)
			{
				HighlightValue += DeltaTime * FadeSpeed;

				if (HighlightValue >= 1.0f)
				{
					HighlightValue = 1.0f;
					bHighlightFadingIn = false;
				}
			}
			else if (bHighlightFadingOut)
			{
				HighlightValue -= DeltaTime * FadeSpeed;

				if (HighlightValue <= 0.0f)
				{
					HighlightValue = 0.0f;
					bHighlightFadingOut = false;

					UKismetMaterialLibrary::SetScalarParameterValue(
						GetWorld(),
						ScanParameterCollection,
						FName("Highlight"),
						HighlightValue
					);

					DestroyHighlightPPActor();
					return;
				}
			}
		}

		UKismetMaterialLibrary::SetScalarParameterValue(
			GetWorld(),
			ScanParameterCollection,
			FName("Highlight"),
			HighlightValue
		);

		return;
	}

	// --------------------------------------------------
	// Highlight Actor 不存在：
	// 处理 ShowDelay
	// --------------------------------------------------

	const bool bScanning =
		ScanState == ETAScanState::FadeIn ||
		ScanState == ETAScanState::FadedIn;

	if (bScanning)
	{
		HighlightShowProgress += DeltaTime;

		if (HighlightShowProgress >= ShowDelay)
		{
			HighlightShowProgress = ShowDelay;
			GetHighlightPPActor();
		}
	}
	else
	{
		HighlightShowProgress -= DeltaTime;

		HighlightShowProgress = FMath::Max(
			HighlightShowProgress,
			0.0f
		);

		if (HighlightShowProgress <= 0.0f &&
			ScanState == ETAScanState::FadedOut)
		{
			SetComponentTickEnabled(false);
		}
	}
}

ATA_HighlightPPActor* UTAScanningComponent::GetHighlightPPActor()
{
	if (IsValid(HighlightPPActor))
	{
		return HighlightPPActor;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();

	HighlightPPActor = GetWorld()->SpawnActor<ATA_HighlightPPActor>(
		ATA_HighlightPPActor::StaticClass(),
		FTransform::Identity,
		SpawnParams
	);

	if (IsValid(HighlightPPActor))
	{
		HighlightValue = 0.0f;

		UKismetMaterialLibrary::SetScalarParameterValue(
			GetWorld(),
			ScanParameterCollection,
			FName("Highlight"),
			HighlightValue
		);

		BeginHighlightFadeIn();
	}

	return HighlightPPActor;
}

void UTAScanningComponent::BeginHighlightFadeIn()
{
	if (!IsValid(HighlightPPActor))
	{
		return;
	}

	bHighlightFadingOut = false;

	if (HighlightValue >= 1.0f)
	{
		HighlightValue = 1.0f;
		bHighlightFadingIn = false;
		return;
	}

	bHighlightFadingIn = true;
	SetComponentTickEnabled(true);
}

void UTAScanningComponent::BeginHighlightFadeOut()
{
	if (!IsValid(HighlightPPActor))
	{
		return;
	}

	bHighlightFadingIn = false;

	if (HighlightValue <= 0.0f)
	{
		HighlightValue = 0.0f;
		bHighlightFadingOut = false;
		DestroyHighlightPPActor();
		return;
	}

	bHighlightFadingOut = true;
	SetComponentTickEnabled(true);
}

void UTAScanningComponent::DestroyHighlightPPActor()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(
			HighlightHideTimerHandle
		);
	}

	if (IsValid(HighlightPPActor))
	{
		HighlightPPActor->Destroy();
		HighlightPPActor = nullptr;
	}

	HighlightValue = 0.0f;
	HighlightShowProgress = 0.0f;

	bHighlightFadingIn = false;
	bHighlightFadingOut = false;

	if (GetWorld() && IsValid(ScanParameterCollection))
	{
		UKismetMaterialLibrary::SetScalarParameterValue(
			GetWorld(),
			ScanParameterCollection,
			FName("Highlight"),
			0.0f
		);
	}
}

void UTAScanningComponent::SetShowDelay(float NewShowDelay)
{
	ShowDelay = FMath::Max(0.0f, NewShowDelay);
}

void UTAScanningComponent::SetHideDelay(float NewHideDelay)
{
	HideDelay = FMath::Max(0.0f, NewHideDelay);
}

void UTAScanningComponent::StartHighlightHideTimer()
{
	if (!IsValid(HighlightPPActor) || !GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(
		HighlightHideTimerHandle
	);

	bHighlightFadingOut = false;

	// HideDelay 比 FadeTime 还短：
	// 直接开始 FadeOut
	if (HideDelay <= HighlightFadeTime)
	{
		BeginHighlightFadeOut();
		return;
	}

	// HideDelay 的最后 HighlightFadeTime 秒开始 FadeOut
	const float TimeBeforeFadeOut =
		HideDelay - HighlightFadeTime;

	GetWorld()->GetTimerManager().SetTimer(
		HighlightHideTimerHandle,
		this,
		&UTAScanningComponent::BeginHighlightFadeOut,
		TimeBeforeFadeOut,
		false
	);
}

void UTAScanningComponent::CancelHighlightHideTimer()
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(
		HighlightHideTimerHandle
	);

	if (IsValid(HighlightPPActor))
	{
		BeginHighlightFadeIn();
	}
}

bool UTAScanningComponent::StartScan()
{
	bScanInputHeld = true;
	if (bWaitingForInputRelease || !PlayerController || !PlayerController->GetPawn())
	{
		return false;
	}
	if (const AThe_AwakeningPlayerController* TAController = Cast<AThe_AwakeningPlayerController>(PlayerController);
		TAController && TAController->IsUIInputModeActive())
	{
		return false;
	}

	switch (ScanState)
	{
		case ETAScanState::FadeOut:
		case ETAScanState::FadedOut:
		{
			// 如果 Highlight 正在等待 Hide，
			// 重新扫描就取消 HideDelay
			if (IsValid(HighlightPPActor))
			{
				CancelHighlightHideTimer();
			}

			if (AThe_AwakeningPlayerController* TAController = Cast<AThe_AwakeningPlayerController>(PlayerController))
			{
				TAController->BeginScanCursorMode();
			}
			else
			{
				PlayerController->SetShowMouseCursor(true);
			}

			ScanRemainingTime = FMath::Max(MaxScanDuration, 0.1f);
			TargetRefreshAccumulator = TargetRefreshInterval;
			const bool bStarted = UpdateScanState(
				ETAScanState::FadeIn,
				false
			);
			if (bStarted)
			{
				OnScanStarted.Broadcast();
				OnScanTimeChanged.Broadcast(ScanRemainingTime, GetScanRemainingRatio());
				OnScanAudioIntensityChanged.Broadcast(GetScanAudioIntensity());
				BP_OnScanAudioStarted();
			}
			return bStarted;
		}

		case ETAScanState::FadeIn:
		case ETAScanState::FadedIn:
		case ETAScanState::Invalid:
			return false;
	}

	return false;
}

bool UTAScanningComponent::EndScan()
{
	const bool bWasScanning = IsScanning();
	NotifyScanInputReleased(false);
	return bWasScanning;
}

void UTAScanningComponent::NotifyScanInputReleased(bool bCanceled)
{
	bScanInputHeld = false;
	bWaitingForInputRelease = false;
	if (IsScanning())
	{
		StopScan(bCanceled ? ETAScanEndReason::Canceled : ETAScanEndReason::Released, false);
	}
}

void UTAScanningComponent::CancelScan(ETAScanEndReason Reason, bool bRequireInputRelease)
{
	if (IsScanning())
	{
		StopScan(Reason, bRequireInputRelease);
	}
}

bool UTAScanningComponent::IsScanning() const
{
	return ScanState == ETAScanState::FadeIn || ScanState == ETAScanState::FadedIn;
}

float UTAScanningComponent::GetScanRemainingRatio() const
{
	return MaxScanDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(ScanRemainingTime / MaxScanDuration, 0.0f, 1.0f)
		: 0.0f;
}

float UTAScanningComponent::GetScanAudioIntensity() const
{
	const float ElapsedRatio = 1.0f - GetScanRemainingRatio();
	return AudioIntensityCurve ? AudioIntensityCurve->GetFloatValue(ElapsedRatio) : ElapsedRatio;
}

void UTAScanningComponent::StopScan(ETAScanEndReason Reason, bool bRequireInputRelease)
{
	if (!IsScanning())
	{
		return;
	}

	bWaitingForInputRelease = bRequireInputRelease && bScanInputHeld;
	if (IsValid(HighlightPPActor))
	{
		StartHighlightHideTimer();
	}

	if (AThe_AwakeningPlayerController* TAController = Cast<AThe_AwakeningPlayerController>(PlayerController))
	{
		TAController->EndScanCursorMode();
	}
	else if (PlayerController)
	{
		PlayerController->SetShowMouseCursor(false);
	}

	ClearHighlightedTargets();
	SetHoveredTarget(nullptr);
	if (ScanInfoWidget)
	{
		ScanInfoWidget->RemoveFromParent();
		ScanInfoWidget = nullptr;
	}

	UpdateScanState(ETAScanState::FadeOut, false);
	OnScanEnded.Broadcast(Reason);
	BP_OnScanAudioEnded(Reason);
}

void UTAScanningComponent::UpdateScanDuration(float DeltaTime)
{
	if (!IsScanning())
	{
		return;
	}
	if (!PlayerController || !PlayerController->GetPawn())
	{
		StopScan(ETAScanEndReason::OwnerInvalid, true);
		return;
	}

	ScanRemainingTime = FMath::Max(0.0f, ScanRemainingTime - DeltaTime);
	OnScanTimeChanged.Broadcast(ScanRemainingTime, GetScanRemainingRatio());
	const float AudioIntensity = GetScanAudioIntensity();
	OnScanAudioIntensityChanged.Broadcast(AudioIntensity);
	BP_OnScanAudioUpdated(AudioIntensity);
	if (ScanRemainingTime <= 0.0f)
	{
		StopScan(ETAScanEndReason::TimedOut, true);
	}
}

void UTAScanningComponent::RefreshScanTargets()
{
	if (!GetWorld() || !PlayerController || !PlayerController->GetPawn())
	{
		return;
	}

	const FVector Origin = PlayerController->GetPawn()->GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TAScanTargets), false, PlayerController->GetPawn());
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeSphere(FMath::Max(Range, 1.0f)),
		QueryParams);

	TSet<TWeakObjectPtr<UTAScannableComponent>> NewTargets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UTAScannableComponent* Target = Actor ? Actor->FindComponentByClass<UTAScannableComponent>() : nullptr;
		if (!IsValid(Target) || !Target->CanBeScanned())
		{
			continue;
		}

		const float Distance = FVector::Distance(Origin, Actor->GetActorLocation());
		const bool bVisibleForHighlight = Target->bAllowHighlightThroughWalls || HasLineOfSightToTarget(Target);
		if (Distance <= Target->HighlightDistance && bVisibleForHighlight)
		{
			NewTargets.Add(Target);
		}
	}

	for (const TWeakObjectPtr<UTAScannableComponent>& Existing : HighlightedTargets)
	{
		if (!NewTargets.Contains(Existing))
		{
			if (UTAScannableComponent* Target = Existing.Get())
			{
				Target->SetScanHighlightEnabled(false);
			}
		}
	}
	for (const TWeakObjectPtr<UTAScannableComponent>& Added : NewTargets)
	{
		if (!HighlightedTargets.Contains(Added))
		{
			if (UTAScannableComponent* Target = Added.Get())
			{
				Target->SetScanHighlightEnabled(true);
			}
		}
	}
	HighlightedTargets = MoveTemp(NewTargets);
}

bool UTAScanningComponent::HasLineOfSightToTarget(const UTAScannableComponent* Target) const
{
	if (!Target || !Target->GetOwner() || !GetWorld() || !PlayerController || !PlayerController->PlayerCameraManager)
	{
		return false;
	}

	const FVector Start = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector End = Target->GetOwner()->GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TAScanVisibility), true, PlayerController->GetPawn());
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, VisibilityTraceChannel, Params);
	return !bHit || Hit.GetActor() == Target->GetOwner();
}

void UTAScanningComponent::UpdateHoveredTarget()
{
	if (!PlayerController || !PlayerController->GetPawn())
	{
		SetHoveredTarget(nullptr);
		return;
	}

	float CursorX = 0.0f;
	float CursorY = 0.0f;
	const UTAInputIconSubsystem* InputIcons = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UTAInputIconSubsystem>() : nullptr;
	const bool bUseScreenCenter = InputIcons && InputIcons->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
	if (bUseScreenCenter || !PlayerController->GetMousePosition(CursorX, CursorY))
	{
		int32 SizeX = 0;
		int32 SizeY = 0;
		PlayerController->GetViewportSize(SizeX, SizeY);
		CursorX = SizeX * 0.5f;
		CursorY = SizeY * 0.5f;
	}

	UTAScannableComponent* BestTarget = nullptr;
	float BestScreenDistanceSquared = FMath::Square(HoverPixelRadius);
	const FVector PawnLocation = PlayerController->GetPawn()->GetActorLocation();
	for (const TWeakObjectPtr<UTAScannableComponent>& CandidatePtr : HighlightedTargets)
	{
		UTAScannableComponent* Candidate = CandidatePtr.Get();
		AActor* Actor = Candidate ? Candidate->GetOwner() : nullptr;
		if (!Actor || FVector::Distance(PawnLocation, Actor->GetActorLocation()) > Candidate->InformationDistance)
		{
			continue;
		}
		if (!Candidate->bAllowInformationThroughWalls && !HasLineOfSightToTarget(Candidate))
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(Actor->GetActorLocation(), ScreenPosition, true))
		{
			continue;
		}
		const float DistanceSquared = FVector2D::DistSquared(ScreenPosition, FVector2D(CursorX, CursorY));
		if (DistanceSquared <= BestScreenDistanceSquared)
		{
			BestScreenDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}
	SetHoveredTarget(BestTarget);
}

void UTAScanningComponent::SetHoveredTarget(UTAScannableComponent* NewTarget)
{
	if (NewTarget && HoveredTarget.Get() == NewTarget)
	{
		return;
	}
	if (!NewTarget && !HoveredTarget.IsValid() &&
		(!ScanInfoWidget || ScanInfoWidget->GetVisibility() == ESlateVisibility::Collapsed))
	{
		return;
	}

	HoveredTarget = NewTarget;
	OnHoveredTargetChanged.Broadcast(NewTarget);
	if (!NewTarget)
	{
		if (ScanInfoWidget)
		{
			ScanInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (!ScanInfoWidget && ScanInfoWidgetClass && PlayerController)
	{
		ScanInfoWidget = CreateWidget<UTAScanInfoWidget>(PlayerController, ScanInfoWidgetClass);
		if (ScanInfoWidget)
		{
			ScanInfoWidget->AddToPlayerScreen(100);
			ScanInfoWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
		}
	}
	if (ScanInfoWidget)
	{
		ScanInfoWidget->SetTargetInfo(NewTarget->GetScanInfo());
	}
}

void UTAScanningComponent::UpdateScanInfoWidgetPosition()
{
	if (!ScanInfoWidget || !HoveredTarget.IsValid() || !PlayerController)
	{
		return;
	}

	float CursorX = 0.0f;
	float CursorY = 0.0f;
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PlayerController->GetViewportSize(ViewportX, ViewportY);
	const UTAInputIconSubsystem* InputIcons = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UTAInputIconSubsystem>() : nullptr;
	const bool bUseScreenCenter = InputIcons && InputIcons->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
	if (bUseScreenCenter || !PlayerController->GetMousePosition(CursorX, CursorY))
	{
		CursorX = ViewportX * 0.5f;
		CursorY = ViewportY * 0.5f;
	}
	const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(PlayerController), KINDA_SMALL_NUMBER);
	ScanInfoWidget->ForceLayoutPrepass();
	const FVector2D DesiredSize = ScanInfoWidget->GetDesiredSize();
	const FVector2D LogicalViewport(ViewportX / ViewportScale, ViewportY / ViewportScale);
	FVector2D Position(CursorX / ViewportScale + ScanInfoCursorOffset.X, CursorY / ViewportScale + ScanInfoCursorOffset.Y);
	Position.X = FMath::Clamp(Position.X, 0.0f, FMath::Max(0.0f, LogicalViewport.X - DesiredSize.X));
	Position.Y = FMath::Clamp(Position.Y, 0.0f, FMath::Max(0.0f, LogicalViewport.Y - DesiredSize.Y));
	ScanInfoWidget->SetPositionInViewport(Position, false);
}

void UTAScanningComponent::ClearHighlightedTargets()
{
	for (const TWeakObjectPtr<UTAScannableComponent>& TargetPtr : HighlightedTargets)
	{
		if (UTAScannableComponent* Target = TargetPtr.Get())
		{
			Target->SetScanHighlightEnabled(false);
		}
	}
	HighlightedTargets.Reset();
}

void UTAScanningComponent::UpgradeHighlight() 
{
	if (GetWorld() && IsValid(ScanParameterCollection))
	{
		UKismetMaterialLibrary::SetScalarParameterValue(
			GetWorld(),
			ScanParameterCollection,
			FName("bUpgraded"),
			1.0f
		);
	}
}
