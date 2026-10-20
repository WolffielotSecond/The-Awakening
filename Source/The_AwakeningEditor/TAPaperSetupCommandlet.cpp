#include "TAPaperSetupCommandlet.h"
#include "Document/TAPaperAnimInstance.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentPresentation.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
	bool SavePaperAsset(UObject* Asset)
	{
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Asset->GetOutermost(), Asset,
			*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}
	double PoseError(UAnimSequence* Transition, int32 Frame, UAnimSequence* Pose)
	{
		const IAnimationDataModel* A = Transition->GetDataModel();
		const IAnimationDataModel* B = Pose->GetDataModel();
		TArray<FName> Tracks, OtherTracks;
		A->GetBoneTrackNames(Tracks); B->GetBoneTrackNames(OtherTracks);
		double Error = 0.;
		for (FName Name : Tracks)
		{
			if (!OtherTracks.Contains(Name)) continue;
			const FTransform X = A->GetBoneTrackTransform(Name, FFrameNumber(Frame));
			const FTransform Y = B->GetBoneTrackTransform(Name, FFrameNumber(0));
			Error += X.GetRotation().AngularDistance(Y.GetRotation()) + FVector::Distance(X.GetLocation(), Y.GetLocation());
		}
		return Error;
	}
}

UTAPaperSetupCommandlet::UTAPaperSetupCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UTAPaperSetupCommandlet::Main(const FString& Params)
{
	UAnimSequence* Fold = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Models/Document/AS_Fold.AS_Fold"));
	UAnimSequence* Folded = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Models/Document/AS_Folded.AS_Folded"));
	UAnimSequence* Opened = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Models/Document/AS_Opened.AS_Opened"));
	UTADocumentDefinition* Doc = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_67.DA_Document_67"));
	if (!Fold || !Folded || !Opened || !Doc || !Fold->GetSkeleton()
		|| Fold->GetSkeleton() != Folded->GetSkeleton() || Fold->GetSkeleton() != Opened->GetSkeleton()) return 1;
	const bool bVerify = Params.Contains(TEXT("Verify"));
	UAnimBlueprint* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Models/Document/ABP_Paper.ABP_Paper"));
	if (!BP && !bVerify)
	{
		BP = Cast<UAnimBlueprint>(FKismetEditorUtilities::CreateBlueprint(UTAPaperAnimInstance::StaticClass(),
			CreatePackage(TEXT("/Game/Models/Document/ABP_Paper")), TEXT("ABP_Paper"), BPTYPE_Normal,
			UAnimBlueprint::StaticClass(), UAnimBlueprintGeneratedClass::StaticClass()));
		if (!BP) return 2;
		BP->TargetSkeleton = Fold->GetSkeleton();
		UEdGraph* Graph = nullptr;
		for (UEdGraph* G : BP->FunctionGraphs) if (G->GetFName() == TEXT("AnimGraph")) Graph = G;
		if (!Graph)
		{
			Graph = FBlueprintEditorUtils::CreateNewGraph(BP, TEXT("AnimGraph"), UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
			FBlueprintEditorUtils::AddFunctionGraph(BP, Graph, false, static_cast<UClass*>(nullptr));
			Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
		}
		UAnimGraphNode_Root* Root = nullptr;
		for (UEdGraphNode* N : Graph->Nodes) if (auto* R = Cast<UAnimGraphNode_Root>(N)) Root = R;
		if (!Root) return 3;
		auto* Evaluate = NewObject<UAnimGraphNode_SequenceEvaluator>(Graph);
		Graph->AddNode(Evaluate, false, false);
		Evaluate->CreateNewGuid(); Evaluate->PostPlacedNewNode();
		Evaluate->Node.SetSequence(Fold); Evaluate->Node.SetShouldLoop(false);
		Evaluate->AllocateDefaultPins();
		for (FOptionalPinFromProperty& Pin : Evaluate->ShowPinForProperties)
			if (Pin.PropertyName == TEXT("Sequence")) Pin.bShowPin = true;
		Evaluate->ReconstructNode(); Evaluate->NodePosX = -250;
		const UEdGraphSchema* Schema = Graph->GetSchema();
		auto Connect = [&](const TCHAR* Property, const TCHAR* PinName, int32 Y)
		{
			auto* Get = NewObject<UK2Node_VariableGet>(Graph);
			Graph->AddNode(Get, false, false); Get->VariableReference.SetSelfMember(Property);
			Get->CreateNewGuid(); Get->PostPlacedNewNode(); Get->AllocateDefaultPins();
			Get->NodePosX = -550; Get->NodePosY = Y;
			return Schema->TryCreateConnection(Get->GetValuePin(), Evaluate->FindPin(PinName));
		};
		if (!Connect(TEXT("SelectedAnimation"), TEXT("Sequence"), 0)
			|| !Connect(TEXT("SampleTime"), TEXT("ExplicitTime"), 150)
			|| !Schema->TryCreateConnection(Evaluate->FindPin(TEXT("Pose")), Root->FindPin(TEXT("Result")))) return 4;
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FAssetRegistryModule::AssetCreated(BP);
	}
	if (!BP) return 5;
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
	if (Results.NumErrors || !BP->GeneratedClass) return 6;
	auto* Defaults = Cast<UTAPaperAnimInstance>(BP->GeneratedClass->GetDefaultObject());
	if (!Defaults) return 7;
	const int32 LastFrame = Fold->GetDataModel()->GetNumberOfFrames();
	const double ForwardError = PoseError(Fold, 0, Opened) + PoseError(Fold, LastFrame, Folded);
	const double ReverseError = PoseError(Fold, 0, Folded) + PoseError(Fold, LastFrame, Opened);
	UE_LOG(LogTemp, Display, TEXT("Paper: duration=%.3f frames=%d forward error=%.6f reverse error=%.6f"), Fold->GetPlayLength(), LastFrame, ForwardError, ReverseError);
	if (!bVerify)
	{
		Defaults->FoldAnimation = Fold; Defaults->FoldedAnimation = Folded; Defaults->OpenedAnimation = Opened;
		Defaults->bFoldStartsOpened = ForwardError <= ReverseError;
		if (!SavePaperAsset(BP)) return 8;
	}
	auto* Profile = LoadObject<UTADocumentPresentation>(nullptr, TEXT("/Game/Models/Document/DA_PaperPresentation.DA_PaperPresentation"));
	if (!Profile && !bVerify)
	{
		Profile = NewObject<UTADocumentPresentation>(CreatePackage(TEXT("/Game/Models/Document/DA_PaperPresentation")), TEXT("DA_PaperPresentation"), RF_Public | RF_Standalone);
		Profile->PreviewRotation = FRotator(0.f, 90.f, 90.f); // Editor XYZ = 90, 0, 90.
		Profile->AnimationClass = BP->GeneratedClass;
		FAssetRegistryModule::AssetCreated(Profile);
		if (!SavePaperAsset(Profile)) return 9;
	}
	if (!Profile) return 10;
	if (!Doc->Presentation && !bVerify)
	{
		const FTransform Inverse = FTransform(Profile->PreviewRotation).Inverse();
		Doc->PickupPreviewTransform = Inverse * Doc->PickupPreviewTransform;
		Doc->ReaderTransform = Inverse * Doc->ReaderTransform;
		Doc->DiaryPageTransform = Inverse * Doc->DiaryPageTransform;
		Doc->Presentation = Profile;
		if (!SavePaperAsset(Doc)) return 11;
	}
	if (Doc->Presentation != Profile || !Defaults->FoldAnimation || !Profile->AnimationClass) return 12;
	UE_LOG(LogTemp, Display, TEXT("PAPER_SETUP_VERIFIED: ABP, shared axis correction, three animations and document binding."));
	return 0;
}
