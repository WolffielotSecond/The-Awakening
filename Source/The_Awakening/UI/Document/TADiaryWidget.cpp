#include "UI/Document/TADiaryWidget.h"
#include "UI/Document/TADocumentReaderWidget.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentPresentation.h"
#include "Document/TADocumentSubsystem.h"
#include "Document/TAPreviewStage.h"
#include "Document/TADiaryPreviewStage.h"
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
#include "Blueprint/SlateBlueprintLibrary.h"
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
	TOptional<FVector2D> SourceScreenPosition, int32 OpenPage, bool bInSkipOpenAnimation, bool bInOpenDocumentImmediately, TOptional<FVector2D> SourceScreenSize, UWidget* SourceIcon)
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
	Diary->SourceSize = SourceScreenSize;
	Diary->SourceIconWidget = SourceIcon;
	Diary->RequestedPage = OpenPage;
	Diary->bSkipOpenAnimation = bInSkipOpenAnimation;
	Diary->bOpenDocumentImmediately = bInOpenDocumentImmediately;
	return Diary->TryOpen() ? Diary : nullptr;
}

bool UTADiaryWidget::TryOpen()
{
	// Validate cooked assets before attaching the modal layer. NativeConstruct runs inside
	// AddToViewport; removing there can happen before the viewport has attached the widget.
	if (!InitializeBookStage())
	{
		ReleasePreviewStages();
		return false;
	}
	AddToViewport(DiaryZOrder);
	if (!bInitialized)
	{
		RemoveFromParent();
		return false;
	}
	return true;
}

bool UTADiaryWidget::InitializeBookStage()
{
	if (!BookStage)
		BookStage = Cast<ATADiaryPreviewStage>(ATAPreviewStage::SpawnPreviewStage(this, ATADiaryPreviewStage::StaticClass(), 1));
	if (!BookStage) return false;
	BookStage->CameraPitch = BookCameraPitch;
	BookStage->FramingPadding = BookFramingPadding;
	BookStage->BookTransform = BookTransform;
	BookStage->DocumentSize = DocumentSize;
	BookStage->RightDocumentPosition = RightDocumentPosition;
	BookStage->PlaybackRate = BookPlaybackRate;
	if (!BookStage->InitializeDiary() || !BookStage->GetDisplayMaterial())
	{
		UE_LOG(LogTemp, Error, TEXT("Diary: 3D book assets or preview material unavailable; refusing to display modal UI."));
		return false;
	}
	return true;
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
	bInitialized = false;
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

	if (!Image_Book || !Panel_Book)
	{
		UE_LOG(LogTemp, Error, TEXT("Diary: book stage or Image_Book missing; rebuild with -run=TADocumentUI -RebuildDiaryOnly."));
		ReleasePreviewStages();
		return;
	}
	if (!InitializeBookStage())
	{
		ReleasePreviewStages();
		return;
	}
	BookStage->OnMotionFinished.AddUniqueDynamic(this, &UTADiaryWidget::HandleBookMotionFinished);
	FSlateBrush BookBrush;
	BookBrush.SetResourceObject(BookStage->GetDisplayMaterial());
	BookBrush.ImageSize = FVector2D(BookStage->RenderTargetSize);
	Image_Book->SetBrush(BookBrush);
	BookStage->SetCaptureEnabled(true);

	// 快速查看指定的页会覆盖保存的页码（策划文档）
	UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this);
	if (Documents && RequestedPage != INDEX_NONE)
	{
		Documents->SetDiaryPage(RequestedPage);
	}
	CurrentPage = Documents ? Documents->GetDiaryPageIndex() : 0;

	BuildControls();
	ShowPage(CurrentPage);

	ClosedBookBounds = BookStage->GetBookSurfaceScreenBounds();
	Phase = EPhase::Opening;
	ApplyOpenCloseVisual(0.f);
	if (bSkipOpenAnimation)
	{
		BookStage->SetOpenImmediate(true);
		Image_Book->SetRenderOpacity(0.f);
		Phase = EPhase::Open;
		ApplyOpenCloseVisual(1.f);
	}
	else
	{
		ClosedBookBounds = BookStage->GetBookSurfaceScreenBounds();
		bPendingOpenFromIcon = SourcePosition.IsSet() || SourceIconWidget.IsValid();
		if (bPendingOpenFromIcon) Panel_Book->SetRenderOpacity(0.f);
		else BookStage->OpenBook();
	}
	RefreshPageVisibility();
	SetIsFocusable(true);
	AcquireInput();
	// Wait for the book's first rendered layout/pose before handing it to the reader.
	bPendingQuickRead = bOpenDocumentImmediately && bSkipOpenAnimation;
	bInitialized = true;
}

