#include "UI/Pickup/TAPickupBannerWidget.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TAPreviewStage.h"
#include "Inventory/TAItemDefinition.h"
#include "Core/TALocalizeSubsystem.h"
#include "UI/TAActionPromptWidget.h"
#include "UI/TAPromptWidgetUtils.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	float BannerEase(float Alpha)
	{
		const float A = FMath::Clamp(Alpha, 0.f, 1.f);
		return 1.f - FMath::Square(1.f - A);
	}
}

FString UTAPickupBannerWidget::MakeBodyPreview(const FString& Body, int32 MaxChars)
{
	// 去掉富文本标记（<img id="..."/> 等），合并空白和换行
	FString Plain;
	Plain.Reserve(Body.Len());
	bool bInTag = false;
	bool bLastSpace = false;
	for (const TCHAR Char : Body)
	{
		if (Char == TEXT('<')) { bInTag = true; continue; }
		if (bInTag) { if (Char == TEXT('>')) bInTag = false; continue; }
		if (FChar::IsWhitespace(Char))
		{
			if (!bLastSpace && !Plain.IsEmpty()) Plain.AppendChar(TEXT(' '));
			bLastSpace = true;
			continue;
		}
		Plain.AppendChar(Char);
		bLastSpace = false;
	}
	Plain.TrimEndInline();
	if (Plain.IsEmpty())
	{
		return Plain;
	}
	// 策划文档：“前 20 个字符” + “...”
	return Plain.Left(MaxChars) + TEXT("...");
}

FText UTAPickupBannerWidget::Localize(const FString& TextId) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	return Loc ? Loc->GetText(TextId) : FText::FromString(TextId);
}

void UTAPickupBannerWidget::SetInventoryAction(UInputAction* InAction)
{
	InventoryAction = InAction;
	if (HorizontalBox_Hint)
	{
		HorizontalBox_Hint->ClearChildren();
		FTAPromptWidgetUtils::AddActionPrompt(this, HorizontalBox_Hint, ActionPromptWidgetClass, InventoryAction, ViewDetailTextId);
	}
}

void UTAPickupBannerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddUniqueDynamic(this, &UTAPickupBannerWidget::HandleLanguageChanged);
		}
	}
	if (!PreviewStage)
	{
		PreviewStage = ATAPreviewStage::SpawnPreviewStage(this, nullptr, PreviewStageSlot);
	}
	if (PreviewStage && Image_Model)
	{
		PreviewStage->RenderTargetSize = ModelRenderSize;
		PreviewStage->RotationSpeed = ModelRotationSpeed;
		PreviewStage->SetAutoRotate(true);
		FSlateBrush Brush;
		UObject* Resource = PreviewStage->GetDisplayMaterial();
		Brush.SetResourceObject(Resource ? Resource : PreviewStage->GetRenderTarget());
		Brush.ImageSize = FVector2D(ModelRenderSize);
		Image_Model->SetBrush(Brush);
	}
	if (HorizontalBox_Hint && HorizontalBox_Hint->GetChildrenCount() == 0 && InventoryAction)
	{
		SetInventoryAction(InventoryAction);
	}
	HideImmediately();
}

void UTAPickupBannerWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTAPickupBannerWidget::HandleLanguageChanged);
		}
	}
	if (PreviewStage)
	{
		PreviewStage->Destroy();
		PreviewStage = nullptr;
	}
	Super::NativeDestruct();
}

// ==================== 队列 ====================

void UTAPickupBannerWidget::ShowItem(UTAItemDefinition* Item)
{
	FTAPickupBannerEntry Entry;
	Entry.Item = Item;
	Enqueue(Entry);
}

void UTAPickupBannerWidget::ShowDocument(UTADocumentDefinition* Document)
{
	FTAPickupBannerEntry Entry;
	Entry.Document = Document;
	Enqueue(Entry);
}

void UTAPickupBannerWidget::Enqueue(const FTAPickupBannerEntry& Entry)
{
	if (!Entry.IsValid())
	{
		return;
	}
	Queue.Add(Entry);
	if (Phase == EPhase::Idle)
	{
		StartNext();
	}
	else if (Phase != EPhase::SlideOut && Phase != EPhase::SlideIn)
	{
		// 显示中又拾取：当前这条立刻滑出，下一条随后滑入
		BeginSlideOut();
	}
	// 正在滑入时：滑入完成后检查队列再滑出（见 NativeTick）
}

void UTAPickupBannerWidget::StartNext()
{
	if (Queue.IsEmpty())
	{
		HideImmediately();
		return;
	}
	Current = Queue[0];
	Queue.RemoveAt(0);
	bShowingBody = false;

	if (Text_Name)
	{
		Text_Name->SetRenderOpacity(1.f);
		Text_Name->SetText(Current.Item ? Localize(Current.Item->DisplayName.ToString()) : Localize(Current.Document->TitleTextId));
	}
	if (PreviewStage)
	{
		if (Current.Item)
		{
			PreviewStage->SetDisplayMesh(Current.Item->PreviewMesh.Get(), Current.Item->PreviewTransform);
		}
		else if (UObject* Mesh = Current.Document->Mesh.Get())
		{
			PreviewStage->SetDocument(Current.Document, Current.Document->PickupPreviewTransform, false);
		}
		else
		{
			// 没有模型时用一张正对镜头的纸
			PreviewStage->SetDisplayMesh(nullptr, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.01f, 0.21f, 0.3f)));
		}
		PreviewStage->SetCaptureEnabled(true);
	}
	if (Panel_Banner)
	{
		Panel_Banner->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	Phase = EPhase::SlideIn;
	PhaseTime = 0.f;
	SlideVisible = 0.f;
	ApplySlide(0.f);
}

