#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Health/TARespawnSubsystem.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_Self.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADeathPresentationBlueprintTest, "TheAwakening.Respawn.BlueprintPresentation",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADeathPresentationBlueprintTest::RunTest(const FString&)
{
 auto* Parent = LoadObject<UClass>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
 if (!TestNotNull(TEXT("Existing player character Blueprint"), Parent)) return false;
 auto* BP = FKismetEditorUtilities::CreateBlueprint(Parent, GetTransientPackage(),
  MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("DeathPresentationTest")), BPTYPE_Normal,
  UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
 if (!TestNotNull(TEXT("Temporary character child Blueprint"), BP)) return false;
 auto* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("Presentation"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
 FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
 const auto* Schema = GetDefault<UEdGraphSchema_K2>();
 int32 Y = 0;
 auto Wire = [&](UEdGraphPin* From, UEdGraphPin* To)
 { TestTrue(TEXT("Death presentation Blueprint pins connect"), From && To && Schema->TryCreateConnection(From, To)); };
 for (bool bBlackout : {false, true})
 {
  const FName EventName = bBlackout ? TEXT("PlayBlackoutPresentation") : TEXT("PlayDeathPresentation");
  auto* Function = Parent->FindFunctionByName(EventName);
  if (!TestNotNull(TEXT("Overridable presentation function"), Function)) return false;
  auto* Event = FKismetEditorUtilities::AddDefaultEventNode(BP, Graph, EventName, Function->GetOuterUClass(), Y);
  if (!TestNotNull(TEXT("Presentation override event"), Event)) return false;
  auto* Complete = NewObject<UK2Node_CallFunction>(Graph);
  Graph->AddNode(Complete); Complete->CreateNewGuid();
  Complete->SetFromFunction(UTARespawnSubsystem::StaticClass()->FindFunctionByName(
   bBlackout ? TEXT("CompleteBlackoutPresentation") : TEXT("CompleteDeathPresentation")));
  Complete->AllocateDefaultPins();
  auto* Self = NewObject<UK2Node_Self>(Graph);
  Graph->AddNode(Self); Self->CreateNewGuid(); Self->AllocateDefaultPins();
  Wire(Event->FindPin(UEdGraphSchema_K2::PN_Then), Complete->GetExecPin());
  Wire(Event->FindPin(TEXT("Respawn")), Complete->FindPin(UEdGraphSchema_K2::PN_Self));
  Wire(Event->FindPin(TEXT("RunId")), Complete->FindPin(TEXT("RunId")));
  Wire(Self->FindPin(UEdGraphSchema_K2::PN_Self), Complete->FindPin(TEXT("Character")));
  TestNotNull(TEXT("Damage source exposed on override"),Event->FindPin(TEXT("DamageSource")));
  TestNotNull(TEXT("Damage type exposed on override"),Event->FindPin(TEXT("DamageType")));
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Results;
 FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
 TestEqual(TEXT("Both presentation override and completion graphs compile"), Results.NumErrors, 0);
 return BP->Status != BS_Error;
}
#endif
