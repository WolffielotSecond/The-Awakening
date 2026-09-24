#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UEdGraphNode;

namespace TAStoryGraphClipboard
{
	void CopyNodes(const TSet<UObject*>& Selection);
	bool CanPaste(const UEdGraph* Graph);
	TSet<UEdGraphNode*> PasteNodes(UEdGraph* Graph, const FVector2f& Location, bool bSelectNewNodes = true);
}
