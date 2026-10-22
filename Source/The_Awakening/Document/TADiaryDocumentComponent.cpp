#include "Document/TADiaryDocumentComponent.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentPresentation.h"
#include "Document/TAPaperAnimInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Inventory/TAItemSettings.h"

UTADiaryDocumentComponent::UTADiaryDocumentComponent()
{
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticDocument"));
	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalDocument"));
	StaticMesh->SetupAttachment(this);
	SkeletalMesh->SetupAttachment(this);
	for (UPrimitiveComponent* Mesh : {static_cast<UPrimitiveComponent*>(StaticMesh), static_cast<UPrimitiveComponent*>(SkeletalMesh)})
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->LightingChannels.bChannel0 = false;
		Mesh->LightingChannels.bChannel1 = true;
		Mesh->SetVisibility(false);
	}
	// The paper physics bodies are much smaller than the visual sheet. Keep culling and picking
	// on the authored mesh bounds throughout folding and page motion.
	SkeletalMesh->bComponentUseFixedSkelBounds = true;
	SkeletalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void UTADiaryDocumentComponent::RegisterMeshes()
{
	StaticMesh->RegisterComponent();
	SkeletalMesh->RegisterComponent();
	// Explicit sampling keeps all document poses on the same presentation clock as the book.
	SkeletalMesh->SetComponentTickEnabled(false);
}

void UTADiaryDocumentComponent::Configure(UTADocumentDefinition* InDefinition, float MaxSize)
{
	if (Definition == InDefinition) return;
	Definition = InDefinition;
	StaticMesh->SetStaticMesh(nullptr);
	SkeletalMesh->SetSkeletalMeshAsset(nullptr);
	SkeletalMesh->SetAnimInstanceClass(nullptr);
	StaticMesh->EmptyOverrideMaterials();
	SkeletalMesh->EmptyOverrideMaterials();
	StaticMesh->SetVisibility(false);
	SkeletalMesh->SetVisibility(false);
	if (!Definition) { SetVisibility(false, true); return; }
	SetVisibility(true);
	UStaticMesh* Static = Cast<UStaticMesh>(Definition->Mesh);
	USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Definition->Mesh);
	FTransform Model = Definition->GetPreviewTransform(Definition->DiaryPageTransform);
	if (!Static && !Skeletal)
	{
		// Preserve the existing placeholder policy until the separate default-model migration.
		Static = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Model = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.003f, 0.1f, 0.14f));
	}
	// Presentation correction faces -X in the existing reader; the book surface faces +Z.
	FacingRotation = Model.GetRotation();
	if (Definition->Presentation)
		FacingRotation = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Definition->Presentation->FoldedDiaryRotation)) * FacingRotation;
	Model.SetRotation(FQuat(FRotator(-90.f, 0.f, 0.f)) * FacingRotation);
	const FBoxSphereBounds ModelBounds = Skeletal ? Skeletal->GetBounds() : Static->GetBounds();
	const FBox Oriented = ModelBounds.GetBox().TransformBy(FTransform(Model.GetRotation(), FVector::ZeroVector, Model.GetScale3D()));
	const FVector Size = Oriented.GetSize();
	const float Scale = MaxSize / FMath::Max(0.01f, FMath::Max(Size.X, Size.Y));
	Model.SetScale3D(Model.GetScale3D() * Scale);
	Model.SetLocation(-Model.TransformVector(ModelBounds.Origin));
	if (Skeletal)
	{
		SkeletalMesh->SetSkeletalMeshAsset(Skeletal);
		SkeletalMesh->SetRelativeTransform(Model);
		SkeletalMesh->SetVisibility(true);
		if (Definition->Presentation) SkeletalMesh->SetAnimInstanceClass(Definition->Presentation->AnimationClass);
		UTAItemSettings::ApplyMissingMaterialFallback(SkeletalMesh);
		Definition->ApplyPaperMaterials(SkeletalMesh);
		SetOpenness(0.f);
	}
	else
	{
		StaticMesh->SetStaticMesh(Static);
		StaticMesh->SetRelativeTransform(Model);
		StaticMesh->SetVisibility(true);
		UTAItemSettings::ApplyMissingMaterialFallback(StaticMesh);
		Definition->ApplyPaperMaterials(StaticMesh);
	}
}

void UTADiaryDocumentComponent::SetOpenness(float Value)
{
	if (auto* Paper = Cast<UTAPaperAnimInstance>(SkeletalMesh->GetAnimInstance())) Paper->SetOpenness(Value);
	if (SkeletalMesh->GetSkeletalMeshAsset())
	{
		SkeletalMesh->TickAnimation(0.f, false);
		SkeletalMesh->RefreshBoneTransforms();
		SkeletalMesh->UpdateBounds();
		SkeletalMesh->MarkRenderDynamicDataDirty();
	}
}

float UTADiaryDocumentComponent::GetFoldDuration() const
{
	const auto* Paper = Cast<UTAPaperAnimInstance>(SkeletalMesh->GetAnimInstance());
	return Paper ? Paper->GetFoldDuration() : 0.f;
}

FBox UTADiaryDocumentComponent::GetWorldBox() const
{
	if (SkeletalMesh->GetSkeletalMeshAsset()) return SkeletalMesh->Bounds.GetBox();
	if (StaticMesh->GetStaticMesh()) return StaticMesh->Bounds.GetBox();
	return FBox(ForceInit);
}
