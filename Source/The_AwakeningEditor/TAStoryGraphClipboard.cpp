#include "TAStoryGraphClipboard.h"

#include "TADialogueAssetGraph.h"
#include "EdGraphUtilities.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"
#include "SNodePanel.h"

namespace
{
	bool CanImportStoryNodes(const UEdGraph* Graph, const FString& Text)
	{
		return Graph && Graph->bEditable && Graph->GetSchema()
			&& Graph->GetSchema()->IsA<UTAStoryGraphSchema>()
			&& Text.Contains(UTAStoryGraphNode::StaticClass()->GetPathName())
			&& FEdGraphUtilities::CanImportNodesFromText(Graph, Text);
	}
}

void TAStoryGraphClipboard::CopyNodes(const TSet<UObject*>& Selection)
{
	TSet<UObject*> NodesToCopy;
	for (UObject* Object : Selection)
	{
		if (UTAStoryGraphNode* Node = Cast<UTAStoryGraphNode>(Object); Node && Node->CanDuplicateNode())
		{
			Node->PrepareForCopying();
			NodesToCopy.Add(Node);
		}
	}
	if (!NodesToCopy.IsEmpty())
	{
		FString ExportedText;
		FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);
		FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
	}
}

bool TAStoryGraphClipboard::CanPaste(const UEdGraph* Graph)
{
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	return CanImportStoryNodes(Graph, ClipboardText);
}

TSet<UEdGraphNode*> TAStoryGraphClipboard::PasteNodes(UEdGraph* Graph, const FVector2f& Location, bool bSelectNewNodes)
{
	TSet<UEdGraphNode*> PastedNodes;
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
	if (!CanImportStoryNodes(Graph, ClipboardText))
	{
		return PastedNodes;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("TAStoryGraph", "PasteNodes", "Paste Story Nodes"));
	Graph->Modify();
	Graph->GetOuter()->Modify();
	FEdGraphUtilities::ImportNodesFromText(Graph, ClipboardText, PastedNodes);
	for (auto It = PastedNodes.CreateIterator(); It; ++It)
	{
		UTAStoryGraphNode* Node = Cast<UTAStoryGraphNode>(*It);
		if (!Node || !Node->CanDuplicateNode())
		{
			// Entry stays unique, even if clipboard text includes it.
			(*It)->DestroyNode();
			It.RemoveCurrent();
		}
	}
	if (PastedNodes.IsEmpty())
	{
		return PastedNodes;
	}

	FVector2f Center = FVector2f::ZeroVector;
	for (UEdGraphNode* Node : PastedNodes)
	{
		Center += FVector2f(Node->NodePosX, Node->NodePosY);
	}
	Center /= static_cast<float>(PastedNodes.Num());

	TSet<const UEdGraphNode*> NewSelection;
	for (UEdGraphNode* GraphNode : PastedNodes)
	{
		UTAStoryGraphNode* Node = CastChecked<UTAStoryGraphNode>(GraphNode);
		Node->NodePosX = FMath::RoundToInt(Node->NodePosX - Center.X + Location.X);
		Node->NodePosY = FMath::RoundToInt(Node->NodePosY - Center.Y + Location.Y);
		Node->SnapToGrid(SNodePanel::GetSnapGridSize());
		Node->CreateNewGuid();
		Node->NodeData.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		// Graph links, already remapped by ImportNodesFromText, determine runtime targets.
		Node->NodeData.Next.Reset();
		for (FTAStoryChoice& Choice : Node->NodeData.Choices)
		{
			Choice.Target.Reset();
		}
		NewSelection.Add(Node);
	}
	Graph->MarkPackageDirty();
	Graph->NotifyGraphChanged();
	if (bSelectNewNodes)
	{
		Graph->SelectNodeSet(NewSelection, true);
	}
	return PastedNodes;
}
