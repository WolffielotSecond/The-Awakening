#include "TAInputBlueprintMigrationCommandlet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Puzzle/TAPathPuzzleWidget.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

UTAInputBlueprintMigrationCommandlet::UTAInputBlueprintMigrationCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UTAInputBlueprintMigrationCommandlet::Main(const FString& Params)
{
	auto* Blueprint = LoadObject<UBlueprint>(nullptr,
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter"));
	if (!Blueprint) return 1;
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UK2Node_CallFunction* Target = nullptr;
	for (auto* Graph : Graphs)
		for (UEdGraphNode* Node : Graph->Nodes)
			if (Graph->GetFName() == TEXT("Debug Graph") && Node->GetFName() == TEXT("K2Node_CallFunction_0"))
				Target = Cast<UK2Node_CallFunction>(Node);
	if (!Target || !Target->GetTargetFunction()) return 2;
	const FName OldName = GET_FUNCTION_NAME_CHECKED(UTAPathPuzzleWidget, OpenPuzzleWithSettings);
	const FName NewName = GET_FUNCTION_NAME_CHECKED(UTAPathPuzzleWidget, OpenPuzzleWithSettingsFromPlayerInput);
	if (Target->GetTargetFunction()->GetFName() == NewName) return 0;
	if (Target->GetTargetFunction()->GetFName() != OldName) return 3;
	// Identical signature: retain the user's WBP, settings wiring, rewards and seed pins.
	TMap<FName, FString> Defaults;
	TMap<FName, int32> Links;
	for (auto* Pin : Target->Pins) { Defaults.Add(Pin->PinName, Pin->DefaultValue); Links.Add(Pin->PinName, Pin->LinkedTo.Num()); }
	Target->SetFromFunction(UTAPathPuzzleWidget::StaticClass()->FindFunctionByName(NewName));
	Target->ReconstructNode();
	for (auto* Pin : Target->Pins)
		if (!Defaults.Contains(Pin->PinName) || Defaults[Pin->PinName] != Pin->DefaultValue || Links[Pin->PinName] != Pin->LinkedTo.Num()) return 4;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error) return 5;
	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args)) return 6;
	UE_LOG(LogTemp, Display, TEXT("Migrated B-key debug call; all default values and links retained."));
	return 0;
}
