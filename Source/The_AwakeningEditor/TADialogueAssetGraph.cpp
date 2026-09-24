#include "TADialogueAssetGraph.h"
#include "TAStoryGraphClipboard.h"
#include "EdGraph/EdGraphPin.h"
#include "ScopedTransaction.h"
#include "Framework/Commands/GenericCommands.h"
#include "GraphEditorActions.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "TADialogueAssetGraph"

namespace
{
	struct FTADialogueAddNodeAction : public FEdGraphSchemaAction
	{
		ETADialogueGraphNodeType NodeType;

		FTADialogueAddNodeAction(const FText& Category, const FText& Label, const FText& Tooltip, ETADialogueGraphNodeType InNodeType)
			: FEdGraphSchemaAction(Category, Label, Tooltip, 0), NodeType(InNodeType) {}

		virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2f& Location, bool bSelectNewNode) override
		{
			if (!ParentGraph)
			{
				return nullptr;
			}

		const FScopedTransaction Transaction(LOCTEXT("AddStoryNode", "Add Story Node"));
		ParentGraph->Modify();
		UTAStoryGraphNode* Node = NewObject<UTAStoryGraphNode>(ParentGraph, NAME_None, RF_Transactional);
		Node->NodeType = NodeType;
		Node->NodeData.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Node->NodeData.Type = NodeType == ETADialogueGraphNodeType::Choice ? TEXT("choice")
			: NodeType == ETADialogueGraphNodeType::End ? TEXT("end") : TEXT("dialogue");
		if (NodeType == ETADialogueGraphNodeType::Choice)
		{
			Node->NodeData.Choices.SetNum(2);
		}
		Node->NodePosX = Location.X;
		Node->NodePosY = Location.Y;
		ParentGraph->AddNode(Node, true, bSelectNewNode);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		if (FromPin)
		{
			UEdGraphPin* TargetPin = FromPin->Direction == EGPD_Output
				? Node->GetFlowInputPin()
				: Node->GetFlowOutputPin(NodeType == ETADialogueGraphNodeType::Choice ? 0 : INDEX_NONE);
			if (TargetPin)
			{
				ParentGraph->GetSchema()->TryCreateConnection(FromPin, TargetPin);
			}
		}
		return Node;
		}
	};

	struct FTAStoryPasteNodesAction : public FEdGraphSchemaAction
	{
		FTAStoryPasteNodesAction()
			: FEdGraphSchemaAction(LOCTEXT("ClipboardCategory", "Clipboard"), LOCTEXT("PasteNodesAction", "Paste Nodes Here"),
				LOCTEXT("PasteNodesTooltip", "Paste copied story nodes at this location."), 0) {}

		virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin,
			const FVector2f& Location, bool bSelectNewNode = true) override
		{
			const TSet<UEdGraphNode*> PastedNodes = TAStoryGraphClipboard::PasteNodes(ParentGraph, Location, bSelectNewNode);
			for (UEdGraphNode* Node : PastedNodes)
			{
				return Node;
			}
			return nullptr;
		}
	};
}

void UTAStoryGraphNode::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	if (NodeType != ETADialogueGraphNodeType::Entry && NodeData.Id.IsEmpty())
	{
		NodeData.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
	if (NodeType == ETADialogueGraphNodeType::Choice && NodeData.Choices.IsEmpty())
	{
		NodeData.Choices.SetNum(2);
	}
}

bool UTAStoryGraphNode::CanUserDeleteNode() const
{
	return NodeType != ETADialogueGraphNodeType::Entry;
}

bool UTAStoryGraphNode::CanDuplicateNode() const
{
	return NodeType != ETADialogueGraphNodeType::Entry;
}

bool UTAStoryGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema && Schema->IsA<UTAStoryGraphSchema>();
}

void UTAStoryGraphNode::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();
	if (GetGraph())
	{
		GetGraph()->NotifyGraphChanged();
	}
}

