#include "UI/Document/TADiaryWidget.h"
#include "UI/Document/TADocumentReaderWidget.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentSubsystem.h"
#include "Document/TAPreviewStage.h"
#include "Core/TAInputIconSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "The_AwakeningPlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "InputAction.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr int32 DiaryZOrder = 75;

	template <typename T>
	void DiaryLoadDefault(TObjectPtr<T>& Target, const TCHAR* Path)
	{
		if (!Target)
		{
			Target = LoadObject<T>(nullptr, Path);
		}
	}

	float EaseOut(float Alpha)
	{
		return 1.f - FMath::Square(1.f - FMath::Clamp(Alpha, 0.f, 1.f));
	}
}

UTADiaryWidget* UTADiaryWidget::Open(APlayerController* PC, TSubclassOf<UTADiaryWidget> WidgetClass,
	TOptional<FVector2D> SourceScreenPosition, int32 OpenPage, bool bInSkipOpenAnimation, bool bInOpenDocumentImmediately)
{
	UClass* Class = WidgetClass ? WidgetClass.Get() : LoadClass<UTADiaryWidget>(nullptr, TEXT("/Game/UI/Document/WBP_Diary.WBP_Diary_C"));
	if (!PC || !Class)
	{
		UE_LOG(LogTemp, Warning, TEXT("Diary: %s."), !Class ? TEXT("WBP_Diary missing; run -run=TADocumentUI") : TEXT("no player controller"));
		return nullptr;
	}
	UTADiaryWidget* Diary = CreateWidget<UTADiaryWidget>(PC, Class);
	if (!Diary)
	{
		return nullptr;
	}
	Diary->SourcePosition = SourceScreenPosition;
	Diary->RequestedPage = OpenPage;
	Diary->bSkipOpenAnimation = bInSkipOpenAnimation;
	Diary->bOpenDocumentImmediately = bInOpenDocumentImmediately;
	Diary->AddToViewport(DiaryZOrder);
	return Diary;
}

void UTADiaryWidget::LoadDefaultAssets()
{
	DiaryLoadDefault(PreviousPageAction, TEXT("/Game/Input/Actions/UI/IA_DiaryPreviousPage.IA_DiaryPreviousPage"));
	DiaryLoadDefault(NextPageAction, TEXT("/Game/Input/Actions/UI/IA_DiaryNextPage.IA_DiaryNextPage"));
	DiaryLoadDefault(ReadAction, TEXT("/Game/Input/Actions/Inventory/IA_Inventory_Confirm.IA_Inventory_Confirm"));
	DiaryLoadDefault(BackAction, TEXT("/Game/Input/Actions/IA_UIBack.IA_UIBack"));
}

void UTADiaryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadDefaultAssets();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddUniqueDynamic(this, &UTADiaryWidget::HandleLanguageChanged);
		}
		if (UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			Icons->OnInputPromptsChanged.AddUniqueDynamic(this, &UTADiaryWidget::HandleInputPresentationChanged);
			bGamepadDevice = Icons->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
		}
	}
	if (Button_Document)
	{
		Button_Document->OnClicked.AddUniqueDynamic(this, &UTADiaryWidget::HandleDocumentClicked);
	}

	// 页面上的文本模型：静止展示，不自动旋转
	PreviewStage = ATAPreviewStage::SpawnPreviewStage(this, nullptr, PreviewStageSlot);
	if (PreviewStage && Image_Document)
	{
		PreviewStage->RenderTargetSize = PageRenderSize;
		PreviewStage->SetAutoRotate(false);
		FSlateBrush Brush;
		UObject* Resource = PreviewStage->GetDisplayMaterial();
		Brush.SetResourceObject(Resource ? Resource : PreviewStage->GetRenderTarget());
		Brush.ImageSize = FVector2D(PageRenderSize);
		Image_Document->SetBrush(Brush);
	}

	// 快速查看指定的页会覆盖保存的页码（策划文档）
	UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this);
	if (Documents && RequestedPage != INDEX_NONE)
	{
		Documents->SetDiaryPage(RequestedPage);
	}
	CurrentPage = Documents ? Documents->GetDiaryPageIndex() : 0;

	BuildControls();
	ShowPage(CurrentPage);

	Phase = EPhase::Opening;
	PhaseTime = 0.f;
	ApplyOpenCloseVisual(OpenDuration > 0.f ? 0.f : 1.f);
	if (bSkipOpenAnimation)
	{
		Phase = EPhase::Open;
		ApplyOpenCloseVisual(1.f);
	}
	SetIsFocusable(true);
	AcquireInput();
	if (bOpenDocumentImmediately)
	{
		OpenCurrentDocument();
	}
}

