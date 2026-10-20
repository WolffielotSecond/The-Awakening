#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentPresentation.h"

FTransform UTADocumentDefinition::GetPreviewTransform(const FTransform& Adjustment) const
{
	return Presentation ? FTransform(Presentation->PreviewRotation) * Adjustment : Adjustment;
}
