#include "UI/Document/TADocumentReaderWidget.h"
#include "UI/Document/TAJustifiedRichTextBlock.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentPresentation.h"
#include "Components/BackgroundBlur.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Document/TADocumentSettings.h"
#include "Document/TAPreviewStage.h"
#include "Document/TADiaryPreviewStage.h"
#include "Core/TALocalizeSubsystem.h"
#include "Core/TAInputIconSubsystem.h"
#include "Core/TATimeSubsystem.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "The_AwakeningPlayerController.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "InputAction.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogTADocumentReader, Log, All);

namespace
{
	constexpr int32 ReaderZOrder = 80;

	template <typename T>
	void ReaderLoadDefault(TObjectPtr<T>& Target, const TCHAR* Path)
	{
		if (!Target)
		{
			Target = LoadObject<T>(nullptr, Path);
		}
	}
}

UTADocumentReaderWidget* UTADocumentReaderWidget::Open(APlayerController* PC, TSubclassOf<UTADocumentReaderWidget> WidgetClass,
	UTADocumentDefinition* InDocument, int32 InInputPriority, bool bInFreezeTime, TFunction<void(bool)> InOnClosed,
	ATAPreviewStage* SharedStage, UImage* DiaryImage)
{
	UClass* Class = WidgetClass ? WidgetClass.Get() : GetDefault<UTADocumentSettings>()->ReaderWidgetClass.LoadSynchronous();
	if (!PC || !InDocument || !Class)
	{
		UE_LOG(LogTADocumentReader, Warning, TEXT("Open: %s."), !Class ? TEXT("reader widget class not set in Project Settings > The Awakening Documents")
			: !InDocument ? TEXT("no document") : TEXT("no player controller"));
		return nullptr;
	}
	UTADocumentReaderWidget* Reader = CreateWidget<UTADocumentReaderWidget>(PC, Class);
	if (!Reader)
	{
		return nullptr;
	}
	Reader->Document = InDocument;
	Reader->InputPriority = InInputPriority;
	Reader->bFreezeTime = bInFreezeTime;
	Reader->OnClosed = MoveTemp(InOnClosed);
	Reader->PreviewStage = SharedStage;
	Reader->bBorrowedStage = SharedStage != nullptr;
	Reader->SourceDiaryImage = DiaryImage;
	if (DiaryImage)
	{
		const FGeometry& Geometry = DiaryImage->GetCachedGeometry();
		Reader->DiaryAbsoluteCenter = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
		Reader->DiaryAbsoluteSize = Geometry.LocalToAbsolute(Geometry.GetLocalSize()) - Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	}
	Reader->AddToViewport(ReaderZOrder);
	return Reader;
}

void UTADocumentReaderWidget::LoadDefaultAssets()
{
	ReaderLoadDefault(TranslateAction, TEXT("/Game/Input/Actions/UI/IA_DocumentTranslate.IA_DocumentTranslate"));
	ReaderLoadDefault(RotateAction, TEXT("/Game/Input/Actions/UI/IA_DocumentRotate.IA_DocumentRotate"));
	ReaderLoadDefault(ScrollAction, TEXT("/Game/Input/Actions/UI/IA_ReaderScroll.IA_ReaderScroll"));
	ReaderLoadDefault(BackAction, TEXT("/Game/Input/Actions/IA_UIBack.IA_UIBack"));
}

void UTADocumentReaderWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadDefaultAssets();

	if (ScrollBox_Body)
	{
		// 滚轮交给本控件做平滑滚动；滚动条拖动仍由 ScrollBox 处理
		ScrollBox_Body->SetConsumeMouseWheel(EConsumeMouseWheel::Never);
		ScrollBox_Body->SetAnimateWheelScrolling(false);
		ScrollBox_Body->SetScrollOffset(0.f);
	}
	// 没有配置文字样式表时，正文沿用标题的字体（项目全局字体，支持中英文），只改字号
	if (RichText_Body && Text_Title && !RichText_Body->GetTextStyleSet())
	{
		FTextBlockStyle Style;
		FSlateFontInfo Font = Text_Title->GetFont();
		Font.Size = BodyFontSize;
		Style.SetFont(Font);
		Style.SetColorAndOpacity(Text_Title->GetColorAndOpacity());
		RichText_Body->SetDefaultTextStyle(Style);
	}
	SetupTranslationPresentation();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddUniqueDynamic(this, &UTADocumentReaderWidget::HandleLanguageChanged);
		}
		if (UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			Icons->OnInputPromptsChanged.AddUniqueDynamic(this, &UTADocumentReaderWidget::RefreshPromptVisibility);
		}
	}

	RefreshText();
	SetupModel();
	BuildControls();

	bTranslationVisible = false;
	TranslationAlpha = 0.f;
	TickTranslation(0.f);
	if (Panel_Translation)
	{
		Panel_Translation->SetRenderOpacity(0.f);
		Panel_Translation->SetVisibility(ESlateVisibility::Collapsed);
	}
	ScrollTarget = LastAppliedScroll = 0.f;
	Phase = EPhase::Opening;
	PhaseTime = 0.f;
	SetRenderOpacity(bPaperTransition ? 1.f : (OpenDuration > 0.f ? 0.f : 1.f));
	if (bPaperTransition)
	{
		if (Image_Model) Image_Model->SetRenderOpacity(0.f); // Wait for the first valid layout.
		if (UWidget* Dim = GetWidgetFromName(TEXT("Image_Dim"))) Dim->SetRenderOpacity(0.f);
		if (UWidget* Blur = GetWidgetFromName(TEXT("Blur_Background"))) Blur->SetRenderOpacity(0.f);
		if (HorizontalBox_Controls) HorizontalBox_Controls->SetRenderOpacity(0.f);
	}
	SetIsFocusable(true);
	AcquireInputAndFreeze();
}

void UTADocumentReaderWidget::NativeDestruct()
{
	ReleaseInputAndFreeze();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTADocumentReaderWidget::HandleLanguageChanged);
		}
		if (UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			Icons->OnInputPromptsChanged.RemoveDynamic(this, &UTADocumentReaderWidget::RefreshPromptVisibility);
		}
	}
	RestoreDiaryPresentation();
	if (PreviewStage && !bBorrowedStage)
	{
		PreviewStage->Destroy();
		PreviewStage = nullptr;
	}
	Super::NativeDestruct();
}