void UTAStoryGraphNode::AllocateDefaultPins()
{
	const FName FlowCategory(TEXT("StoryFlow"));
	if (NodeType != ETADialogueGraphNodeType::Entry)
	{
		CreatePin(EGPD_Input, FlowCategory, FName(TEXT("In")));
	}

	if (NodeType == ETADialogueGraphNodeType::Dialogue)
	{
		CreatePin(EGPD_Output, FlowCategory, FName(TEXT("Next")));
	}
	else if (NodeType == ETADialogueGraphNodeType::Choice)
	{
		for (int32 Index = 0; Index < NodeData.Choices.Num(); ++Index)
		{
			CreatePin(EGPD_Output, FlowCategory, *FString::Printf(TEXT("Choice_%d"), Index));
		}
	}
	else if (NodeType == ETADialogueGraphNodeType::Entry)
	{
		CreatePin(EGPD_Output, FlowCategory, FName(TEXT("Entry")));
	}
}

#if WITH_EDITOR
void UTAStoryGraphNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName ChangedProperty = PropertyChangedEvent.GetPropertyName();
	const bool bChoiceCountChanged = ChangedProperty == GET_MEMBER_NAME_CHECKED(FTAStoryNode, Choices);
	if (bChoiceCountChanged && NodeType == ETADialogueGraphNodeType::Choice)
	{
		SynchronizeChoicePins(PropertyChangedEvent);
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (GetGraph())
	{
		GetGraph()->NotifyGraphChanged();
	}
}

void UTAStoryGraphNode::PostEditUndo()
{
	Super::PostEditUndo();
	if (GetGraph())
	{
		GetGraph()->NotifyGraphChanged();
	}
}
#endif

void UTAStoryGraphNode::SynchronizeChoicePins(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Modify();
	TArray<UEdGraphPin*> ChoicePins;
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output)
		{
			ChoicePins.Add(Pin);
		}
	}

	const int32 NewCount = NodeData.Choices.Num();
	const int32 ChangedIndex = PropertyChangedEvent.GetArrayIndex(TEXT("Choices"));
	auto RemoveChoicePin = [this, &ChoicePins](int32 Index)
	{
		UEdGraphPin* RemovedPin = ChoicePins[Index];
		ChoicePins.RemoveAt(Index);
		RemovedPin->BreakAllPinLinks(true);
		RemovePin(RemovedPin);
	};

	if ((PropertyChangedEvent.ChangeType & EPropertyChangeType::ArrayRemove)
		&& ChoicePins.Num() == NewCount + 1 && ChoicePins.IsValidIndex(ChangedIndex))
	{
		// Remove the selected branch, preserving the actual pins (and links) of later options.
		RemoveChoicePin(ChangedIndex);
	}
	else if ((PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayAdd | EPropertyChangeType::Duplicate))
		&& NewCount == ChoicePins.Num() + 1 && ChangedIndex >= 0 && ChangedIndex <= ChoicePins.Num())
	{
		UEdGraphPin* NewPin = CreatePin(EGPD_Output, FName(TEXT("StoryFlow")), NAME_None);
		ChoicePins.Insert(NewPin, ChangedIndex);
	}

	while (ChoicePins.Num() > NewCount)
	{
		RemoveChoicePin(ChoicePins.Num() - 1);
	}
	while (ChoicePins.Num() < NewCount)
	{
		ChoicePins.Add(CreatePin(EGPD_Output, FName(TEXT("StoryFlow")), NAME_None));
	}
	for (int32 Index = 0; Index < ChoicePins.Num(); ++Index)
	{
		ChoicePins[Index]->PinName = FName(*FString::Printf(TEXT("Choice_%d"), Index));
	}

	// Keep the visual pin order consistent with the array after insertion or deletion.
	Pins.RemoveAll([](const UEdGraphPin* Pin) { return Pin && Pin->Direction == EGPD_Output; });
	Pins.Append(ChoicePins);
}

UEdGraphPin* UTAStoryGraphNode::GetFlowOutputPin(int32 ChoiceIndex) const
{
	const FName WantedName = NodeType == ETADialogueGraphNodeType::Choice
		? FName(*FString::Printf(TEXT("Choice_%d"), ChoiceIndex))
		: (NodeType == ETADialogueGraphNodeType::Entry ? FName(TEXT("Entry")) : FName(TEXT("Next")));
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->Direction == EGPD_Output && Pin->PinName == WantedName)
		{
			return Pin;
		}
	}
	return nullptr;
}

UEdGraphPin* UTAStoryGraphNode::GetFlowInputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			return Pin;
		}
	}
	return nullptr;
}

