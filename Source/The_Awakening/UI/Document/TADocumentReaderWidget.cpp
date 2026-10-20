#include "UI/Document/TADocumentReaderWidget.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentSettings.h"
#include "Document/TAPreviewStage.h"
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
	UTADocumentDefinition* InDocument, int32 InInputPriority, bool bInFreezeTime, TFunction<void(bool)> InOnClosed)
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
	if (Panel_Translation)
	{
		Panel_Translation->SetRenderOpacity(0.f);
		Panel_Translation->SetVisibility(ESlateVisibility::Collapsed);
	}
	ScrollTarget = LastAppliedScroll = 0.f;
	Phase = EPhase::Opening;
	PhaseTime = 0.f;
	SetRenderOpacity(OpenDuration > 0.f ? 0.f : 1.f);
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
	if (PreviewStage)
	{
		PreviewStage->Destroy();
		PreviewStage = nullptr;
	}
	Super::NativeDestruct();
}

void UTADocumentReaderWidget::SetupModel()
{
	if (!Image_Model || PreviewStage)
	{
		return;
	}
	PreviewStage = ATAPreviewStage::SpawnPreviewStage(this, nullptr, PreviewStageSlot);
	if (!PreviewStage)
	{
		return;
	}
	PreviewStage->RenderTargetSize = ModelRenderSize;
	PreviewStage->SetAutoRotate(false);
	if (UObject* Mesh = Document ? Document->Mesh.Get() : nullptr)
	{
		// 静态 / 骨骼网格体由预览台按类型显示
		PreviewStage->SetDisplayModel(Mesh, Document->ReaderTransform);
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
}

void UTADocumentReaderWidget::BuildControls()
{
	if (!HorizontalBox_Controls)
	{
		return;
	}
	HorizontalBox_Controls->ClearChildren();
	TranslatePrompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, TranslateAction, ShowTranslationTextId);
	if (TranslatePrompt)
	{
		TranslatePrompt->OnPromptClicked.AddDynamic(this, &UTADocumentReaderWidget::HandlePromptClicked);
	}
	// 旋转只是提示，不可点击（没有绑定点击事件）
	RotatePrompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, RotateAction, RotateTextId);
	BackPrompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, BackAction, BackTextId);
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

void UTADocumentReaderWidget::ExecutePlayerInput(FKey Key, ETAInputCapability Capability)
{
	if (Capability == ETAInputCapability::Confirm)
	{
		ToggleTranslation();
	}
}

// ==================== 打开 / 关闭 / 转译 ====================

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
	if (CloseDuration <= 0.f)
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
	if (bTranslationVisible && Panel_Translation)
	{
		Panel_Translation->SetVisibility(ESlateVisibility::Visible);
	}
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
	PhaseTime += InDeltaTime;
	if (Phase == EPhase::Opening)
	{
		const float Alpha = OpenDuration > 0.f ? FMath::Clamp(PhaseTime / OpenDuration, 0.f, 1.f) : 1.f;
		SetRenderOpacity(Alpha);
		if (Alpha >= 1.f)
		{
			Phase = EPhase::Open;
		}
	}
	else if (Phase == EPhase::Closing)
	{
		const float Alpha = CloseDuration > 0.f ? FMath::Clamp(1.f - PhaseTime / CloseDuration, 0.f, 1.f) : 0.f;
		SetRenderOpacity(Alpha);
		if (Alpha <= 0.f)
		{
			FinishClose(false);
			return;
		}
	}

	// 转译面板淡入淡出，完全淡出后收起，不再挡住鼠标
	if (Panel_Translation)
	{
		const float Step = TranslationFadeDuration > 0.f ? InDeltaTime / TranslationFadeDuration : 1.f;
		TranslationAlpha = FMath::Clamp(TranslationAlpha + (bTranslationVisible ? Step : -Step), 0.f, 1.f);
		Panel_Translation->SetRenderOpacity(TranslationAlpha);
		if (!bTranslationVisible && TranslationAlpha <= 0.f && Panel_Translation->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Panel_Translation->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 摇杆旋转：转译面板显示时右摇杆用来滚动正文，不旋转
	const bool bCanNavigate = Phase != EPhase::Closing && AllowsPlayerInput(ETAInputCapability::Navigate);
	if (bCanNavigate && !bTranslationVisible && PreviewStage && RotateAction)
	{
		if (AThe_AwakeningPlayerController* PC = InputRequestController.Get())
		{
			const FVector2D Stick = PC->ReadHeldAction(RotateAction).Get<FVector2D>();
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
	if (bTranslationVisible && Phase != EPhase::Closing && ScrollAction && AllowsPlayerInput(ETAInputCapability::Navigate))
	{
		if (AThe_AwakeningPlayerController* PC = InputRequestController.Get())
		{
			ScrollTarget += PC->ReadHeldAction(ScrollAction).Get<float>() * HeldScrollSpeed * DeltaTime;
		}
	}
	ClampScrollTarget();
	const float Next = FMath::FInterpTo(Actual, ScrollTarget, DeltaTime, ScrollSmoothing);
	ScrollBox_Body->SetScrollOffset(Next);
	LastAppliedScroll = Next;
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
	if (bDraggingModel && PreviewStage)
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
