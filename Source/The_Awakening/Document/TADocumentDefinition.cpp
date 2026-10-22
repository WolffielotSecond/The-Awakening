#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentPresentation.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UTADocumentDefinition::UTADocumentDefinition()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Front(TEXT("/Game/Models/Document/M_Document_Front.M_Document_Front"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Back(TEXT("/Game/Models/Document/M_Document_Back.M_Document_Back"));
	PaperFrontMaterial = Front.Object;
	PaperBackMaterial = Back.Object;
}

void UTADocumentDefinition::ApplyPaperMaterials(UMeshComponent* Component) const
{
	if (!Component) return;
	const TArray<FName> Slots = Component->GetMaterialSlotNames();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index] == TEXT("M_Paper_Front") || Slots[Index] == TEXT("M_Document_Front"))
			Component->SetMaterial(Index, PaperFrontMaterial);
		else if (Slots[Index] == TEXT("M_Paper_Back") || Slots[Index] == TEXT("M_Document_Back"))
			Component->SetMaterial(Index, PaperBackMaterial);
	}
}

FTransform UTADocumentDefinition::GetPreviewTransform(const FTransform& Adjustment) const
{
	return Presentation ? FTransform(Presentation->PreviewRotation) * Adjustment : Adjustment;
}
