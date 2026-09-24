#include "../TAStoryGraphValidation.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Story/TADialogueTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAStoryEndPathsTest, "TheAwakening.Story.EndPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTAStoryEndPathsTest::RunTest(const FString& Parameters)
{
	auto Dialogue = [](const TCHAR* Id, const TCHAR* Next)
	{
		FTAStoryNode Node;
		Node.Id = Id;
		Node.Next = Next;
		return Node;
	};
	FTAStoryNode End;
	End.Id = TEXT("End");
	End.Type = TEXT("end");
	FTAStoryNode Choice;
	Choice.Id = TEXT("Choice");
	Choice.Type = TEXT("choice");
	Choice.Choices.SetNum(2);
	Choice.Choices[0].Target = TEXT("Left");
	Choice.Choices[1].Target = TEXT("Right");

	FTAStoryData Story;
	Story.Nodes = { Choice, Dialogue(TEXT("Left"), TEXT("Merge")), Dialogue(TEXT("Right"), TEXT("Merge")),
		Dialogue(TEXT("Merge"), TEXT("End")), End };
	FText Error;
	FString ErrorNode;
	TestTrue(TEXT("Both branches can converge and finish at End"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));

	Story.Nodes[0].Choices[1].Target.Reset();
	TestFalse(TEXT("Every choice needs a target"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));
	TestEqual(TEXT("Error identifies the choice node"), ErrorNode, FString(TEXT("Choice")));
	Story.Nodes[0].Choices[1].Target = TEXT("Right");

	Story.Nodes[3].Next.Reset();
	TestFalse(TEXT("A dialogue cannot terminate implicitly"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));
	Story.Nodes[3].Next = TEXT("MissingNode");
	TestFalse(TEXT("Targets must belong to the story"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));
	Story.Nodes[3].Next = TEXT("Merge");
	TestFalse(TEXT("Self-loops cannot finish at End"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));
	Story.Nodes[3].Next = TEXT("End");

	Story.Nodes[0].Choices[0].Target = TEXT("Choice");
	TestFalse(TEXT("A loop is rejected even when another choice reaches End"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));
	Story.Nodes[0].Choices[0].Target = TEXT("Left");

	Story.Nodes.Add(Dialogue(TEXT("Unconnected"), TEXT("")));
	TestFalse(TEXT("Disconnected authoring branches must also reach End"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));
	Story.Nodes.Pop();

	Story.Nodes[0].Choices[0].Target = TEXT("End");
	Story.Nodes[0].Choices[1].Target = TEXT("End");
	TestTrue(TEXT("Several outputs may share the same End"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));
	Story.Nodes[0].Choices.Reset();
	TestFalse(TEXT("A choice without options is a dead end"), TAStoryGraphValidation::ValidateEndPaths(Story, Error, ErrorNode));
	return true;
}
#endif