void UTADiaryWidget::NativeDestruct()
{
	if (ActiveReader)
	{
		UTADocumentReaderWidget* Reader = ActiveReader;
		ActiveReader = nullptr;
		Reader->ForceClose();
	}
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
	ReleasePreviewStages();
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
	if (Phase == EPhase::Reading) return;
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
	if (BookStage && Phase != EPhase::Flipping && Phase != EPhase::Reading)
	{
		UTADocumentDefinition* Left = Documents && CurrentPage > 0
			? Documents->FindDefinition(Documents->GetDocumentAtPage(CurrentPage - 1)) : nullptr;
		BookStage->SetPages(Left, Definition);
	}
	RefreshControls();
	RefreshPageVisibility();
}

void UTADiaryWidget::TurnPage(int32 Direction)
{
	const int32 Count = GetPageCount();
	if (Phase != EPhase::Open || Count < 2 || Direction == 0)
	{
		return;
	}
	TurnToPage(FMath::Clamp(CurrentPage + (Direction > 0 ? 1 : -1), 0, Count - 1));
}

void UTADiaryWidget::TurnToPage(int32 Page)
{
	const int32 Count = GetPageCount();
	if (Phase != EPhase::Open || Count < 2 || Page < 0 || Page >= Count || Page == CurrentPage) return;
	UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this);
	if (!BookStage || !Documents) return;
	PendingPage = Page;
	UTADocumentDefinition* TargetRight = Documents->FindDefinition(Documents->GetDocumentAtPage(Page));
	UTADocumentDefinition* TargetLeft = Page > 0 ? Documents->FindDefinition(Documents->GetDocumentAtPage(Page - 1)) : nullptr;
	if (!BookStage->BeginPageTurn(Page > CurrentPage, TargetLeft, TargetRight)) return;
	Phase = EPhase::Flipping;
	RefreshPageVisibility();
}

void UTADiaryWidget::UpdatePageHold(float DeltaSeconds, bool bHeld, bool bOwnsInput)
{
	if (!bOwnsInput || (Phase != EPhase::Open && Phase != EPhase::Flipping))
	{
		HeldPageDirection = QueuedEndpoint = 0;
		bPageActionHeld = false;
		PageHeldTime = 0.f;
		return;
	}
	if (!bHeld)
	{
		HeldPageDirection = 0;
		bPageActionHeld = false;
		PageHeldTime = 0.f;
	}
	else if (HeldPageDirection && !bPageHoldTriggered)
	{
		PageHeldTime += FMath::Max(0.f, DeltaSeconds);
		if (PageHeldTime >= PageHoldDuration)
		{
			bPageHoldTriggered = true;
			QueuedEndpoint = HeldPageDirection;
		}
	}
	if (QueuedEndpoint && Phase == EPhase::Open)
	{
		const int32 Direction = QueuedEndpoint;
		QueuedEndpoint = 0;
		TurnToPage(Direction < 0 ? 0 : GetPageCount() - 1);
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
				Self->ActiveReader = nullptr;
				Self->RefreshPageVisibility();
			}
		}, BookStage.Get(), Image_Book.Get());
	if (Reader)
	{
		Phase = EPhase::Reading;
		ActiveReader = Reader;
		RefreshPageVisibility();
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
	auto Add = [this](UInputAction* Action, const FString& TextId, bool bClickable = true) -> UTAActionPromptWidget*
	{
		UTAActionPromptWidget* Prompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, Action, TextId, FLinearColor::White);
		if (Prompt && bClickable)
		{
			Prompt->OnPromptClicked.AddDynamic(this, &UTADiaryWidget::HandlePromptClicked);
		}
		return Prompt;
	};
	PreviousPrompt = Add(PreviousPageAction, PreviousPageTextId);
	NextPrompt = Add(NextPageAction, NextPageTextId);
	// Hold instructions share the live IMC icon; they are informational, not instant-jump buttons.
	FirstPageHoldPrompt = Add(PreviousPageAction, FirstPageHoldTextId, false);
	LastPageHoldPrompt = Add(NextPageAction, LastPageHoldTextId, false);
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
	if (FirstPageHoldPrompt) FirstPageHoldPrompt->SetVisibility(TurnVisibility);
	if (LastPageHoldPrompt) LastPageHoldPrompt->SetVisibility(TurnVisibility);
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
		for (UInputAction* Action : {PreviousPageAction.Get(), NextPageAction.Get(), ReadAction.Get(), BackAction.Get()}) PC->RegisterEnhancedUIAction(Action);
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