void UTADocumentReaderWidget::SetupTranslationPresentation()
{
	// Upgrade the existing Blueprint layout in place, preserving the model, text and controls.
	if (auto* Border = Cast<UBorder>(Panel_Translation))
	{
		Border->SetBrushColor(FLinearColor::Transparent);
		Border->SetHorizontalAlignment(HAlign_Fill);
		Border->SetVerticalAlignment(VAlign_Center);
		Border->SetPadding(FMargin(0.f, 32.f));
	}
	if (Text_Title)
	{
		Text_Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Text_Title->SetJustification(ETextJustify::Center);
	}
	if (RichText_Body)
	{
		if (!RichText_Body->IsA<UTAJustifiedRichTextBlock>() && WidgetTree && ScrollBox_Body)
		{
			auto* JustifiedBody = WidgetTree->ConstructWidget<UTAJustifiedRichTextBlock>(UTAJustifiedRichTextBlock::StaticClass(), TEXT("RichText_JustifiedBody"));
			// Retain the Blueprint's typography, markup, style table and decorator classes.
			// Do not copy transient decorator instances or UWidget ownership/slot state.
			for (TFieldIterator<FProperty> It(URichTextBlock::StaticClass()); It; ++It)
			{
				if ((It->GetOwnerClass() == URichTextBlock::StaticClass() || It->GetOwnerClass() == UTextLayoutWidget::StaticClass())
					&& !It->HasAnyPropertyFlags(CPF_Transient))
				{
					It->CopyCompleteValue_InContainer(JustifiedBody, RichText_Body);
				}
			}
			RichText_Body->TakeWidget(); // Resolve a style table even if the original body has not been built yet.
			JustifiedBody->SetDefaultTextStyle(RichText_Body->GetCurrentDefaultTextStyle());
			RichText_Body->RemoveFromParent();
			ScrollBox_Body->AddChild(JustifiedBody);
			RichText_Body = JustifiedBody;
		}
		RichText_Body->SetDefaultColorAndOpacity(FSlateColor(FLinearColor::White));
		RichText_Body->SetJustification(ETextJustify::Left);
		RichText_Body->SetAutoWrapText(true);
		RichText_Body->SetWrapTextAt(0.f);
	}

	auto* Canvas = Panel_Translation ? Cast<UCanvasPanel>(Panel_Translation->GetParent()) : nullptr;
	auto* TextSlot = Panel_Translation ? Cast<UCanvasPanelSlot>(Panel_Translation->Slot) : nullptr;
	auto* ControlsSlot = HorizontalBox_Controls ? Cast<UCanvasPanelSlot>(HorizontalBox_Controls->Slot) : nullptr;
	if (!WidgetTree || !Canvas || !TextSlot || !ControlsSlot || HorizontalBox_Controls->GetParent() != Canvas)
	{
		UE_LOG(LogTADocumentReader, Warning, TEXT("Translation presentation requires translation and controls in the same CanvasPanel."));
		return;
	}
	TextSlot->SetAnchors(FAnchors(1.f / 3.f, 0.12f, 2.f / 3.f, 0.88f));
	TextSlot->SetAlignment(FVector2D::ZeroVector);
	TextSlot->SetOffsets(FMargin(0.f));
	TextSlot->SetAutoSize(false);
	TranslationBlur = Cast<UBackgroundBlur>(GetWidgetFromName(TEXT("Blur_TranslationBackground")));
	TranslationDim = Cast<UImage>(GetWidgetFromName(TEXT("Image_TranslationDim")));
	if (!TranslationBlur)
	{
		TranslationBlur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("Blur_TranslationBackground"));
		Canvas->AddChildToCanvas(TranslationBlur);
	}
	if (!TranslationDim)
	{
		TranslationDim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_TranslationDim"));
		Canvas->AddChildToCanvas(TranslationDim);
	}
	int32 BackdropZ = 0;
	for (UWidget* Child : Canvas->GetAllChildren())
	{
		if (Child == Panel_Translation || Child == HorizontalBox_Controls || Child == TranslationBlur || Child == TranslationDim) continue;
		if (auto* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot)) BackdropZ = FMath::Max(BackdropZ, ChildSlot->GetZOrder() + 1);
	}
	for (UWidget* Backdrop : {static_cast<UWidget*>(TranslationBlur.Get()), static_cast<UWidget*>(TranslationDim.Get())})
	{
		auto* BackdropSlot = CastChecked<UCanvasPanelSlot>(Backdrop->Slot);
		BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackdropSlot->SetOffsets(FMargin(0.f));
		BackdropSlot->SetZOrder(Backdrop == TranslationBlur ? BackdropZ : BackdropZ + 1);
	}
	TextSlot->SetZOrder(BackdropZ + 2);
	ControlsSlot->SetZOrder(BackdropZ + 3);
}

void UTADocumentReaderWidget::TickTranslation(float DeltaTime)
{
	const float Step = TranslationFadeDuration > 0.f ? DeltaTime / TranslationFadeDuration : 1.f;
	TranslationAlpha = FMath::Clamp(TranslationAlpha + (bTranslationVisible ? Step : -Step), 0.f, 1.f);
	const bool bShow = bTranslationVisible || TranslationAlpha > 0.f;
	if (Panel_Translation)
	{
		Panel_Translation->SetRenderOpacity(TranslationAlpha);
		Panel_Translation->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	// These siblings draw after the model but before the translation and bottom controls.
	if (TranslationBlur)
	{
		TranslationBlur->SetBlurStrength(TranslationBlurStrength);
		TranslationBlur->SetRenderOpacity(TranslationAlpha);
		TranslationBlur->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TranslationDim)
	{
		TranslationDim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, TranslationDimOpacity));
		TranslationDim->SetRenderOpacity(TranslationAlpha);
		TranslationDim->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UTADocumentReaderWidget::SetupModel()
{
	if (!Image_Model)
	{
		return;
	}
	if (!PreviewStage) PreviewStage = ATAPreviewStage::SpawnPreviewStage(this, nullptr, PreviewStageSlot);
	if (!PreviewStage)
	{
		return;
	}
	if (!bBorrowedStage) PreviewStage->RenderTargetSize = ModelRenderSize;
	PreviewStage->SetAutoRotate(false);
	if (auto* Diary = Cast<ATADiaryPreviewStage>(PreviewStage))
	{
		bPaperTransition = Diary->BeginReading();
		FSlateBrush Brush;
		Brush.SetResourceObject(Diary->GetDisplayMaterial());
		Brush.ImageSize = FVector2D(Diary->RenderTargetSize);
		Image_Model->SetBrush(Brush);
		return;
	}
	bPaperTransition = Document && Document->Presentation && Document->Presentation->AnimationClass;
	DiaryModelTransform = PreviewStage->GetDiaryModelTransform(Document);
	ReaderModelTransform = Document ? Document->GetPreviewTransform(Document->ReaderTransform) : FTransform::Identity;
	if (bPaperTransition)
	{
		// The asset corrects model axes; this additional rotation faces the actual preview camera.
		ReaderModelTransform.SetRotation(FQuat(FRotator(PreviewStage->CameraPitch, 0.f, 0.f)) * ReaderModelTransform.GetRotation());
	}
	if (UObject* Mesh = Document ? Document->Mesh.Get() : nullptr)
	{
		// 静态 / 骨骼网格体由预览台按类型显示
		if (!bBorrowedStage) PreviewStage->SetDocument(Document, Document->ReaderTransform, !bPaperTransition);
		PreviewStage->SetModelPresentationTransform(bPaperTransition && bBorrowedStage ? DiaryModelTransform : ReaderModelTransform);
	}
	else
	{
		// 没有模型时用一张正对镜头的纸（占位立方体压扁）
		PreviewStage->SetDisplayMesh(nullptr, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.01f, 0.21f, 0.3f)));
	}
	PreviewStage->SetCaptureEnabled(true);

	// 透明背景要经显示材质还原透明度；材质缺失时直接显示 RenderTarget
	FSlateBrush Brush;
	UObject* Resource = PreviewStage->GetDisplayMaterial();
	Brush.SetResourceObject(Resource ? Resource : PreviewStage->GetRenderTarget());
	Brush.ImageSize = FVector2D(ModelRenderSize);
	Image_Model->SetBrush(Brush);
	Image_Model->SetRenderTransformPivot(FVector2D(0.5f));
}

