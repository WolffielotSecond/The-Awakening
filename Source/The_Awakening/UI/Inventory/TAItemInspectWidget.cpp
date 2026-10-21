#include "UI/Inventory/TAItemInspectWidget.h"
#include "Document/TAPreviewStage.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TAInputIconSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "The_AwakeningPlayerController.h"
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
	constexpr int32 InspectZOrder = 75;
}

UTAItemInspectWidget* UTAItemInspectWidget::Open(APlayerController* PC, TSubclassOf<UTAItemInspectWidget> WidgetClass, UTAItemDefinition* InItem)
{
	UClass* Class = WidgetClass ? WidgetClass.Get() : LoadClass<UTAItemInspectWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ItemInspect.WBP_ItemInspect_C"));
	if (!PC || !InItem || !Class)
	{
		UE_LOG(LogTemp, Warning, TEXT("Item inspect: %s."), !Class ? TEXT("WBP_ItemInspect missing; run -run=TADocumentUI") : TEXT("no item or player controller"));
		return nullptr;
	}
	UTAItemInspectWidget* Widget = CreateWidget<UTAItemInspectWidget>(PC, Class);
	if (!Widget)
	{
		return nullptr;
	}
	Widget->Item = InItem;
	Widget->AddToViewport(InspectZOrder);
	return Widget;
}

void UTAItemInspectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!RotateAction) RotateAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/UI/IA_DocumentRotate.IA_DocumentRotate"));
	if (!BackAction) BackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_UIBack.IA_UIBack"));

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddUniqueDynamic(this, &UTAItemInspectWidget::HandleLanguageChanged);
		}
		if (UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			Icons->OnInputPromptsChanged.AddUniqueDynamic(this, &UTAItemInspectWidget::HandleInputPresentationChanged);
			bGamepadDevice = Icons->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
		}
	}

	// 3D 模型：物品定义的 PreviewMesh，没有时显示占位方块
	PreviewStage = ATAPreviewStage::SpawnPreviewStage(this, nullptr, PreviewStageSlot);
	if (PreviewStage && Image_Model)
	{
		PreviewStage->RenderTargetSize = ModelRenderSize;
		PreviewStage->SetAutoRotate(false);
		PreviewStage->SetDisplayMesh(Item ? Item->PreviewMesh.Get() : nullptr, Item ? Item->PreviewTransform : FTransform::Identity);
		PreviewStage->SetCaptureEnabled(true);
		FSlateBrush Brush;
		UObject* Resource = PreviewStage->GetDisplayMaterial();
		Brush.SetResourceObject(Resource ? Resource : PreviewStage->GetRenderTarget());
		Brush.ImageSize = FVector2D(ModelRenderSize);
		Image_Model->SetBrush(Brush);
	}

	if (HorizontalBox_Controls)
	{
		HorizontalBox_Controls->ClearChildren();
		RotatePrompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, RotateAction, RotateTextId, FLinearColor::White);
		BackPrompt = FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Controls, ActionPromptWidgetClass, BackAction, BackTextId, FLinearColor::White);
		if (BackPrompt)
		{
			BackPrompt->OnPromptClicked.AddDynamic(this, &UTAItemInspectWidget::HandlePromptClicked);
		}
	}
	HandleInputPresentationChanged();
	RefreshText();

	Phase = EPhase::Opening;
	PhaseTime = 0.f;
	SetRenderOpacity(OpenDuration > 0.f ? 0.f : 1.f);
	SetIsFocusable(true);
	if (auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		FTAInputRequest Request;
		Request.Owner = this;
		Request.Priority = InputPriority;
		Request.Allowed = {ETAInputCapability::Navigate, ETAInputCapability::Cursor, ETAInputCapability::Close};
		Request.Presentation.InputMode = ETAInputModeRequirement::GameAndUI;
		Request.Presentation.bShowCursor = true;
		Request.Presentation.Focus = ETAInputFocusRequirement::Target;
		Request.Presentation.FocusTarget = this;
		InputRequestController = PC;
		InputRequestHandle = PC->AcquireInputRequest(Request);
	}
}

void UTAItemInspectWidget::NativeDestruct()
{
	ReleaseInput();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTAItemInspectWidget::HandleLanguageChanged);
		}
		if (UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			Icons->OnInputPromptsChanged.RemoveDynamic(this, &UTAItemInspectWidget::HandleInputPresentationChanged);
		}
	}
	if (PreviewStage)
	{
		PreviewStage->Destroy();
		PreviewStage = nullptr;
	}
	Super::NativeDestruct();
}

