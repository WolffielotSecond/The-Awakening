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
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
	bool SavePaperAsset(UObject* Asset)
	{
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Asset->GetOutermost(), Asset,
			*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}
	bool MatchesPaper(const UTADocumentDefinition* Document, const UTADocumentDefinition* Reference)
	{
		return Document->Mesh == Reference->Mesh && Document->Presentation == Reference->Presentation
			&& Document->WorldTransform.Equals(Reference->WorldTransform)
			&& Document->PickupPreviewTransform.Equals(Reference->PickupPreviewTransform)
			&& Document->ReaderTransform.Equals(Reference->ReaderTransform)
			&& Document->DiaryPageTransform.Equals(Reference->DiaryPageTransform);
	}

	bool IsDefaultDocument(const UTADocumentDefinition* Document)
	{
		// These are the existing default examples, including assets already migrated by this command.
		if (Document->GetPathName() == TEXT("/Game/Document/DA_Document_Sample01.DA_Document_Sample01")
			|| Document->GetPathName() == TEXT("/Game/Document/DA_Document_Sample02.DA_Document_Sample02")) return true;
		if (!Document->Mesh) return true;
		if (auto* Mesh = Cast<UStaticMesh>(Document->Mesh))
		{
			if (Mesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/")) || Mesh->GetName().Contains(TEXT("棍母"))) return true;
			for (const FStaticMaterial& Material : Mesh->GetStaticMaterials())
				if (Material.MaterialInterface && Material.MaterialInterface->GetName().Contains(TEXT("M_MissingMaterialFallback"))) return true;
		}
		return false;
	}

	int32 UnifyDefaultDocuments(bool bVerify)
	{
		auto* Reference = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_67.DA_Document_67"));
		auto* Mesh = Reference ? Cast<USkeletalMesh>(Reference->Mesh) : nullptr;
		if (!Mesh || !Reference->Presentation || !Reference->Presentation->AnimationClass) return 20;
		FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		Registry.Get().SearchAllAssets(true);
		FARFilter Filter;
		Filter.ClassPaths.Add(UTADocumentDefinition::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;
		TArray<FAssetData> Assets;
		Registry.Get().GetAssets(Filter, Assets);
		const FString BackupDirectory = FPaths::ProjectSavedDir() / TEXT("DocumentPresentationBackups") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		int32 Targets = 0, Changed = 0;
		for (const FAssetData& Asset : Assets)
		{
			auto* Document = Cast<UTADocumentDefinition>(Asset.GetAsset());
			if (!Document) return 21;
			UE_LOG(LogTemp, Display, TEXT("Document style audit: %s id=%s mesh=%s presentation=%s"),
				*Document->GetPathName(), *Document->DocumentId.ToString(), *GetPathNameSafe(Document->Mesh), *GetPathNameSafe(Document->Presentation));
			if (Document == Reference || !IsDefaultDocument(Document)) continue;
			++Targets;
			if (MatchesPaper(Document, Reference)) continue;
			if (bVerify)
			{
				UE_LOG(LogTemp, Error, TEXT("Default document does not match reference paper: %s"), *Document->GetPathName());
				return 22;
			}
			const FString PackageName = Document->GetOutermost()->GetName();
			const FString SourceFile = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			const FString BackupFile = BackupDirectory / PackageName.RightChop(1) + FPackageName::GetAssetPackageExtension();
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(BackupFile), true);
			if (IFileManager::Get().Copy(*BackupFile, *SourceFile, false) != COPY_OK) return 23;
			// Share the exact mesh (including materials/textures), animation profile, and all display contexts.
			// Identity, localization, icon, scan text and inline illustrations remain on the original document.
			Document->Mesh = Reference->Mesh;
			Document->Presentation = Reference->Presentation;
			Document->WorldTransform = Reference->WorldTransform;
			Document->PickupPreviewTransform = Reference->PickupPreviewTransform;
			Document->ReaderTransform = Reference->ReaderTransform;
			Document->DiaryPageTransform = Reference->DiaryPageTransform;
			if (!SavePaperAsset(Document)) return 24;
			++Changed;
		}
		UE_LOG(LogTemp, Display, TEXT("DEFAULT_PAPER_VERIFIED: scanned=%d targets=%d changed=%d backup=%s"),
			Assets.Num(), Targets, Changed, *BackupDirectory);
		return Targets > 0 ? 0 : 25;
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
	if (FParse::Param(*Params, TEXT("UnifyDefaults")))
		return UnifyDefaultDocuments(FParse::Param(*Params, TEXT("Verify")));
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