void UTADocumentReaderWidget::BuildControls()
{
	if (!HorizontalBox_Controls)
	{
		return;
	}
	HorizontalBox_Controls->ClearChildren();
	TranslatePrompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, TranslateAction, ShowTranslationTextId, FLinearColor::White);
	if (TranslatePrompt)
	{
		TranslatePrompt->OnPromptClicked.AddDynamic(this, &UTADocumentReaderWidget::HandlePromptClicked);
	}
	// 旋转只是提示，不可点击（没有绑定点击事件）
	RotatePrompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, RotateAction, RotateTextId, FLinearColor::White);
	BackPrompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, BackAction, BackTextId, FLinearColor::White);
	if (BackPrompt)
	{
		BackPrompt->OnPromptClicked.AddDynamic(this, &UTADocumentReaderWidget::HandlePromptClicked);
	}
	RefreshPromptVisibility();
	if (!TranslateAction || !RotateAction)
	{
		UE_LOG(LogTADocumentReader, Warning, TEXT("IA_DocumentTranslate / IA_DocumentRotate missing; run -run=TADocumentUI and map their keys in IMC_UI."));
	}
}

void UTADocumentReaderWidget::RefreshPromptVisibility()
{
	const UGameInstance* GI = GetGameInstance();
	const UTAInputIconSubsystem* Icons = GI ? GI->GetSubsystem<UTAInputIconSubsystem>() : nullptr;
	bGamepadDevice = Icons && Icons->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
	if (RotatePrompt)
	{
		RotatePrompt->SetVisibility(bGamepadDevice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (bGamepadDevice)
	{
		bDraggingModel = false;
	}
	if (AThe_AwakeningPlayerController* PC = InputRequestController.Get())
	{
		PC->SynchronizeInputPresentation();
	}
}

void UTADocumentReaderWidget::HandlePromptClicked(UTAActionPromptWidget* Prompt)
{
	if (Prompt == TranslatePrompt && AllowsPlayerInput(ETAInputCapability::Confirm))
	{
		ToggleTranslation();
	}
	else if (Prompt == BackPrompt && AllowsPlayerInput(ETAInputCapability::Close))
	{
		RequestClose();
	}
}

void UTADocumentReaderWidget::RefreshText()
{
	const UGameInstance* GI = GetGameInstance();
	const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	auto Localize = [Loc](const FString& Id) { return Loc ? Loc->GetText(Id) : FText::FromString(Id); };
	if (Text_Title)
	{
		Text_Title->SetText(Document ? Localize(Document->TitleTextId) : FText::GetEmpty());
	}
	if (RichText_Body)
	{
		RichText_Body->SetText(Document ? Localize(Document->BodyTextId) : FText::GetEmpty());
	}
}

void UTADocumentReaderWidget::HandleLanguageChanged()
{
	RefreshText();
}

// ==================== 输入与时停 ====================

void UTADocumentReaderWidget::AcquireInputAndFreeze()
{
	if (!InputRequestHandle)
	{
		if (auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
		{
			FTAInputRequest Request;
			Request.Owner = this;
			Request.Priority = InputPriority;
			// Confirm = 显示 / 收起转译；Navigate = 滚动和旋转；Close = 返回
			Request.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Cursor, ETAInputCapability::Confirm, ETAInputCapability::Close};
			Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
			Request.Presentation.bShowCursor = true;
			Request.Presentation.Focus = ETAInputFocusRequirement::Target;
			Request.Presentation.FocusTarget = this;
			InputRequestController = PC;
			InputRequestHandle = PC->AcquireInputRequest(Request);
			for (UInputAction* Action : {TranslateAction.Get(), BackAction.Get(), RotateAction.Get(), ScrollAction.Get()}) PC->RegisterEnhancedUIAction(Action);
		}
	}
	if (bFreezeTime && !bFreezeRequested)
	{
		if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this))
		{
			Time->RequestFreeze(this, 1.f);
			bFreezeRequested = true;
		}
	}
}