void UTADiaryWidget::NativeDestruct()
{
	ReleaseInput();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTADiaryWidget::HandleLanguageChanged);
		}
		if (UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			Icons->OnInputPromptsChanged.RemoveDynamic(this, &UTADiaryWidget::HandleInputPresentationChanged);
		}
	}
	if (Button_Document)
	{
		Button_Document->OnClicked.RemoveDynamic(this, &UTADiaryWidget::HandleDocumentClicked);
	}
	if (PreviewStage)
	{
		PreviewStage->Destroy();
		PreviewStage = nullptr;
	}
	Super::NativeDestruct();
}

int32 UTADiaryWidget::GetPageCount() const
{
	const UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this);
	return Documents ? Documents->GetPageCount() : 0;
}

FText UTADiaryWidget::Localize(const FString& TextId) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	return Loc ? Loc->GetText(TextId) : FText::FromString(TextId);
}

// ==================== 页面 ====================

void UTADiaryWidget::ShowPage(int32 PageIndex)
{
	UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this);
	const int32 Count = GetPageCount();
	CurrentPage = Count > 0 ? FMath::Clamp(PageIndex, 0, Count - 1) : 0;
	UTADocumentDefinition* Definition = Documents && Count > 0 ? Documents->FindDefinition(Documents->GetDocumentAtPage(CurrentPage)) : nullptr;

	if (Text_Empty)
	{
		Text_Empty->SetText(Localize(EmptyTextId));
		Text_Empty->SetVisibility(Count == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Panel_Page)
	{
		Panel_Page->SetVisibility(Definition ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Text_PageNumber)
	{
		Text_PageNumber->SetText(Count > 0 ? FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentPage + 1, Count)) : FText::GetEmpty());
	}
	if (Text_DocumentTitle)
	{
		Text_DocumentTitle->SetText(Definition ? Localize(Definition->TitleTextId) : FText::GetEmpty());
	}
	if (PreviewStage)
	{
		if (!Definition)
		{
			PreviewStage->ClearDisplay();
		}
		else
		{
			if (UObject* Mesh = Definition->Mesh.Get())
			{
				// 静态 / 骨骼网格体由预览台按类型显示
				PreviewStage->SetDisplayModel(Mesh, Definition->DiaryPageTransform);
			}
			else
			{
				// 没有模型时用一张正对镜头的纸（占位立方体压扁）
				PreviewStage->SetDisplayMesh(nullptr, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.01f, 0.21f, 0.3f)));
			}
			PreviewStage->SetCaptureEnabled(true);
		}
	}
	RefreshControls();
}

void UTADiaryWidget::TurnPage(int32 Direction)
{
	const int32 Count = GetPageCount();
	if (Phase != EPhase::Open || Count < 2 || Direction == 0)
	{
		return;
	}
	PendingPage = (CurrentPage + (Direction > 0 ? 1 : -1) + Count) % Count;
	Phase = EPhase::Flipping;
	PhaseTime = 0.f;
	bPageSwapped = false;
	if (PageFlipDuration <= 0.f)
	{
		ShowPage(PendingPage);
		Phase = EPhase::Open;
	}
}

void UTADiaryWidget::OpenCurrentDocument()
{
	if (Phase != EPhase::Open)
	{
		return;
	}
	UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this);
	UTADocumentDefinition* Definition = Documents && GetPageCount() > 0 ? Documents->FindDefinition(Documents->GetDocumentAtPage(CurrentPage)) : nullptr;
	if (!Definition)
	{
		return;
	}
	// 预览界面优先级比日记本高；时停沿用背包的。关闭后回到日记本
	UTADocumentReaderWidget* Reader = UTADocumentReaderWidget::Open(GetOwningPlayer(), ReaderWidgetClass, Definition, InputPriority + 10, false,
		[WeakThis = TWeakObjectPtr<UTADiaryWidget>(this)](bool)
		{
			if (UTADiaryWidget* Self = WeakThis.Get(); Self && Self->Phase == EPhase::Reading)
			{
				Self->Phase = EPhase::Open;
			}
		});
	if (Reader)
	{
		Phase = EPhase::Reading;
	}
}

