#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentPresentation.h"
#include "Document/TAPaperAnimInstance.h"
#include "Document/TAPreviewStage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Animation/AnimSequence.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/TATimeSubsystem.h"
#include "UI/Document/TADocumentReaderWidget.h"
#include "Components/Image.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPaperPresentationTest, "TheAwakening.Document.Paper.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPaperPresentationTest::RunTest(const FString&)
{
	auto* Doc = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_67.DA_Document_67"));
	if (!TestNotNull(TEXT("Document"), Doc) || !TestNotNull(TEXT("Shared presentation"), Doc->Presentation.Get())) return false;
	TestTrue(TEXT("Shared correction is editor XYZ 90,0,90"), Doc->Presentation->PreviewRotation.Equals(FRotator(0.f, 90.f, 90.f), 0.001f));
	TestTrue(TEXT("Reader no longer duplicates model-axis correction"), Doc->ReaderTransform.GetRotation().Equals(FQuat::Identity, 0.001f));
	TestTrue(TEXT("Diary correction applied exactly once"), Doc->GetPreviewTransform(Doc->DiaryPageTransform).GetRotation().Equals(FQuat(FRotator(0.f, 90.f, 90.f)), 0.001f));
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ATAPreviewStage* Stage = ATAPreviewStage::SpawnPreviewStage(World, nullptr, 7);
	Stage->SetAutoRotate(false);
	Stage->SetDocument(Doc, Doc->DiaryPageTransform, false);
	auto* Mesh = Stage->GetSkeletalMeshComponent();
	auto* Paper = Cast<UTAPaperAnimInstance>(Mesh->GetAnimInstance());
	if (TestNotNull(TEXT("ABP instance attached"), Paper))
	{
		TestTrue(TEXT("All three animation resources configured"), Paper->FoldAnimation && Paper->FoldedAnimation && Paper->OpenedAnimation);
		TestTrue(TEXT("Fold animation has duration"), Paper->GetFoldDuration() > 0.f);
		TestTrue(TEXT("Diary starts folded"), Paper->SelectedAnimation == Paper->FoldedAnimation);
		Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms();
		const TArray<FTransform> FoldedBones = Mesh->GetComponentSpaceTransforms();
		const FVector Camera = Stage->GetCaptureComponent()->GetRelativeLocation();
		Stage->SetPaperOpenness(0.25f);
		TestTrue(TEXT("Transition samples fold animation"), Paper->SelectedAnimation == Paper->FoldAnimation);
		TestTrue(TEXT("Fold time follows direction"), FMath::IsNearlyEqual(Paper->SampleTime,
			(Paper->bFoldStartsOpened ? 0.75f : 0.25f) * Paper->GetFoldDuration()));
		Stage->SetPaperOpenness(1.f);
		Mesh->TickAnimation(0.f, false); Mesh->RefreshBoneTransforms();
		TestTrue(TEXT("Opened endpoint uses opened pose"), Paper->SelectedAnimation == Paper->OpenedAnimation);
		const TArray<FTransform>& OpenBones = Mesh->GetComponentSpaceTransforms();
		bool bDifferent = false;
		for (int32 I = 0; I < FMath::Min(OpenBones.Num(), FoldedBones.Num()); ++I)
			bDifferent |= !OpenBones[I].Equals(FoldedBones[I], 0.001f);
		TestTrue(TEXT("ABP actually changes evaluated bone poses"), bDifferent);
		TestTrue(TEXT("Camera framing stable across fold"), Camera.Equals(Stage->GetCaptureComponent()->GetRelativeLocation()));
		Stage->SetPaperOpenness(0.5f);
		Stage->SetPaperOpenness(0.f);
		TestTrue(TEXT("Return ends folded"), Paper->SelectedAnimation == Paper->FoldedAnimation);
		Stage->AddManualRotation(135.f, 80.f);
		Stage->SetManualRotation(FQuat::Identity);
		TestTrue(TEXT("Manual reading rotation resets for return"), Stage->GetManualRotation().Equals(FQuat::Identity));
		Stage->SetDisplayMesh(Stage->PlaceholderMesh, FTransform::Identity);
		TestNull(TEXT("Switching to non-paper clears ABP"), Mesh->GetAnimInstance());
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPaperReaderTransitionTest, "TheAwakening.Document.Paper.ReaderTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPaperReaderTransitionTest::RunTest(const FString&)
{
	auto* Doc = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_67.DA_Document_67"));
	if (!Doc || !Doc->Presentation) return false;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	auto* Stage = ATAPreviewStage::SpawnPreviewStage(World, nullptr, 8);
	Stage->SetDocument(Doc, Doc->DiaryPageTransform, false);
	auto* Paper = Cast<UTAPaperAnimInstance>(Stage->GetSkeletalMeshComponent()->GetAnimInstance());
	if (!TestNotNull(TEXT("Paper ABP"), Paper))
	{
		World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
	}
	auto* Source = NewObject<UImage>();
	auto* Reader = NewObject<UTADocumentReaderWidget>(World);
	Reader->Initialize();
	Reader->Document = Doc; Reader->PreviewStage = Stage;
	Reader->bBorrowedStage = true; Reader->bPaperTransition = true;
	Reader->Image_Model = NewObject<UImage>(Reader);
	Reader->SourceDiaryImage = Source;
	Reader->DiaryModelTransform = Stage->GetDiaryModelTransform(Doc);
	Stage->SetModelPresentationTransform(Reader->DiaryModelTransform);
	const FQuat ScreenTurn = Reader->DiaryModelTransform.GetRotation()
		* Doc->GetPreviewTransform(Doc->DiaryPageTransform).GetRotation().Inverse();
	TestTrue(TEXT("Folded paper turns screen-right towards screen-up (counterclockwise 90 degrees)"),
		ScreenTurn.RotateVector(FVector::YAxisVector).Equals(FRotator(Stage->CameraPitch, 0.f, 0.f).RotateVector(FVector::ZAxisVector), 0.001f));
	Reader->ReaderModelTransform = Doc->GetPreviewTransform(Doc->ReaderTransform);
	Reader->ReaderModelTransform.SetRotation(FQuat(FRotator(Stage->CameraPitch, 0.f, 0.f)) * Reader->ReaderModelTransform.GetRotation());
	// Supply a laid-out, DPI-scaled destination without requiring an editor viewport.
	Reader->bPaperLayoutReady = true;
	Reader->ReaderTargetGeometry = FGeometry::MakeRoot(FVector2D(800.f), FSlateLayoutTransform(1.5f, FVector2D(100.f)));
	Reader->DiaryAbsoluteCenter = FVector2D(1000.f, 600.f);
	Reader->DiaryAbsoluteSize = FVector2D(300.f);
	Source->SetRenderOpacity(0.f);
	int32 Closed = 0;
	bool bForced = false;
	Reader->OnClosed = [&](bool Forced) { ++Closed; bForced = Forced; };
	Reader->TickPaperTransition(Doc->Presentation->TravelDuration * 0.5f);
	TestEqual(TEXT("Take-out keeps folded pose"), Paper->Openness, 0.f);
	TestFalse(TEXT("Still-folded paper already rotates towards camera"),
		Stage->GetSkeletalMeshComponent()->GetRelativeTransform().GetRotation().Equals(Reader->DiaryModelTransform.GetRotation(), 0.001f));
	TestTrue(TEXT("Take-out is between book and full size"), Reader->Image_Model->GetRenderTransform().Scale.X > 0.25f
		&& Reader->Image_Model->GetRenderTransform().Scale.X < 1.f);
	Reader->TickPaperTransition(Doc->Presentation->TravelDuration * 0.5f + Paper->GetFoldDuration() * 0.5f);
	TestTrue(TEXT("Unfolding is synchronized with UI time"), FMath::IsNearlyEqual(Paper->Openness, 0.5f));
	TestTrue(TEXT("Paper faces camera before unfolding"), Stage->GetSkeletalMeshComponent()->GetRelativeTransform().GetRotation()
		.Equals(Reader->ReaderModelTransform.GetRotation(), 0.001f));
	Reader->TickPaperTransition(Paper->GetFoldDuration() + Doc->Presentation->OrientDuration);
	TestTrue(TEXT("Reader becomes interactive only after unfold and orient"), Reader->Phase == UTADocumentReaderWidget::EPhase::Open);
	TestEqual(TEXT("Fully open"), Paper->Openness, 1.f);
	Stage->AddManualRotation(110.f, 75.f);
	Reader->RequestClose();
	Reader->ReaderTargetGeometry = FGeometry::MakeRoot(FVector2D(800.f), FSlateLayoutTransform(1.5f, FVector2D(100.f)));
	Reader->TickPaperTransition(Doc->Presentation->OrientDuration * 0.5f);
	TestEqual(TEXT("Return first reorients without folding"), Paper->Openness, 1.f);
	TestFalse(TEXT("Return rotation is smoothly restored"), Stage->GetManualRotation().Equals(FQuat::Identity));
	Reader->TickPaperTransition(Doc->Presentation->OrientDuration * 0.5f + Paper->GetFoldDuration() * 0.5f);
	TestTrue(TEXT("Returning sheet folds"), FMath::IsNearlyEqual(Paper->Openness, 0.5f));
	Reader->TickPaperTransition(Paper->GetFoldDuration() + Doc->Presentation->TravelDuration);
	TestEqual(TEXT("Normal close callback exactly once"), Closed, 1);
	TestFalse(TEXT("Normal return is not forced"), bForced);
	TestEqual(TEXT("Book sheet becomes visible again"), Source->GetRenderOpacity(), 1.f);
	TestTrue(TEXT("Borrowed stage survives"), IsValid(Stage));
	TestEqual(TEXT("Book sheet restored folded"), Paper->Openness, 0.f);
	TestTrue(TEXT("Book rotation restored"), Stage->GetManualRotation().Equals(FQuat::Identity));
	// Forced close midway through unfolding must restore the same shared stage and source.
	Reader->bDiaryPresentationRestored = false; Reader->Phase = UTADocumentReaderWidget::EPhase::Opening;
	Reader->PhaseTime = 0.f; Reader->bClosedNormally = false;
	Reader->OnClosed = [&](bool Forced) { ++Closed; bForced = Forced; };
	Source->SetRenderOpacity(0.f);
	Reader->TickPaperTransition(Doc->Presentation->TravelDuration + Paper->GetFoldDuration() * 0.5f);
	Reader->ForceClose(); Reader->ForceClose();
	TestEqual(TEXT("Forced close callback exactly once"), Closed, 2);
	TestTrue(TEXT("Forced close reports interruption"), bForced);
	TestEqual(TEXT("Forced close restores folded paper"), Paper->Openness, 0.f);
	TestEqual(TEXT("Forced close restores source visibility"), Source->GetRenderOpacity(), 1.f);
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