void UTADocumentReaderWidget::ReleaseInputAndFreeze()
{
	ActionRotation = FVector2D::ZeroVector;
	ActionScroll = 0.f;
	const auto Handle = InputRequestHandle;
	InputRequestHandle = 0;
	const auto Issuer = InputRequestController;
	InputRequestController.Reset();
	if (Handle)
		if (auto* PC = Issuer.Get()) PC->ReleaseInputRequest(Handle);
	if (bFreezeRequested)
	{
		bFreezeRequested = false;
		if (UTATimeSubsystem* Time = UTATimeSubsystem::Get(this)) Time->ReleaseFreeze(this);
	}
}

bool UTADocumentReaderWidget::AllowsPlayerInput(ETAInputCapability Capability) const
{
	const auto* PC = InputRequestController.Get();
	return PC && PC->AllowsInputFor(InputRequestHandle, this, Capability);
}

TOptional<ETAInputCapability> UTADocumentReaderWidget::ResolvePlayerInput(FKey Key) const
{
	const auto* PC = InputRequestController.Get();
	if (PC && TranslateAction && PC->IsKeyMappedToAction(Key, TranslateAction)) return ETAInputCapability::Confirm;
	return {};
}

void UTADocumentReaderWidget::ExecutePlayerInput(FKey, ETAInputCapability) {}

void UTADocumentReaderWidget::ExecutePlayerAction(const UInputAction* Action, ETriggerEvent Event, const FInputActionInstance& Instance)
{
	if (Event == ETriggerEvent::Started)
	{
		if (Action == BackAction) HandleMenuBackRequested();
		else if (Action == TranslateAction && AllowsPlayerInput(ETAInputCapability::Confirm)) ToggleTranslation();
	}
	const bool bTriggered = Event == ETriggerEvent::Triggered && AllowsPlayerInput(ETAInputCapability::Navigate);
	if (Action == RotateAction) ActionRotation = bTriggered ? Instance.GetValue().Get<FVector2D>() : FVector2D::ZeroVector;
	if (Action == ScrollAction) ActionScroll = bTriggered ? Instance.GetValue().Get<float>() : 0.f;
}

bool UTADocumentReaderWidget::HandleMenuBackRequested()
{
	if (!AllowsPlayerInput(ETAInputCapability::Close))
	{
		return false;
	}
	// 动画没播完之前返回键无效，但要吞掉，不能漏给下层界面
	RequestClose();
	return true;
}

void UTADocumentReaderWidget::RequestClose()
{
	if (Phase != EPhase::Open)
	{
		return;
	}
	Phase = EPhase::Closing;
	PhaseTime = 0.f;
	bDraggingModel = false;
	if (bPaperTransition && PreviewStage) ClosingManualRotation = PreviewStage->GetManualRotation();
	if (bPaperTransition && Image_Model)
	{
		if (!Cast<ATADiaryPreviewStage>(PreviewStage)) ReaderTargetGeometry = Image_Model->GetCachedGeometry();
		if (UImage* Source = SourceDiaryImage.Get())
		{
			const FGeometry& Geometry = Source->GetCachedGeometry();
			DiaryAbsoluteCenter = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
			DiaryAbsoluteSize = Geometry.LocalToAbsolute(Geometry.GetLocalSize()) - Geometry.LocalToAbsolute(FVector2D::ZeroVector);
		}
	}
	bTranslationVisible = false;
	if (CloseDuration <= 0.f && !bPaperTransition)
	{
		FinishClose(false);
	}
}

