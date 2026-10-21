#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TADiaryDocumentComponent.generated.h"

class UTADocumentDefinition;
class UStaticMeshComponent;
class USkeletalMeshComponent;

/** A document in the book scene. Owns its pose; never owns a camera or a light. */
UCLASS()
class THE_AWAKENING_API UTADiaryDocumentComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UTADiaryDocumentComponent();
	void RegisterMeshes();
	void Configure(UTADocumentDefinition* InDefinition, float MaxSize);
	void SetOpenness(float Value);
	float GetFoldDuration() const;
	FBox GetWorldBox() const;
	UTADocumentDefinition* GetDefinition() const { return Definition; }
	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMesh; }
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMesh; }
	FQuat GetFacingRotation() const { return FacingRotation; }
protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> StaticMesh;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh;
private:
	UPROPERTY(Transient)
	TObjectPtr<UTADocumentDefinition> Definition;
	FQuat FacingRotation = FQuat::Identity;
};