void UTADiaryWidget::HandleDocumentClicked()
{
	if (AllowsPlayerInput(ETAInputCapability::Confirm))
	{
		OpenCurrentDocument();
	}
}

// ==================== 操作栏 ====================

void UTADiaryWidget::BuildControls()
{
	if (!HorizontalBox_Controls)
	{
		return;
	}
	HorizontalBox_Controls->ClearChildren();
	auto Add = [this](UInputAction* Action, const FString& TextId) -> UTAActionPromptWidget*
	{
		UTAActionPromptWidget* Prompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, Action, TextId);
		if (Prompt)
		{
			Prompt->OnPromptClicked.AddDynamic(this, &UTADiaryWidget::HandlePromptClicked);
		}
		return Prompt;
	};
	PreviousPrompt = Add(PreviousPageAction, PreviousPageTextId);
	NextPrompt = Add(NextPageAction, NextPageTextId);
	ReadPrompt = Add(ReadAction, ReadTextId);
	BackPrompt = Add(BackAction, BackTextId);
	if (!PreviousPageAction || !NextPageAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("IA_DiaryPreviousPage / IA_DiaryNextPage missing; run -run=TADocumentUI and map their keys in IMC_UI."));
	}
}

void UTADiaryWidget::RefreshControls()
{
	const int32 Count = GetPageCount();
	// 0 / 1 份不能翻页，0 份没有可查看的文本：对应提示收起
	const ESlateVisibility TurnVisibility = Count >= 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (PreviousPrompt) PreviousPrompt->SetVisibility(TurnVisibility);
	if (NextPrompt) NextPrompt->SetVisibility(TurnVisibility);
	if (ReadPrompt) ReadPrompt->SetVisibility(Count > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UTADiaryWidget::HandlePromptClicked(UTAActionPromptWidget* Prompt)
{
	if (Prompt == PreviousPrompt && AllowsPlayerInput(ETAInputCapability::Navigate)) TurnPage(-1);
	else if (Prompt == NextPrompt && AllowsPlayerInput(ETAInputCapability::Navigate)) TurnPage(1);
	else if (Prompt == ReadPrompt && AllowsPlayerInput(ETAInputCapability::Confirm)) OpenCurrentDocument();
	else if (Prompt == BackPrompt && AllowsPlayerInput(ETAInputCapability::Close)) RequestClose();
}

void UTADiaryWidget::HandleInputPresentationChanged()
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			bGamepadDevice = Icons->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
		}
	}
	if (AThe_AwakeningPlayerController* PC = InputRequestController.Get())
	{
		PC->SynchronizeInputPresentation();
	}
}

void UTADiaryWidget::HandleLanguageChanged()
{
	ShowPage(CurrentPage);
}

// ==================== 输入 ====================

void UTADiaryWidget::AcquireInput()
{
	if (InputRequestHandle)
	{
		return;
	}
	if (auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		FTAInputRequest Request;
		Request.Owner = this;
		Request.Priority = InputPriority;
		Request.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Cursor, ETAInputCapability::Confirm, ETAInputCapability::Close};
		Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
		Request.Presentation.bShowCursor = true;
		Request.Presentation.Focus = ETAInputFocusRequirement::Target;
		Request.Presentation.FocusTarget = this;
		InputRequestController = PC;
		InputRequestHandle = PC->AcquireInputRequest(Request);
	}
}

void UTADiaryWidget::ReleaseInput()
{
	const auto Handle = InputRequestHandle;
	InputRequestHandle = 0;
	const auto Issuer = InputRequestController;
	InputRequestController.Reset();
	if (Handle)
		if (auto* PC = Issuer.Get()) PC->ReleaseInputRequest(Handle);
}

bool UTADiaryWidget::AllowsPlayerInput(ETAInputCapability Capability) const
{
	const auto* PC = InputRequestController.Get();
	return PC && PC->AllowsInputFor(InputRequestHandle, this, Capability);
}

TOptional<ETAInputCapability> UTADiaryWidget::ResolvePlayerInput(FKey Key) const
{
	const auto* PC = InputRequestController.Get();
	if (!PC) return {};
	if ((PreviousPageAction && PC->IsKeyMappedToAction(Key, PreviousPageAction)) || (NextPageAction && PC->IsKeyMappedToAction(Key, NextPageAction)))
		return ETAInputCapability::Navigate;
	if (ReadAction && PC->IsKeyMappedToAction(Key, ReadAction)) return ETAInputCapability::Confirm;
	return {};
}