void UTADocumentReaderWidget::ToggleTranslation()
{
	if (Phase != EPhase::Open)
	{
		return;
	}
	bTranslationVisible = !bTranslationVisible;
	TickTranslation(0.f);
	if (TranslatePrompt)
	{
		TranslatePrompt->ConfigureLocalizedPrompt(TranslateAction, bTranslationVisible ? HideTranslationTextId : ShowTranslationTextId);
	}
}

void UTADocumentReaderWidget::ForceClose()
{
	FinishClose(true);
}

void UTADocumentReaderWidget::FinishClose(bool bForced)
{
	bClosedNormally = !bForced;
	RemoveFromParent();
}

void UTADocumentReaderWidget::RemoveFromParent()
{
	RestoreDiaryPresentation();
	ReleaseInputAndFreeze();
	// 先移出界面再回调：回调里可能打开下一个界面或播放放下动画
	TFunction<void(bool)> Callback = MoveTemp(OnClosed);
	OnClosed = nullptr;
	const bool bForced = !bClosedNormally;
	Super::RemoveFromParent();
	if (Callback)
	{
		Callback(bForced);
	}
}

// ==================== 每帧：动画、旋转、滚动 ====================

void UTADocumentReaderWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// InDeltaTime 是 UI 的真实时间，不受时停影响
	if (bPaperTransition)
	{
		TickPaperTransition(InDeltaTime);
		if (!IsInViewport()) return;
	}
	else PhaseTime += InDeltaTime;
	if (!bPaperTransition && Phase == EPhase::Opening)
	{
		const float Alpha = OpenDuration > 0.f ? FMath::Clamp(PhaseTime / OpenDuration, 0.f, 1.f) : 1.f;
		SetRenderOpacity(Alpha);
		if (Alpha >= 1.f)
		{
			Phase = EPhase::Open;
		}
	}
	else if (!bPaperTransition && Phase == EPhase::Closing)
	{
		const float Alpha = CloseDuration > 0.f ? FMath::Clamp(1.f - PhaseTime / CloseDuration, 0.f, 1.f) : 0.f;
		SetRenderOpacity(Alpha);
		if (Alpha <= 0.f)
		{
			FinishClose(false);
			return;
		}
	}

	if (!AllowsPlayerInput(ETAInputCapability::Navigate)) { ActionRotation = FVector2D::ZeroVector; ActionScroll = 0.f; }
	TickTranslation(InDeltaTime);

	// 摇杆旋转：转译面板显示时右摇杆用来滚动正文，不旋转
	const bool bCanNavigate = Phase == EPhase::Open && AllowsPlayerInput(ETAInputCapability::Navigate);
	if (bCanNavigate && !bTranslationVisible && PreviewStage && RotateAction)
	{
		if (AThe_AwakeningPlayerController* PC = InputRequestController.Get())
		{
			const FVector2D Stick = ActionRotation;
			if (!Stick.IsNearlyZero())
			{
				// 摇杆右推 = 模型正面转向右；上推 = 顶部转离镜头（与鼠标向上拖一致）
				PreviewStage->AddManualRotation(-Stick.X * StickRotateSpeed * InDeltaTime, Stick.Y * StickRotateSpeed * InDeltaTime);
			}
		}
	}
	TickScroll(InDeltaTime);
}

void UTADocumentReaderWidget::TickScroll(float DeltaTime)
{
	if (!ScrollBox_Body)
	{
		return;
	}
	// 用户拖了滚动条：以实际位置为准
	const float Actual = ScrollBox_Body->GetScrollOffset();
	if (!FMath::IsNearlyEqual(Actual, LastAppliedScroll, 1.f))
	{
		ScrollTarget = Actual;
	}
	if (bTranslationVisible && Phase == EPhase::Open && ScrollAction && AllowsPlayerInput(ETAInputCapability::Navigate))
	{
		if (AThe_AwakeningPlayerController* PC = InputRequestController.Get())
		{
			ScrollTarget += ActionScroll * HeldScrollSpeed * DeltaTime;
		}
	}
	ClampScrollTarget();
	const float Next = FMath::FInterpTo(Actual, ScrollTarget, DeltaTime, ScrollSmoothing);
	ScrollBox_Body->SetScrollOffset(Next);
	LastAppliedScroll = Next;
}

