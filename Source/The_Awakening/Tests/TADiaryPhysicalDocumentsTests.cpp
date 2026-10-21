#include "Document/TADiaryPreviewStage.h"
#include "Document/TADiaryDocumentComponent.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentPresentation.h"
#include "Document/TAPaperAnimInstance.h"
#include "UI/Document/TADocumentReaderWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
namespace
{
	struct FDiaryWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ATADiaryPreviewStage* Stage;
		FDiaryWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Stage = ATADiaryPreviewStage::SpawnDiaryStage(World);
			if (Stage) Stage->SetOpenImmediate(true);
		}
		~FDiaryWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};
	UTADocumentDefinition* Paper()
	{
		return LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_67.DA_Document_67"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADiaryPhysicalTurnTest, "TheAwakening.Document.Diary.PhysicalTurn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADiaryPhysicalTurnTest::RunTest(const FString&)
{
	FDiaryWorld F;
	UTADocumentDefinition* A = Paper();
	if (!TestNotNull(TEXT("Book stage"), F.Stage) || !TestNotNull(TEXT("Paper definition"), A)) return false;
	auto* B = DuplicateObject<UTADocumentDefinition>(A, GetTransientPackage());
	auto* C = DuplicateObject<UTADocumentDefinition>(A, GetTransientPackage());
	F.Stage->SetPages(C, A);
	auto* Moving = F.Stage->GetMovingDocument();
	auto* Right = F.Stage->GetRightDocument();
	auto* Left = F.Stage->GetLeftDocument();
	const FTransform Camera = F.Stage->GetCaptureComponent()->GetComponentTransform();
	const FTransform RightStart = Right->GetComponentTransform();
	TestTrue(TEXT("Paper hit bounds cover the visible sheet rather than tiny physics bodies"), Right->GetWorldBox().GetSize().GetMax() > 10.f);
	const FBox2D Screen = F.Stage->GetRightDocumentScreenBounds();
	TestTrue(TEXT("Document projects to clickable right side of capture"), Screen.bIsValid && Screen.GetCenter().X > 0.5f
		&& Screen.Min.X >= 0.f && Screen.Max.X <= 1.f && Screen.Min.Y >= 0.f && Screen.Max.Y <= 1.f);
	TestTrue(TEXT("Forward accepted"), F.Stage->BeginPageTurn(true, A, B));
	TestTrue(TEXT("Target right document exists before any animation time"), Right->GetDefinition() == B && Right->IsVisible());
	const FVector BookUp = F.Stage->GetSkeletalMeshComponent()->GetComponentTransform().GetUnitAxis(EAxis::Z);
	TestTrue(TEXT("Incoming document starts underneath the turning page"), FVector::DotProduct(Right->GetComponentLocation()
		- F.Stage->GetSkeletalMeshComponent()->GetSocketLocation(TEXT("Document_Attach")), BookUp) < 0.f);
	TestTrue(TEXT("Outgoing document on moving sheet"), Moving->GetDefinition() == A && Moving->IsVisible());
	TestTrue(TEXT("No takeoff jump"), Moving->GetComponentTransform().Equals(RightStart, 0.001f));
	const FTransform Before = Moving->GetComponentTransform();
	F.Stage->Tick(0.5f);
	TestFalse(TEXT("Document moves with page bone"), Moving->GetComponentTransform().Equals(Before, 0.001f));
	TestFalse(TEXT("Cannot read during page motion"), F.Stage->BeginReading());
	TestFalse(TEXT("Cannot queue another turn during motion"), F.Stage->BeginPageTurn(false, C, A));
	F.Stage->Tick(0.2f);
	TestFalse(TEXT("Document is occluded from the back before the forward turn finishes"), Moving->IsVisible());
	F.Stage->Tick(0.299f);
	const FTransform NearEnd = Moving->GetComponentTransform();
	F.Stage->Tick(0.002f);
	TestTrue(TEXT("Previous document retained for backward turn but hidden at rest"), Left->GetDefinition() == A && !Left->IsVisible());
	TestTrue(TEXT("No landing jump"), Left->GetComponentTransform().Equals(NearEnd, 0.02f));
	TestTrue(TEXT("Prepared right document survives completion"), Right->GetDefinition() == B && Right->IsVisible());
	TestFalse(TEXT("Moving copy hidden after landing"), Moving->IsVisible());
	const FTransform LeftStart = Left->GetComponentTransform();
	TestTrue(TEXT("Backward accepted"), F.Stage->BeginPageTurn(false, C, A));
	TestTrue(TEXT("Backward starts from left document"), Moving->GetDefinition() == A && Moving->GetComponentTransform().Equals(LeftStart, 0.001f));
	TestFalse(TEXT("Backward starts with document hidden behind the turning page"), Moving->IsVisible());
	TestTrue(TEXT("Previous left data prepared without displaying a stationary document"), Left->GetDefinition() == C && !Left->IsVisible());
	F.Stage->Tick(0.7f);
	TestTrue(TEXT("Backward document becomes visible as its face turns toward camera"), Moving->IsVisible());
	F.Stage->Tick(0.4f);
	TestTrue(TEXT("Backward restores current right"), Right->GetDefinition() == A);
	TestTrue(TEXT("Backward returns to original right transform"), Right->GetComponentTransform().Equals(RightStart, 0.001f));
	TestTrue(TEXT("Camera stable across both turns"), Camera.Equals(F.Stage->GetCaptureComponent()->GetComponentTransform()));
	F.Stage->BeginPageTurn(true, A, B);
	F.Stage->Tick(0.4f);
	F.Stage->SetOpenImmediate(true);
	TestTrue(TEXT("Forced interruption restores original documents"), Right->GetDefinition() == A && Left->GetDefinition() == C);
	TestFalse(TEXT("Interrupted moving copy hidden"), Moving->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADiarySharedReaderTest, "TheAwakening.Document.Diary.SharedReader",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTADiarySharedReaderTest::RunTest(const FString&)
{
	for (const TCHAR* Path : {TEXT("/Game/Document/DA_Document_67.DA_Document_67"),
		TEXT("/Game/Document/DA_Document_Sample01.DA_Document_Sample01"),
		TEXT("/Game/Document/DA_Document_Sample02.DA_Document_Sample02")})
	{
		AddInfo(FString::Printf(TEXT("Shared diary reader: %s"), Path));
		FDiaryWorld F;
		UTADocumentDefinition* Document = LoadObject<UTADocumentDefinition>(nullptr, Path);
		if (!TestNotNull(TEXT("Book stage"), F.Stage) || !TestNotNull(TEXT("Paper definition"), Document)) return false;
		F.Stage->SetPages(Document, Document);
		const FTransform Book = F.Stage->GetSkeletalMeshComponent()->GetComponentTransform();
		const FTransform Left = F.Stage->GetLeftDocument()->GetComponentTransform();
		const FTransform Seated = F.Stage->GetRightDocument()->GetComponentTransform();
		const FTransform Camera = F.Stage->GetCaptureComponent()->GetComponentTransform();
		auto* Reader = NewObject<UTADocumentReaderWidget>(F.World);
		Reader->Initialize();
		Reader->Document = Document;
		Reader->PreviewStage = F.Stage;
		Reader->bBorrowedStage = true;
		Reader->Image_Model = NewObject<UImage>(Reader);
		Reader->SetupModel();
		TestTrue(TEXT("Existing reader borrows book scene without replacing its mesh"), F.Stage->IsReading() && Reader->bPaperTransition);
		TestTrue(TEXT("Read starts at exact seated pose"), Seated.Equals(F.Stage->GetRightDocument()->GetComponentTransform()));
		Reader->bPaperLayoutReady = true;
		Reader->ReaderTargetGeometry = FGeometry::MakeRoot(FVector2D(900.f, 600.f), FSlateLayoutTransform(1.f));
		Reader->DiaryAbsoluteCenter = FVector2D(800.f, 500.f);
		Reader->DiaryAbsoluteSize = FVector2D(1200.f, 800.f);
		Reader->TickPaperTransition(0.175f);
		TestFalse(TEXT("Document physically leaves book"), Seated.Equals(F.Stage->GetRightDocument()->GetComponentTransform()));
		auto* Anim = Cast<UTAPaperAnimInstance>(F.Stage->GetRightDocument()->GetSkeletalMeshComponent()->GetAnimInstance());
		if (!TestNotNull(TEXT("Existing folding animation preserved"), Anim)) return false;
		TestEqual(TEXT("Document stays folded during takeout"), Anim->Openness, 0.f);
		Reader->TickPaperTransition(10.f);
		TestEqual(TEXT("Document unfolds before interaction"), Anim->Openness, 1.f);
		F.Stage->AddManualRotation(70.f, 35.f);
		TestFalse(TEXT("Mouse rotation affects current document"), F.Stage->GetManualRotation().Equals(FQuat::Identity));
		TestTrue(TEXT("Book does not rotate with document"), Book.Equals(F.Stage->GetSkeletalMeshComponent()->GetComponentTransform()));
		TestTrue(TEXT("Left document remains in book"), Left.Equals(F.Stage->GetLeftDocument()->GetComponentTransform()));
		TestTrue(TEXT("Read camera is the book camera"), Camera.Equals(F.Stage->GetCaptureComponent()->GetComponentTransform()));
		TestFalse(TEXT("Cannot close book during reading"), F.Stage->CloseBook());
		int32 Closed = 0;
		Reader->OnClosed = [&](bool) { ++Closed; };
		Reader->RequestClose();
		Reader->TickPaperTransition(Document->Presentation->OrientDuration * 0.5f);
		TestEqual(TEXT("Returning first removes manual rotation without folding"), Anim->Openness, 1.f);
		Reader->TickPaperTransition(10.f);
		TestEqual(TEXT("Close callback once"), Closed, 1);
		TestFalse(TEXT("Book scene released from reader control"), F.Stage->IsReading());
		TestTrue(TEXT("Document returns to original location/rotation/scale"), Seated.Equals(F.Stage->GetRightDocument()->GetComponentTransform(), 0.001f));
		TestEqual(TEXT("Returned document folded"), Anim->Openness, 0.f);
		Reader->bDiaryPresentationRestored = false;
		Reader->SetupModel();
		F.Stage->UpdateReading(0.7f, 0.5f, 1.f);
		Reader->ForceClose();
		Reader->ForceClose();
		TestFalse(TEXT("Forced close restores scene once"), F.Stage->IsReading());
		TestTrue(TEXT("Forced close restores exact seated pose"), Seated.Equals(F.Stage->GetRightDocument()->GetComponentTransform(), 0.001f));
	}
	return true;
}
#endif
