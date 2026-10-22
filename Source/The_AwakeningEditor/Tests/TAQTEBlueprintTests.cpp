#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Debug/TADebugQTETester.h"
#include "Interaction/TAQTESequenceComponent.h"
#include "InputAction.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MakeArray.h"
#include "K2Node_Self.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAQTEBlueprintFunctionTest, "TheAwakening.Interaction.QTE.BlueprintFunctions",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAQTEBlueprintFunctionTest::RunTest(const FString&)
{
 auto* BP = FKismetEditorUtilities::CreateBlueprint(ATADebugQTETester::StaticClass(), GetTransientPackage(),
  MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("QTEFunctionTest")), BPTYPE_Normal,
  UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
 if (!TestNotNull(TEXT("Temporary Blueprint created"), BP)) return false;
 auto* Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("QTEExample"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
 FBlueprintEditorUtils::AddUbergraphPage(BP, Graph);
 auto AddNode = [&]<typename T>()
 {
  auto* Node = NewObject<T>(Graph); Graph->AddNode(Node); Node->CreateNewGuid(); return Node;
 };
 auto* Start = AddNode.operator()<UK2Node_CustomEvent>();
 Start->CustomFunctionName = TEXT("RunQTEExample"); Start->AllocateDefaultPins();
 auto* Callback = AddNode.operator()<UK2Node_CustomEvent>();
 Callback->CustomFunctionName = TEXT("HandleQTEStep"); Callback->AllocateDefaultPins();
 FEdGraphPinType Type; Type.PinCategory = UEdGraphSchema_K2::PC_Int;
 Callback->CreateUserDefinedPin(TEXT("StepIndex"), Type, EGPD_Output);
 Type.PinCategory = UEdGraphSchema_K2::PC_Object; Type.PinSubCategoryObject = UInputAction::StaticClass();
 Callback->CreateUserDefinedPin(TEXT("Action"), Type, EGPD_Output);
 Type.PinCategory = UEdGraphSchema_K2::PC_Real; Type.PinSubCategory = UEdGraphSchema_K2::PC_Float; Type.PinSubCategoryObject = nullptr;
 Callback->CreateUserDefinedPin(TEXT("Progress"), Type, EGPD_Output);
 auto* EndCallback = AddNode.operator()<UK2Node_CustomEvent>();
 EndCallback->CustomFunctionName = TEXT("HandleQTEFinished"); EndCallback->AllocateDefaultPins();
 Type.PinCategory = UEdGraphSchema_K2::PC_Boolean; Type.PinSubCategory = NAME_None;
 EndCallback->CreateUserDefinedPin(TEXT("bSucceeded"), Type, EGPD_Output);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 auto Call = [&](FName Name)
 {
  auto* Node = AddNode.operator()<UK2Node_CallFunction>();
  Node->SetFromFunction(UTAQTESequenceComponent::StaticClass()->FindFunctionByName(Name));
  Node->AllocateDefaultPins(); return Node;
 };
 auto* Make = Call(GET_FUNCTION_NAME_CHECKED(UTAQTESequenceComponent, MakeQTEStep));
 auto* Generate = Call(GET_FUNCTION_NAME_CHECKED(UTAQTESequenceComponent, GenerateQTE));
 auto* Array = AddNode.operator()<UK2Node_MakeArray>(); Array->AllocateDefaultPins();
 auto* Self = AddNode.operator()<UK2Node_Self>(); Self->AllocateDefaultPins();
 auto* Component = AddNode.operator()<UK2Node_VariableGet>();
 Component->VariableReference.SetSelfMember(TEXT("QTESequence")); Component->AllocateDefaultPins();
 const auto* Schema = GetDefault<UEdGraphSchema_K2>();
 auto Wire = [&](UEdGraphPin* From, UEdGraphPin* To)
 { return TestTrue(TEXT("Blueprint QTE pins connect"), From && To && Schema->TryCreateConnection(From, To)); };
 Wire(Start->FindPin(UEdGraphSchema_K2::PN_Then), Generate->GetExecPin());
 Wire(Component->GetValuePin(), Generate->FindPin(UEdGraphSchema_K2::PN_Self));
 Wire(Self->FindPin(UEdGraphSchema_K2::PN_Self), Generate->FindPin(TEXT("Interactor")));
 Wire(Make->GetReturnValuePin(), Array->FindPin(TEXT("[0]")));
 Wire(Array->GetOutputPin(), Generate->FindPin(TEXT("InSteps")));
 auto Bind = [&](const TCHAR* Name, UK2Node_CallFunction* Node, FName Pin)
 {
  auto* Delegate = AddNode.operator()<UK2Node_CreateDelegate>(); Delegate->AllocateDefaultPins();
  Wire(Delegate->GetDelegateOutPin(), Node->FindPin(Pin)); Delegate->SetFunction(Name);
 };
 Bind(TEXT("HandleQTEStep"), Make, TEXT("OnAcceptedInput"));
 Bind(TEXT("HandleQTEStep"), Make, TEXT("OnCompleted"));
 Bind(TEXT("HandleQTEFinished"), Generate, TEXT("OnFinished"));
 FCompilerResultsLog Results;
 FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
 TestEqual(TEXT("Make Step / array / Generate / Create Event graph compiles"), Results.NumErrors, 0);
 TestTrue(TEXT("Blueprint function graph is usable"), BP->Status != BS_Error);
 return true;
}
#endif