void UTADocumentReaderWidget::ApplyPaperTravel(float Progress)
{
	if (!Image_Model) return;
	// Shared book capture stays in its exact on-screen rectangle; only the physical document moves.
	const float T = Cast<ATADiaryPreviewStage>(PreviewStage) ? 0.f : FMath::SmoothStep(0.f, 1.f, Progress);
	FWidgetTransform Visual;
	if (bBorrowedStage && !DiaryAbsoluteSize.IsNearlyZero())
	{
		const FGeometry& Target = ReaderTargetGeometry;
		const FVector2D Size = Target.GetLocalSize();
		const FVector2D Center = Size * 0.5f;
		const FVector2D SourceCenter = Target.AbsoluteToLocal(DiaryAbsoluteCenter);
		const FVector2D SourceSize = Target.AbsoluteToLocal(DiaryAbsoluteCenter + DiaryAbsoluteSize * 0.5f)
			- Target.AbsoluteToLocal(DiaryAbsoluteCenter - DiaryAbsoluteSize * 0.5f);
		Visual.Translation = (SourceCenter - Center) * (1.f - T);
		Visual.Scale = FMath::Lerp(SourceSize / Size, FVector2D(1.f), T);
	}
	else
	{
		Visual.Scale = FVector2D(FMath::Lerp(0.75f, 1.f, T));
		Image_Model->SetRenderOpacity(T);
	}
	Image_Model->SetRenderTransform(Visual);
}

void UTADocumentReaderWidget::TickPaperTransition(float DeltaTime)
{
	if (!IsValid(PreviewStage)) { ForceClose(); return; }
	auto* Diary = Cast<ATADiaryPreviewStage>(PreviewStage);
	if (!Document || (!Document->Presentation && !Diary) || !Image_Model) return;
	if (Diary)
	{
		const FGeometry& Geometry = Image_Model->GetCachedGeometry();
		if (Geometry.GetLocalSize().GetMin() > 1.f)
			ReaderTargetGeometry = FGeometry::MakeRoot(Geometry.GetLocalSize(), Geometry.GetAccumulatedLayoutTransform());
		if (UImage* Source = SourceDiaryImage.Get())
		{
			const FGeometry& SourceGeometry = Source->GetCachedGeometry();
			if (SourceGeometry.GetLocalSize().GetMin() <= 1.f) return;
			DiaryAbsoluteCenter = SourceGeometry.LocalToAbsolute(SourceGeometry.GetLocalSize() * 0.5f);
			DiaryAbsoluteSize = SourceGeometry.LocalToAbsolute(SourceGeometry.GetLocalSize()) - SourceGeometry.LocalToAbsolute(FVector2D::ZeroVector);
		}
	}
	if (!bPaperLayoutReady)
	{
		if (Image_Model->GetCachedGeometry().GetLocalSize().GetMin() <= 1.f) return;
		bPaperLayoutReady = true;
		if (!Diary) ReaderTargetGeometry = Image_Model->GetCachedGeometry();
		Image_Model->SetRenderOpacity(1.f);
		if (UImage* Source = SourceDiaryImage.Get()) Source->SetRenderOpacity(0.f);
		ApplyPaperTravel(0.f);
	}
	if (Phase == EPhase::Open)
	{
		if (Diary) ApplyPaperTravel(0.f);
		return;
	}
	PhaseTime += DeltaTime;
	const float Travel = FMath::Max(0.01f, Document->Presentation ? Document->Presentation->TravelDuration : 0.35f);
	const float Fold = FMath::Max(0.001f, Diary ? Diary->GetDocumentFoldDuration() : PreviewStage->GetPaperFoldDuration());
	const float Orient = FMath::Max(0.01f, Document->Presentation ? Document->Presentation->OrientDuration : 0.25f);
	const bool bOpening = Phase == EPhase::Opening;
	// Take the still-folded sheet out and face it towards the camera before unfolding.
	const float Departure = FMath::Max(Travel, Orient);
	const float Total = Departure + Fold + (bOpening ? 0.f : Orient);
	// Returning first removes the player's rotation; only then fold and move back to the book.
	const float PathTime = bOpening ? PhaseTime : Departure + Fold - FMath::Max(0.f, PhaseTime - Orient);
	const float TravelAlpha = FMath::Clamp(PathTime / Travel, 0.f, 1.f);
	const float OpenAlpha = FMath::Clamp((PathTime - Departure) / Fold, 0.f, 1.f);
	const float OrientAlpha = FMath::SmoothStep(0.f, 1.f, PathTime / Orient);
	ApplyPaperTravel(TravelAlpha);
	if (Diary) Diary->UpdateReading(TravelAlpha, OpenAlpha, OrientAlpha);
	else
	{
		PreviewStage->SetPaperOpenness(OpenAlpha);
		FTransform Pose;
		Pose.Blend(bBorrowedStage ? DiaryModelTransform : ReaderModelTransform, ReaderModelTransform, OrientAlpha);
		PreviewStage->SetModelPresentationTransform(Pose);
	}
	PreviewStage->SetManualRotation(bOpening ? FQuat::Identity : FQuat::Slerp(ClosingManualRotation, FQuat::Identity,
		FMath::SmoothStep(0.f, 1.f, PhaseTime / Orient)));
	// Fade the backdrop independently: the travelling sheet must stay visible throughout.
	const float BackdropAlpha = FMath::SmoothStep(0.f, 1.f, PathTime / (Departure + Fold));
	if (UWidget* Dim = GetWidgetFromName(TEXT("Image_Dim"))) Dim->SetRenderOpacity(BackdropAlpha);
	if (auto* Blur = Cast<UBackgroundBlur>(GetWidgetFromName(TEXT("Blur_Background")))) Blur->SetRenderOpacity(BackdropAlpha);
	if (HorizontalBox_Controls) HorizontalBox_Controls->SetRenderOpacity(BackdropAlpha);
	if (PhaseTime >= Total)
	{
		if (bOpening) Phase = EPhase::Open;
		else FinishClose(false);
	}
}