void UTAItemInspectWidget::RefreshText()
{
	const UGameInstance* GI = GetGameInstance();
	const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	// 物品的名称 / 介绍字段存的是本地化 ID
	auto Localize = [Loc](const FText& Id) { return Loc ? Loc->GetText(Id.ToString()) : Id; };
	if (Text_Name) Text_Name->SetText(Item ? Localize(Item->DisplayName) : FText::GetEmpty());
	if (Text_Description) Text_Description->SetText(Item ? Localize(Item->Description) : FText::GetEmpty());
}

void UTAItemInspectWidget::HandleLanguageChanged()
{
	RefreshText();
}

void UTAItemInspectWidget::HandleInputPresentationChanged()
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UTAInputIconSubsystem* Icons = GI->GetSubsystem<UTAInputIconSubsystem>())
		{
			bGamepadDevice = Icons->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
		}
	}
	// 与其他界面一致：只用于手柄的“旋转”提示在键鼠下收起（键鼠直接拖动模型）
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

void UTAItemInspectWidget::HandlePromptClicked(UTAActionPromptWidget* Prompt)
{
	if (Prompt == BackPrompt && AllowsPlayerInput(ETAInputCapability::Close))
	{
		RequestClose();
	}
}

bool UTAItemInspectWidget::HandleMenuBackRequested()
{
	if (!AllowsPlayerInput(ETAInputCapability::Close))
	{
		return false;
	}
	// 动画没播完之前返回无效，但要吞掉，不能漏给背包
	RequestClose();
	return true;
}

void UTAItemInspectWidget::RequestClose()
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
		RemoveFromParent();
	}
}

void UTAItemInspectWidget::RemoveFromParent()
{
	ReleaseInput();
	Super::RemoveFromParent();
}

void UTAItemInspectWidget::ReleaseInput()
{
	const auto Handle = InputRequestHandle;
	InputRequestHandle = 0;
	const auto Issuer = InputRequestController;
	InputRequestController.Reset();
	if (Handle)
		if (auto* PC = Issuer.Get()) PC->ReleaseInputRequest(Handle);
}

bool UTAItemInspectWidget::AllowsPlayerInput(ETAInputCapability Capability) const
{
	const auto* PC = InputRequestController.Get();
	return PC && PC->AllowsInputFor(InputRequestHandle, this, Capability);
}

void UTAItemInspectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PhaseTime += InDeltaTime;
	if (Phase == EPhase::Opening)
	{
		const float Alpha = OpenDuration > 0.f ? FMath::Clamp(PhaseTime / OpenDuration, 0.f, 1.f) : 1.f;
		SetRenderOpacity(Alpha);
		if (Alpha >= 1.f) Phase = EPhase::Open;
	}
	else if (Phase == EPhase::Closing)
	{
		const float Alpha = CloseDuration > 0.f ? FMath::Clamp(1.f - PhaseTime / CloseDuration, 0.f, 1.f) : 0.f;
		SetRenderOpacity(Alpha);
		if (Alpha <= 0.f)
		{
			RemoveFromParent();
			return;
		}
	}
	// 摇杆旋转（方向与鼠标拖动一致）
	if (Phase != EPhase::Closing && PreviewStage && RotateAction && AllowsPlayerInput(ETAInputCapability::Navigate))
	{
		if (AThe_AwakeningPlayerController* PC = InputRequestController.Get())
		{
			const FVector2D Stick = PC->ReadHeldAction(RotateAction).Get<FVector2D>();
			if (!Stick.IsNearlyZero())
			{
				PreviewStage->AddManualRotation(-Stick.X * StickRotateSpeed * InDeltaTime, Stick.Y * StickRotateSpeed * InDeltaTime);
			}
		}
	}
}

FReply UTAItemInspectWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Image_Model && PreviewStage
		&& Phase == EPhase::Open && AllowsPlayerInput(ETAInputCapability::Navigate)
		&& Image_Model->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bDraggingModel = true;
		LastDragPosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	// 检视铺满全屏：其他点击不穿透到背包
	return FReply::Handled();
}

FReply UTAItemInspectWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingModel && PreviewStage)
	{
		const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
		const FVector2D Delta = Position - LastDragPosition;
		LastDragPosition = Position;
		// 抓着模型转：向右拖 = 正面转向右；向下拖 = 顶部转向镜头
		PreviewStage->AddManualRotation(-Delta.X * DragRotateSensitivity, -Delta.Y * DragRotateSensitivity);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTAItemInspectWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDraggingModel && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDraggingModel = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UTAItemInspectWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bDraggingModel = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
