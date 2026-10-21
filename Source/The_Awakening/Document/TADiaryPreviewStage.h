#pragma once

#include "CoreMinimal.h"
#include "Document/TAPreviewStage.h"
#include "TADiaryPreviewStage.generated.h"

class UAnimSequence;
class USkeletalMesh;
class UTADocumentDefinition;
class UTADiaryDocumentComponent;

UENUM(BlueprintType)
enum class ETADiaryMotion : uint8
{
	Closed, Opening, Open, TurningForward, TurningBackward, Closing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTADiaryMotionFinished, ETADiaryMotion, CompletedMotion);

/** Book-only presentation. Document collection, navigation and input belong to the diary widget. */
UCLASS(Blueprintable)
class THE_AWAKENING_API ATADiaryPreviewStage : public ATAPreviewStage
{
	GENERATED_BODY()
public:
	ATADiaryPreviewStage();

	UFUNCTION(BlueprintCallable, Category = "Diary", meta = (WorldContext = "WorldContext"))
	static ATADiaryPreviewStage* SpawnDiaryStage(UObject* WorldContext);

	/** Loads and validates the modular meshes and the four book animations. */
	UFUNCTION(BlueprintCallable, Category = "Diary")
	bool InitializeDiary();
	UFUNCTION(BlueprintCallable, Category = "Diary")
	bool OpenBook();
	UFUNCTION(BlueprintCallable, Category = "Diary")
	bool CloseBook();
	UFUNCTION(BlueprintCallable, Category = "Diary")
	bool TurnPage(bool bForward);
	/** Quick-view/forced cleanup: sample a stable endpoint without emitting completion. */
	UFUNCTION(BlueprintCallable, Category = "Diary")
	void SetOpenImmediate(bool bOpen);
	UFUNCTION(BlueprintPure, Category = "Diary")
	ETADiaryMotion GetMotion() const { return Motion; }
	UFUNCTION(BlueprintPure, Category = "Diary")
	bool IsReady() const { return bReady; }
	bool IsPresentationReady() const { return bFirstPosePublished; }
	UFUNCTION(BlueprintPure, Category = "Diary")
	float GetMotionProgress() const;
	void SetPages(UTADocumentDefinition* Left, UTADocumentDefinition* Right);
	bool BeginPageTurn(bool bForward, UTADocumentDefinition* TargetLeft, UTADocumentDefinition* TargetRight);
	bool BeginReading();
	void UpdateReading(float Travel, float Openness, float Orientation);
	void EndReading();
	float GetDocumentFoldDuration() const;
	bool IsReading() const { return bReading; }
	FBox2D GetRightDocumentScreenBounds() const;
	/** Project the two visible paper surfaces, including the closed-book offset. */
	FBox2D GetBookSurfaceScreenBounds() const;
	UTADiaryDocumentComponent* GetLeftDocument() const { return LeftDocument; }
	UTADiaryDocumentComponent* GetRightDocument() const { return RightDocument; }
	UTADiaryDocumentComponent* GetMovingDocument() const { return MovingDocument; }
	virtual void AddManualRotation(float DeltaYaw, float DeltaPitch) override;
	virtual void SetManualRotation(const FQuat& Rotation) override;
	virtual FQuat GetManualRotation() const override { return DocumentRotation; }
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Documents", meta = (ClampMin = "1"))
	float DocumentSize = 14.f;
	/** Center of the folded document in book mesh coordinates (centimeters). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Documents")
	FVector RightDocumentPosition = FVector(7.7f, 0.f, 1.25f);

	UPROPERTY(BlueprintAssignable, Category = "Diary")
	FTADiaryMotionFinished OnMotionFinished;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Assets")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Assets")
	TSoftObjectPtr<USkeletalMesh> PageMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Assets")
	TSoftObjectPtr<UAnimSequence> OpenAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Assets")
	TSoftObjectPtr<UAnimSequence> CloseAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Assets")
	TSoftObjectPtr<UAnimSequence> ForwardAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary|Assets")
	TSoftObjectPtr<UAnimSequence> BackwardAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diary", meta = (ClampMin = "0.01"))
	float PlaybackRate = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Diary")
	FTransform BookTransform = FTransform(FRotator(0.f, 90.f, 0.f));

	USkeletalMeshComponent* GetTurningPageComponent() const { return TurningPageComponent; }
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> TurningPageComponent;

private:
	void UpdateDocumentFrames();
	void InitializeDocumentFrames();
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTADiaryDocumentComponent> LeftDocument;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTADiaryDocumentComponent> RightDocument;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTADiaryDocumentComponent> MovingDocument;
	UPROPERTY(Transient)
	TObjectPtr<UTADocumentDefinition> CurrentLeft;
	UPROPERTY(Transient)
	TObjectPtr<UTADocumentDefinition> CurrentRight;
	UPROPERTY(Transient)
	TObjectPtr<UTADocumentDefinition> PendingLeft;
	UPROPERTY(Transient)
	TObjectPtr<UTADocumentDefinition> PendingRight;
	FTransform RightSeatInBone;
	FTransform LeftSeatInBone;
	FTransform MovingSeatInBone;
	FTransform ReadStart;
	FTransform ReadEnd;
	FTransform ReadPose;
	FQuat DocumentRotation = FQuat::Identity;
	bool bTurningDocuments = false;
	bool bReading = false;
	bool StartMotion(ETADiaryMotion NewMotion, UAnimSequence* Animation);
	void SamplePose(UAnimSequence* Animation, float Time);
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActiveAnimation;
	/** Keep all four clips alive even when only an endpoint is being displayed. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequence>> LoadedAnimations;
	ETADiaryMotion Motion = ETADiaryMotion::Closed;
	float MotionTime = 0.f;
	bool bReady = false;
	bool bFirstPosePublished = false;
	uint64 InitializationFrame = 0;
};