void UTADocumentReaderWidget::RestoreDiaryPresentation()
{
	if (!bBorrowedStage || bDiaryPresentationRestored) return;
	bDiaryPresentationRestored = true;
	if (IsValid(PreviewStage))
	{
		if (auto* Diary = Cast<ATADiaryPreviewStage>(PreviewStage)) Diary->EndReading();
		else
		{
			PreviewStage->SetPaperOpenness(0.f);
			PreviewStage->SetManualRotation(FQuat::Identity);
			PreviewStage->SetModelPresentationTransform(DiaryModelTransform);
		}
	}
	if (UImage* Source = SourceDiaryImage.Get()) Source->SetRenderOpacity(1.f);
}

void UTADocumentReaderWidget::ClampScrollTarget()
{
	ScrollTarget = FMath::Clamp(ScrollTarget, 0.f, FMath::Max(0.f, ScrollBox_Body->GetScrollOffsetOfEnd()));
}

// ==================== 鼠标：滚轮滚动、拖动旋转 ====================

FReply UTADocumentReaderWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ScrollBox_Body && bTranslationVisible && Phase != EPhase::Closing && AllowsPlayerInput(ETAInputCapability::Navigate))
	{
		ScrollTarget -= InMouseEvent.GetWheelDelta() * WheelScrollStep;
		ClampScrollTarget();
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

FReply UTADocumentReaderWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 只在模型画面上按左键才开始拖动；转译面板盖住的部分由面板自己处理（先收到事件）
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Image_Model && PreviewStage
		&& !bTranslationVisible
		&& Phase == EPhase::Open && AllowsPlayerInput(ETAInputCapability::Navigate)
		&& Image_Model->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bDraggingModel = true;
		LastDragPosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UTADocumentReaderWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingModel && PreviewStage && !bTranslationVisible)
	{
		const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
		const FVector2D Delta = Position - LastDragPosition;
		LastDragPosition = Position;
		// 抓着模型转：向右拖 = 正面转向右；向下拖 = 顶部转向镜头（屏幕 Y 向下为正）
		PreviewStage->AddManualRotation(-Delta.X * DragRotateSensitivity, -Delta.Y * DragRotateSensitivity);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTADocumentReaderWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingModel && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDraggingModel = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UTADocumentReaderWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bDraggingModel = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