void UTADiaryWidget::ExecutePlayerInput(FKey Key, ETAInputCapability Capability)
{
	if (!AllowsPlayerInput(Capability)) return;
	const auto* PC = InputRequestController.Get();
	if (Capability == ETAInputCapability::Navigate)
	{
		TurnPage(PC && PreviousPageAction && PC->IsKeyMappedToAction(Key, PreviousPageAction) ? -1 : 1);
	}
	else if (Capability == ETAInputCapability::Confirm)
	{
		OpenCurrentDocument();
	}
}

bool UTADiaryWidget::HandleMenuBackRequested()
{
	if (!AllowsPlayerInput(ETAInputCapability::Close))
	{
		return false;
	}
	// 动画没播完之前返回键无效，但要吞掉，不能漏给背包
	RequestClose();
	return true;
}

void UTADiaryWidget::RequestClose()
{
	if (Phase != EPhase::Open)
	{
		return;
	}
	// 退出时保存页码
	if (UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this))
	{
		Documents->SetDiaryPage(CurrentPage);
	}
	Phase = EPhase::Closing;
	PhaseTime = 0.f;
	if (CloseDuration <= 0.f)
	{
		RemoveFromParent();
	}
}

void UTADiaryWidget::RemoveFromParent()
{
	ReleaseInput();
	Super::RemoveFromParent();
}

// ==================== 动画（UMG 2D 占位） ====================

void UTADiaryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PhaseTime += InDeltaTime;
	switch (Phase)
	{
	case EPhase::Opening:
	{
		const float Alpha = OpenDuration > 0.f ? PhaseTime / OpenDuration : 1.f;
		ApplyOpenCloseVisual(Alpha);
		if (Alpha >= 1.f)
		{
			Phase = EPhase::Open;
		}
		break;
	}
	case EPhase::Closing:
	{
		const float Alpha = CloseDuration > 0.f ? 1.f - PhaseTime / CloseDuration : 0.f;
		ApplyOpenCloseVisual(Alpha);
		if (Alpha <= 0.f)
		{
			RemoveFromParent();
		}
		break;
	}
	case EPhase::Flipping:
	{
		const float Progress = PageFlipDuration > 0.f ? PhaseTime / PageFlipDuration : 1.f;
		if (!bPageSwapped && Progress >= 0.5f)
		{
			ShowPage(PendingPage);
			bPageSwapped = true;
		}
		ApplyPageFlipVisual(Progress);
		if (Progress >= 1.f)
		{
			ApplyPageFlipVisual(1.f);
			Phase = EPhase::Open;
		}
		break;
	}
	default:
		break;
	}
}

void UTADiaryWidget::ApplyOpenCloseVisual(float Alpha)
{
	const float Eased = EaseOut(Alpha);
	SetRenderOpacity(FMath::Clamp(Alpha, 0.f, 1.f));
	if (!Panel_Book)
	{
		return;
	}
	// 从剧情格位置放大到屏幕中央；关闭时反向缩回
	FVector2D Offset = FVector2D::ZeroVector;
	if (SourcePosition.IsSet())
	{
		const FVector2D Viewport = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
		Offset = SourcePosition.GetValue() - Viewport * 0.5f;
	}
	FWidgetTransform Transform;
	Transform.Translation = Offset * (1.f - Eased);
	Transform.Scale = FVector2D(FMath::Lerp(CollapsedScale, 1.f, Eased));
	Panel_Book->SetRenderTransform(Transform);
}

void UTADiaryWidget::ApplyPageFlipVisual(float Progress)
{
	if (!Panel_Page)
	{
		return;
	}
	// 前一半旧页淡出并略向外滑，后一半新页淡入回到原位
	const float P = FMath::Clamp(Progress, 0.f, 1.f);
	const float Fade = P < 0.5f ? 1.f - P * 2.f : (P - 0.5f) * 2.f;
	Panel_Page->SetRenderOpacity(Fade);
	FWidgetTransform Transform;
	Transform.Translation = FVector2D((1.f - Fade) * 30.f, 0.f);
	Panel_Page->SetRenderTransform(Transform);
}