void UTAPickupBannerWidget::BeginSlideOut()
{
	SlideOutFrom = SlideVisible;
	Phase = EPhase::SlideOut;
	PhaseTime = 0.f;
}

void UTAPickupBannerWidget::HideImmediately()
{
	Phase = EPhase::Idle;
	PhaseTime = 0.f;
	SlideVisible = 0.f;
	Current = FTAPickupBannerEntry();
	ApplySlide(0.f);
	if (Panel_Banner)
	{
		Panel_Banner->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (PreviewStage)
	{
		PreviewStage->SetCaptureEnabled(false);
	}
}

FTAPickupBannerEntry UTAPickupBannerWidget::TakeQuickViewEntry()
{
	if (Phase == EPhase::Idle || Phase == EPhase::SlideOut)
	{
		return FTAPickupBannerEntry();
	}
	const FTAPickupBannerEntry Entry = Current;
	Queue.Reset();
	HideImmediately();
	return Entry;
}

float UTAPickupBannerWidget::GetHoldDuration() const
{
	return Current.Document ? DocumentTextHoldDuration : ItemHoldDuration;
}

void UTAPickupBannerWidget::HandleLanguageChanged()
{
	if (!Text_Name || !Current.IsValid())
	{
		return;
	}
	if (Current.Item)
	{
		Text_Name->SetText(Localize(Current.Item->DisplayName.ToString()));
	}
	else
	{
		Text_Name->SetText(bShowingBody
			? FText::FromString(MakeBodyPreview(Localize(Current.Document->BodyTextId).ToString(), BodyPreviewChars))
			: Localize(Current.Document->TitleTextId));
	}
}

// ==================== 每帧（真实时间） ====================

void UTAPickupBannerWidget::ApplySlide(float Visible)
{
	if (!Panel_Banner)
	{
		return;
	}
	// 从屏幕右侧外滑入：完全隐藏时向右平移一个面板宽度再多一点
	const float Width = Panel_Banner->GetCachedGeometry().GetLocalSize().X;
	const float Offscreen = (Width > 1.f ? Width : 600.f) + 80.f;
	Panel_Banner->SetRenderTranslation(FVector2D((1.f - BannerEase(Visible)) * Offscreen, 0.f));
}

void UTAPickupBannerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Phase == EPhase::Idle)
	{
		return;
	}
	PhaseTime += InDeltaTime;
	switch (Phase)
	{
	case EPhase::SlideIn:
		SlideVisible = SlideInDuration > 0.f ? FMath::Clamp(PhaseTime / SlideInDuration, 0.f, 1.f) : 1.f;
		ApplySlide(SlideVisible);
		if (SlideVisible >= 1.f)
		{
			if (!Queue.IsEmpty())
			{
				BeginSlideOut();
			}
			else
			{
				Phase = EPhase::HoldTitle;
				PhaseTime = 0.f;
			}
		}
		break;
	case EPhase::HoldTitle:
		if (PhaseTime >= GetHoldDuration())
		{
			if (Current.Document)
			{
				Phase = EPhase::SwapText;
				PhaseTime = 0.f;
			}
			else
			{
				BeginSlideOut();
			}
		}
		break;
	case EPhase::SwapText:
	{
		// 前一半淡出标题，切成正文预览，后一半淡入
		const float Half = FMath::Max(DocumentTextSwapDuration * 0.5f, KINDA_SMALL_NUMBER);
		if (!bShowingBody && PhaseTime >= Half)
		{
			bShowingBody = true;
			if (Text_Name)
			{
				Text_Name->SetText(FText::FromString(MakeBodyPreview(Localize(Current.Document->BodyTextId).ToString(), BodyPreviewChars)));
			}
		}
		const float Opacity = PhaseTime < Half ? 1.f - PhaseTime / Half : FMath::Clamp((PhaseTime - Half) / Half, 0.f, 1.f);
		if (Text_Name) Text_Name->SetRenderOpacity(Opacity);
		if (PhaseTime >= Half * 2.f)
		{
			if (Text_Name) Text_Name->SetRenderOpacity(1.f);
			Phase = EPhase::HoldBody;
			PhaseTime = 0.f;
		}
		break;
	}
	case EPhase::HoldBody:
		if (PhaseTime >= DocumentTextHoldDuration)
		{
			BeginSlideOut();
		}
		break;
	case EPhase::SlideOut:
		SlideVisible = SlideOutDuration > 0.f ? SlideOutFrom * FMath::Clamp(1.f - PhaseTime / SlideOutDuration, 0.f, 1.f) : 0.f;
		ApplySlide(SlideVisible);
		if (SlideVisible <= 0.f)
		{
			StartNext();
		}
		break;
	default:
		break;
	}
}