FText UTAStoryGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	(void)TitleType;
	switch (NodeType)
	{
	case ETADialogueGraphNodeType::Entry:
		return LOCTEXT("EntryNodeTitle", "Entry");
	case ETADialogueGraphNodeType::Dialogue:
		return FText::Format(LOCTEXT("DialogueNodeTitle", "Dialogue: {0}"), FText::FromString(NodeData.Id));
	case ETADialogueGraphNodeType::Choice:
		return FText::Format(LOCTEXT("ChoiceNodeTitle", "Choices: {0}"), FText::FromString(NodeData.Id));
	case ETADialogueGraphNodeType::End:
		return FText::Format(LOCTEXT("EndNodeTitle", "End: {0}"), FText::FromString(NodeData.Id));
	default:
		return LOCTEXT("UnknownNodeTitle", "Story Node");
	}
}

FLinearColor UTAStoryGraphNode::GetNodeTitleColor() const
{
	if (NodeType == ETADialogueGraphNodeType::Entry || NodeType == ETADialogueGraphNodeType::End)
	{
		return FLinearColor(0.65f, 0.04f, 0.04f);
	}
	if (NodeType == ETADialogueGraphNodeType::Choice)
	{
		return FLinearColor(0.08f, 0.5f, 0.12f);
	}
	return Super::GetNodeTitleColor();
}

FLinearColor UTAStoryGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

const FPinConnectionResponse UTAStoryGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if (!A || !B || A == B || A->Direction == B->Direction || A->PinType.PinCategory != B->PinType.PinCategory)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("InvalidConnection", "Connect an output to an input."));
	}

	const UEdGraphPin* Output = A->Direction == EGPD_Output ? A : B;
	if (Output->LinkedTo.Num() > 0)
	{
		return FPinConnectionResponse(Output == A ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B,
			LOCTEXT("ReplaceOutputConnection", "Replace the existing output connection."));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ValidConnection", "Connect story nodes."));
}

bool UTAStoryGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	FScopedTransaction Transaction(LOCTEXT("ConnectStoryNodes", "Connect Story Nodes"));
	if (Super::TryCreateConnection(A, B))
	{
		A->GetOwningNode()->GetGraph()->NotifyGraphChanged();
		return true;
	}
	Transaction.Cancel();
	return false;
}

void UTAStoryGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	ContextMenuBuilder.AddAction(MakeShared<FTADialogueAddNodeAction>(LOCTEXT("NodeCategory", "Story Nodes"), LOCTEXT("DialogueAction", "Dialogue"), LOCTEXT("DialogueActionTooltip", "Add a dialogue line."), ETADialogueGraphNodeType::Dialogue));
	ContextMenuBuilder.AddAction(MakeShared<FTADialogueAddNodeAction>(LOCTEXT("NodeCategory", "Story Nodes"), LOCTEXT("ChoiceAction", "Choice"), LOCTEXT("ChoiceActionTooltip", "Add a choice branch."), ETADialogueGraphNodeType::Choice));
	ContextMenuBuilder.AddAction(MakeShared<FTADialogueAddNodeAction>(LOCTEXT("NodeCategory", "Story Nodes"), LOCTEXT("EndAction", "End"), LOCTEXT("EndActionTooltip", "Add an ending node."), ETADialogueGraphNodeType::End));
	if (!ContextMenuBuilder.FromPin && TAStoryGraphClipboard::CanPaste(ContextMenuBuilder.CurrentGraph))
	{
		ContextMenuBuilder.AddAction(MakeShared<FTAStoryPasteNodesAction>());
	}
}

void UTAStoryGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetContextMenuActions(Menu, Context);
	if (!Context || Context->bIsDebugging || !Context->Node || Context->Pin)
	{
		return;
	}

	FToolMenuSection& Section = Menu->AddSection(TEXT("StoryNodeActions"), LOCTEXT("StoryNodeActions", "Node Actions"));
	Section.AddMenuEntry(FGenericCommands::Get().Copy);
	Section.AddMenuEntry(FGenericCommands::Get().Paste);
	Section.AddMenuEntry(FGenericCommands::Get().Delete);
	Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
}

void UTAStoryGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(LOCTEXT("BreakStoryPinLinks", "Break Story Pin Links"));
	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);
}

#undef LOCTEXT_NAMESPACE
