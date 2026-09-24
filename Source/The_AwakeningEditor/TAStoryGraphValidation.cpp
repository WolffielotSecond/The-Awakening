#include "TAStoryGraphValidation.h"
#include "Story/TADialogueTypes.h"

#define LOCTEXT_NAMESPACE "TAStoryGraphValidation"

bool TAStoryGraphValidation::ValidateEndPaths(const FTAStoryData& Story, FText& OutError, FString& OutNodeId)
{
	OutError = FText::GetEmpty();
	OutNodeId.Reset();
	TMap<FString, int32> Indices;
	for (int32 Index = 0; Index < Story.Nodes.Num(); ++Index)
	{
		Indices.Add(Story.Nodes[Index].Id, Index);
	}
	TArray<TArray<int32>> Predecessors;
	Predecessors.SetNum(Story.Nodes.Num());
	TArray<int32> RemainingOutputs;
	RemainingOutputs.Init(0, Story.Nodes.Num());
	TArray<int32> Finished;
	for (int32 Index = 0; Index < Story.Nodes.Num(); ++Index)
	{
		const FTAStoryNode& Node = Story.Nodes[Index];
		if (Node.Type == TEXT("end"))
		{
			Finished.Add(Index);
			continue;
		}
		auto AddTarget = [&](const FString& Target, const FText& OutputName)
		{
			const int32* TargetIndex = Indices.Find(Target);
			if (!TargetIndex)
			{
				OutNodeId = Node.Id;
				OutError = FText::Format(LOCTEXT("MissingTarget",
					"Compile failed: node {0}, output {1} has no valid target. Connect every branch to an End node."),
					FText::FromString(Node.Id), OutputName);
				return false;
			}
			Predecessors[*TargetIndex].Add(Index);
			++RemainingOutputs[Index];
			return true;
		};
		if (Node.Type == TEXT("dialogue"))
		{
			if (!AddTarget(Node.Next, LOCTEXT("Next", "Next")))
			{
				return false;
			}
		}
		else if (Node.Type == TEXT("choice") && !Node.Choices.IsEmpty())
		{
			for (int32 ChoiceIndex = 0; ChoiceIndex < Node.Choices.Num(); ++ChoiceIndex)
			{
				if (!AddTarget(Node.Choices[ChoiceIndex].Target,
					FText::Format(LOCTEXT("ChoiceOutput", "Choice {0}"), FText::AsNumber(ChoiceIndex + 1))))
				{
					return false;
				}
			}
		}
		else
		{
			OutNodeId = Node.Id;
			OutError = FText::Format(LOCTEXT("DeadEnd", "Compile failed: node {0} is a dead end. Every path must finish at an End node."), FText::FromString(Node.Id));
			return false;
		}
	}

	// A node is finished only when ALL of its outputs are known to terminate.
	// The iterative pass also rejects loops with an optional exit, without recursion.
	for (int32 QueueIndex = 0; QueueIndex < Finished.Num(); ++QueueIndex)
	{
		for (int32 Previous : Predecessors[Finished[QueueIndex]])
		{
			if (--RemainingOutputs[Previous] == 0)
			{
				Finished.Add(Previous);
			}
		}
	}
	for (int32 Index = 0; Index < RemainingOutputs.Num(); ++Index)
	{
		if (RemainingOutputs[Index] > 0)
		{
			OutNodeId = Story.Nodes[Index].Id;
			OutError = FText::Format(LOCTEXT("Cycle", "Compile failed: node {0} belongs to or leads into a cycle. Every path must terminate at End."), FText::FromString(OutNodeId));
			return false;
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