void UTADiaryWidget::ExecutePlayerInput(FKey, ETAInputCapability)
{
	// Keyboard/controller actions are evaluated by Enhanced Input, never Slate key-down dispatch.
}

void UTADiaryWidget::ExecutePlayerAction(const UInputAction* Action, ETriggerEvent Event, const FInputActionInstance& Instance)
{
	if (Action == BackAction && Event == ETriggerEvent::Started) { HandleMenuBackRequested(); return; }
	if (Action == ReadAction && Event == ETriggerEvent::Started && AllowsPlayerInput(ETAInputCapability::Confirm))
	{ OpenCurrentDocument(); return; }
	if (Action != PreviousPageAction && Action != NextPageAction) return;
	const int32 Direction = Action == PreviousPageAction ? -1 : 1;
	if (Event == ETriggerEvent::Completed || Event == ETriggerEvent::Canceled)
	{
		if (Direction == HeldPageDirection) bPageActionHeld = false;
		return;
	}
	if (Event != ETriggerEvent::Started || !AllowsPlayerInput(ETAInputCapability::Navigate)
		|| Phase != EPhase::Open || GetPageCount() < 2) return;
	HeldPageDirection = Direction;
	bPageActionHeld = true;
	PageHeldTime = 0.f;
	bPageHoldTriggered = false;
	TurnPage(Direction);
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
	if (!IsValid(BookStage) || !BookStage->IsReady() || bPendingOpenFromIcon)
	{
		RemoveFromParent();
		return;
	}
	if (Phase != EPhase::Open)
	{
		return;
	}
	UpdateSourceIconGeometry();
	if (!BookStage || !BookStage->CloseBook()) return;
	// 退出时保存页码
	if (UTADocumentSubsystem* Documents = UTADocumentSubsystem::Get(this))
	{
		Documents->SetDiaryPage(CurrentPage);
	}
	Phase = EPhase::Closing;
	RefreshPageVisibility();
}

void UTADiaryWidget::RemoveFromParent()
{
	bInitialized = false;
	if (ActiveReader)
	{
		UTADocumentReaderWidget* Reader = ActiveReader;
		ActiveReader = nullptr;
		Reader->ForceClose();
	}
	ReleaseInput();
	ReleasePreviewStages();
	Super::RemoveFromParent();
}

void UTADiaryWidget::ReleasePreviewStages()
{
	if (BookStage)
	{
		BookStage->OnMotionFinished.RemoveDynamic(this, &UTADiaryWidget::HandleBookMotionFinished);
		BookStage->SetCaptureEnabled(false);
		BookStage->Destroy();
		BookStage = nullptr;
	}
}

