// Source/The_Awakening/Story/TAPortraitWidget.cpp
#include "Story/TAPortraitWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"

void UTAPortraitWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetRenderOpacity(1.f);

	// 锚点：底部中心（position 是底部中心锚点坐标）
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
	}

	ScheduleNextBlink();
}

void UTAPortraitWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// ---- 待应用的目标（需要父级画布布局完成后才有有效尺寸） ----
	if (bPendingTarget)
	{
		UWidget* Parent = GetParent();
		if (Parent)
		{
			const FVector2D ViewportSize = Parent->GetCachedGeometry().GetLocalSize();
			if (ViewportSize.GetMax() > 1.f)
			{
				ResolvePixelTargets(ViewportSize);
				bPendingTarget = false;
			}
		}
	}

	// ---- 眨眼 ----
	if (bBlinking)
	{
		BlinkStateTimer -= InDeltaTime;
		if (BlinkStateTimer <= 0.f)
		{
			if (bSecondBlinkPending)
			{
				bSecondBlinkPending = false;
				BlinkStateTimer = BlinkClosedDuration; // 双眨的第二次闭眼
			}
			else
			{
				bBlinking = false;
				SetEyesClosed(false);
				ScheduleNextBlink();
			}
		}
	}
	else
	{
		BlinkTimer -= InDeltaTime;
		if (BlinkTimer <= 0.f)
		{
			bBlinking = true;
			BlinkStateTimer = BlinkClosedDuration;
			bSecondBlinkPending = FMath::FRand() < DoubleBlinkChance;
			SetEyesClosed(true);
		}
	}

	// ---- 说话动嘴（只有 SetTalking(true) 的角色） ----
	if (bTalking)
	{
		MouthTimer -= InDeltaTime;
		if (MouthTimer <= 0.f)
		{
			MouthTimer = MouthToggleInterval;
			SetMouthOpen(!bMouthOpen);
		}
	}
	else if (bMouthOpen)
	{
		SetMouthOpen(false);
	}

	// ---- 位置/尺寸补间 ----
	if (bHasTarget && TweenProgress < 1.f)
	{
		TweenProgress = FMath::Min(1.f, TweenProgress + InDeltaTime / FMath::Max(MoveDuration, 0.001f));
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			CanvasSlot->SetPosition(FMath::Lerp(StartPositionPx, TargetPositionPx, TweenProgress));
			CanvasSlot->SetSize(FMath::Lerp(StartSizePx, TargetSizePx, TweenProgress));
		}
		if (TweenProgress >= 1.f)
		{
			bHasTarget = false;
		}
	}

	// ---- 淡出 ----
	if (bFadingOut)
	{
		FadeProgress += InDeltaTime / FMath::Max(FadeDuration, 0.001f);
		SetRenderOpacity(FMath::Clamp(1.f - FadeProgress, 0.f, 1.f));
		if (FadeProgress >= 1.f)
		{
			RemoveFromParent();
		}
	}
}

void UTAPortraitWidget::ApplyPortrait(const FTAPortraitEntry& Entry)
{
	Current = Entry;
	UpdateLayerTextures();
	bPendingTarget = true;
}

void UTAPortraitWidget::FadeOutAndRemove()
{
	if (!bFadingOut)
	{
		bFadingOut = true;
		FadeProgress = 0.f;
	}
}

UTexture2D* UTAPortraitWidget::LoadTexture(const FString& Path)
{
	if (Path.IsEmpty())
	{
		return nullptr;
	}

	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Path);
	if (!Tex)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dialogue] 立绘贴图加载失败: %s"), *Path);
	}
	return Tex;
}

void UTAPortraitWidget::UpdateLayerTextures()
{
	if (LastBasePath != Current.Base)
	{
		TexBase = LoadTexture(Current.Base);
		LastBasePath = Current.Base;
	}
	if (LastEyesOpenPath != Current.EyesOpen)
	{
		TexEyesOpen = LoadTexture(Current.EyesOpen);
		LastEyesOpenPath = Current.EyesOpen;
	}
	if (LastEyesClosedPath != Current.EyesClosed)
	{
		TexEyesClosed = LoadTexture(Current.EyesClosed);
		LastEyesClosedPath = Current.EyesClosed;
	}
	if (LastMouthOpenPath != Current.MouthOpen)
	{
		TexMouthOpen = LoadTexture(Current.MouthOpen);
		LastMouthOpenPath = Current.MouthOpen;
	}
	if (LastMouthClosedPath != Current.MouthClosed)
	{
		TexMouthClosed = LoadTexture(Current.MouthClosed);
		LastMouthClosedPath = Current.MouthClosed;
	}

	if (Image_Base)
	{
		Image_Base->SetBrushFromTexture(TexBase);
	}
	SetEyesClosed(bEyesClosed);
	SetMouthOpen(bMouthOpen);
}

void UTAPortraitWidget::ScheduleNextBlink()
{
	BlinkTimer = FMath::FRandRange(BlinkMinInterval, BlinkMaxInterval);
}

void UTAPortraitWidget::SetEyesClosed(bool bClosed)
{
	bEyesClosed = bClosed;
	if (Image_Eyes)
	{
		Image_Eyes->SetBrushFromTexture(bClosed ? TexEyesClosed : TexEyesOpen);
	}
}

void UTAPortraitWidget::SetMouthOpen(bool bOpen)
{
	bMouthOpen = bOpen;
	if (Image_Mouth)
	{
		Image_Mouth->SetBrushFromTexture(bOpen ? TexMouthOpen : TexMouthClosed);
	}
}

void UTAPortraitWidget::ResolvePixelTargets(const FVector2D& ViewportSize)
{
	const float Height = ViewportSize.Y * BaseHeightRatio * FMath::Max(Current.Scale, 0.01f);

	float Aspect = 0.6f;
	if (TexBase && TexBase->GetSurfaceHeight() > 0)
	{
		Aspect = (float)TexBase->GetSurfaceWidth() / (float)TexBase->GetSurfaceHeight();
	}
	const float Width = Height * Aspect;

	// 锚点 = 底部中心
	const FVector2D NewPosition(
		Current.Position.X * ViewportSize.X,
		ViewportSize.Y * (1.f - Current.Position.Y));
	const FVector2D NewSize(Width, Height);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
	{
		if (!bHasTarget)
		{
			StartPositionPx = CanvasSlot->GetPosition();
			StartSizePx = CanvasSlot->GetSize();
			TweenProgress = 0.f;
		}
		TargetPositionPx = NewPosition;
		TargetSizePx = NewSize;
		bHasTarget = true;
	}
}