void UTADiaryWidget::RefreshPageVisibility()
{
	const bool bShowPage = Phase == EPhase::Open;
	if (Panel_Page) Panel_Page->SetVisibility(GetPageCount() > 0 && bShowPage
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Text_Empty) Text_Empty->SetVisibility(GetPageCount() == 0 && Phase == EPhase::Open
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UTADiaryWidget::HandleBookMotionFinished(ETADiaryMotion CompletedMotion)
{
	if (CompletedMotion == ETADiaryMotion::Opening && Phase == EPhase::Opening)
	{
		Phase = EPhase::Open;
		ApplyOpenCloseVisual(1.f);
		RefreshPageVisibility();
		if (bOpenDocumentImmediately) OpenCurrentDocument();
	}
	else if ((CompletedMotion == ETADiaryMotion::TurningForward || CompletedMotion == ETADiaryMotion::TurningBackward)
		&& Phase == EPhase::Flipping)
	{
		Phase = EPhase::Open;
		ShowPage(PendingPage);
	}
	else if (CompletedMotion == ETADiaryMotion::Closing && Phase == EPhase::Closing)
	{
		RemoveFromParent();
	}
}

void UTADiaryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!BookStage) return;
	UpdateSourceIconGeometry();
	if (bPendingOpenFromIcon && (Panel_Book->GetCachedGeometry().GetLocalSize().GetMin() <= 1.f
		|| (SourceIconWidget.IsValid() && !SourcePosition.IsSet())))
	{
		ApplyOpenCloseVisual(0.f);
		return;
	}
	if (bPendingOpenFromIcon)
	{
		bPendingOpenFromIcon = false;
		Panel_Book->SetRenderOpacity(1.f);
		ApplyOpenCloseVisual(0.f);
		BookStage->OpenBook();
	}
	if (Phase == EPhase::Opening) ApplyOpenCloseVisual(BookStage->GetMotionProgress());
	else if (Phase == EPhase::Closing) ApplyOpenCloseVisual(1.f - BookStage->GetMotionProgress());
	if (HeldPageDirection || QueuedEndpoint)
	{
		UpdatePageHold(InDeltaTime, bPageActionHeld, AllowsPlayerInput(ETAInputCapability::Navigate));
	}
	if (bSkipOpenAnimation && BookStage->IsPresentationReady()) Image_Book->SetRenderOpacity(1.f);
	if (bPendingQuickRead && BookStage->IsPresentationReady() && Phase == EPhase::Open && Image_Book->GetCachedGeometry().GetLocalSize().GetMin() > 1.f)
	{
		bPendingQuickRead = false;
		OpenCurrentDocument();
	}
}
void UTADiaryWidget::ApplyOpenCloseVisual(float Alpha)
{
	const float Eased = EaseOut(Alpha);
	// The book is visible from the icon-sized starting pose; fade only the surrounding UI.
	SetRenderOpacity(1.f);
	for (const FName Name : {FName(TEXT("Image_Dim")), FName(TEXT("Blur_Background"))})
		if (UWidget* Background = GetWidgetFromName(Name)) Background->SetRenderOpacity(Alpha);
	if (HorizontalBox_Controls) HorizontalBox_Controls->SetRenderOpacity(Alpha);
	if (Text_PageNumber) Text_PageNumber->SetRenderOpacity(Alpha);
	if (!Panel_Book) return;
	FVector2D Offset = FVector2D::ZeroVector;
	float StartScale = CollapsedScale;
	const FGeometry& Geometry = Panel_Book->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	if (SourcePosition.IsSet() && Size.GetMin() > 1.f)
	{
		// Layout geometry excludes our previous render transform and the ScaleBox's local scale.
		const FGeometry Layout = FGeometry::MakeRoot(Size, Geometry.GetAccumulatedLayoutTransform());
		const FGeometry& Root = GetCachedGeometry();
		Offset = Layout.AbsoluteToLocal(Root.LocalToAbsolute(SourcePosition.GetValue())) - Size * 0.5f;
		if (SourceSize.IsSet())
		{
			const FVector2D LocalSize = Layout.AbsoluteToLocal(Root.LocalToAbsolute(SourcePosition.GetValue() + SourceSize.GetValue()))
				- Layout.AbsoluteToLocal(Root.LocalToAbsolute(SourcePosition.GetValue()));
			if (ClosedBookBounds.bIsValid)
			{
				const FVector2D BookSize = Size * ClosedBookBounds.GetSize();
				StartScale = FMath::Clamp(float(FMath::Min(LocalSize.X / FMath::Max(1., BookSize.X),
					LocalSize.Y / FMath::Max(1., BookSize.Y))), 0.01f, 1.f);
			}
		}
	}
	if (SourcePosition.IsSet() && ClosedBookBounds.bIsValid)
		Offset -= (ClosedBookBounds.GetCenter() - FVector2D(0.5f)) * Size * StartScale;
	FWidgetTransform Transform;
	Transform.Translation = Offset * (1.f - Eased);
	Transform.Scale = FVector2D(FMath::Lerp(StartScale, 1.f, Eased));
	Panel_Book->SetRenderTransform(Transform);
}

FReply UTADiaryWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && Phase == EPhase::Open
		&& Image_Book && BookStage && AllowsPlayerInput(ETAInputCapability::Confirm))
	{
		const FGeometry& ImageGeometry = Image_Book->GetCachedGeometry();
		const FVector2D Size = ImageGeometry.GetLocalSize();
		if (Size.GetMin() > 1.f)
		{
			const FVector2D UV = ImageGeometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) / Size;
			const FBox2D Bounds = BookStage->GetRightDocumentScreenBounds();
			if (Bounds.bIsValid && Bounds.IsInside(UV))
			{
				OpenCurrentDocument();
				return FReply::Handled();
			}
		}
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void UTADiaryWidget::UpdateSourceIconGeometry()
{
	const UWidget* Icon = SourceIconWidget.Get();
	if (!Icon) return;
	const FGeometry& Geometry = Icon->GetCachedGeometry();
	if (Geometry.GetLocalSize().GetMin() <= 1.f) return;
	const FGeometry& Root = GetCachedGeometry();
	if (Root.GetLocalSize().GetMin() <= 1.f) return;
	SourcePosition = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f));
	SourceSize = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()))
		- Root.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
}
