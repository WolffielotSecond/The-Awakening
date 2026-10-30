#include "UI/Inventory/TAInventoryTabBar.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

class SInventoryTabBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SInventoryTabBar) {} SLATE_END_ARGS()
	void Construct(const FArguments&) { SetCanTick(true); }
	TFunction<void(int32)> OnRequested;
	FSlateFontInfo Font;
	FText Labels[2];
	FText StatusLabels[2];
	FSlateBrush Icons[2];
	FLinearColor Dark = FLinearColor(FColor(0x26, 0x34, 0x3A));
	FLinearColor Light = FLinearColor(FColor(0xB9, 0xD7, 0xD8));
	float Duration = 0.32f;
	int32 SelectedPage = 0;
	float Position = 0.f;
	float Start = 0.f;
	float Elapsed = 0.f;
	bool bAnimating = false;
	int32 PressedPage = INDEX_NONE;

	void Select(int32 Page, bool bAnimate)
	{
		Page = FMath::Clamp(Page, 0, 1);
		if (SelectedPage == Page && bAnimate) return;
		SelectedPage = Page;
		Start = Position;
		Elapsed = 0.f;
		bAnimating = bAnimate && !FMath::IsNearlyEqual(Position, float(Page));
		if (!bAnimating) Position = float(Page);
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	virtual void Tick(const FGeometry&, double, float Delta) override
	{
		if (!bAnimating) return;
		Elapsed += Delta;
		const float Alpha = FMath::Clamp(Elapsed / FMath::Max(Duration, 0.01f), 0.f, 1.f);
		Position = FMath::Lerp(Start, float(SelectedPage), FMath::InterpEaseInOut(0.f, 1.f, Alpha, 3.f));
		bAnimating = Alpha < 1.f;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	static float TabsWidth(const FVector2D& Size) { return Size.X * (0.532f / 0.824f); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1582.f, 80.f); }
	int32 HitPage(const FGeometry& Geometry, const FVector2D& Screen) const
	{
		const FVector2D P = Geometry.AbsoluteToLocal(Screen);
		const FVector2D Size = Geometry.GetLocalSize();
		const float Skew = FMath::Min(Size.Y * 0.58f, TabsWidth(Size) * 0.08f);
		const float Pitch = (TabsWidth(Size) - Skew) * 0.5f;
		const float Left = Skew * (1.f - P.Y / FMath::Max(Size.Y, 1.f));
		if (P.Y < 0 || P.Y > Size.Y || P.X < Left || P.X > Left + Pitch * 2.f) return INDEX_NONE;
		return P.X < Left + Pitch ? 0 : 1;
	}
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
		PressedPage = HitPage(Geometry, Event.GetScreenSpacePosition());
		return PressedPage == INDEX_NONE ? FReply::Unhandled() : FReply::Handled().CaptureMouse(SharedThis(this));
	}
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture()) return FReply::Unhandled();
		const int32 Page = HitPage(Geometry, Event.GetScreenSpacePosition());
		if (Page != INDEX_NONE && Page == PressedPage && OnRequested) OnRequested(Page);
		PressedPage = INDEX_NONE;
		return FReply::Handled().ReleaseMouseCapture();
	}
	virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override { PressedPage = INDEX_NONE; }
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override
	{
		return HitPage(Geometry, Event.GetScreenSpacePosition()) == INDEX_NONE
			? FCursorReply::Unhandled() : FCursorReply::Cursor(EMouseCursor::Hand);
	}
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = G.GetLocalSize();
		if (Size.X <= 0 || Size.Y <= 0) return Layer;
		const float H = Size.Y;
		const float TabWidth = TabsWidth(Size);
		const float Skew = FMath::Min(H * 0.58f, TabWidth * 0.08f);
		const float Pitch = (TabWidth - Skew) * 0.5f;
		const float X = Position * Pitch;
		const FVector2D TL(X + Skew, 0), TR(X + Pitch + Skew, 0), BL(X, H), BR(X + Pitch, H);
		const FLinearColor Tint = Style.GetColorAndOpacityTint();
		const FColor PlateColor = (Light * Tint).ToFColor(true);
		TArray<FSlateVertex> Verts;
		for (const FVector2D& P : {TL, TR, BL, BR})
			Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(), FVector2f(P), FVector2f(0.5f, 0.5f), PlateColor));
		const TArray<SlateIndex> Indices = {0, 1, 2, 2, 1, 3};
		FSlateDrawElement::MakeCustomVerts(Out, Layer, FSlateApplication::Get().GetRenderer()->GetResourceHandle(
			*FCoreStyle::Get().GetBrush("WhiteBrush")), Verts, Indices, nullptr, 0, 0);
		TArray<FVector2D> Line = { FVector2D(0, H), FVector2D(TabWidth - Skew, H), FVector2D(TabWidth, 0) };
		FSlateDrawElement::MakeLines(Out, Layer + 1, G.ToPaintGeometry(), Line, ESlateDrawEffect::None, Light * Tint, true, 1.3f);

		FSlateFontInfo DrawFont = Font;
		DrawFont.Size = FMath::Max(10, FMath::RoundToInt(H * 0.43f));
		auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		auto PaintLabel = [&](int32 Page, FLinearColor Color, uint32 AtLayer)
		{
			const FString Text = Labels[Page].ToString();
			const float Center = Skew * 0.5f + Pitch * (Page + 0.5f);
			// Keep CJK glyphs upright and widely spaced, matching the reference header.
			const bool bCJKPair = Text.Len() == 2 && Text[0] >= 0x2E80 && Text[1] >= 0x2E80;
			if (bCJKPair)
			{
				for (int32 Index = 0; Index < 2; ++Index)
				{
					const FString Glyph = Text.Mid(Index, 1);
					const FVector2D Extent = Measure->Measure(Glyph, DrawFont);
					const FVector2D Offset(Center + (Index ? 1.f : -1.f) * Pitch * 0.21f - Extent.X * 0.5f, (H - Extent.Y) * 0.5f);
					FSlateDrawElement::MakeText(Out, AtLayer, G.ToPaintGeometry(Extent, FSlateLayoutTransform(Offset)), Glyph, DrawFont, ESlateDrawEffect::None, Color * Tint);
				}
			}
			else
			{
				const FVector2D Extent = Measure->Measure(Text, DrawFont);
				FSlateDrawElement::MakeText(Out, AtLayer, G.ToPaintGeometry(Extent, FSlateLayoutTransform(FVector2D(Center - Extent.X * 0.5f, (H - Extent.Y) * 0.5f))), Text, DrawFont, ESlateDrawEffect::None, Color * Tint);
			}
		};
		PaintLabel(0, Light, Layer + 2);
		PaintLabel(1, Light, Layer + 2);
		// A matching four-point clip makes text invert exactly where the slanted plate passes.
		Out.PushClip(FSlateClippingZone(G.LocalToAbsolute(TL), G.LocalToAbsolute(TR), G.LocalToAbsolute(BL), G.LocalToAbsolute(BR)));
		PaintLabel(0, Dark, Layer + 3);
		PaintLabel(1, Dark, Layer + 3);
		Out.PopClip();

		// Only the destination shortcut is visible, inside the inactive tab next to the central seam.
		// Fade through zero during motion so the prompts never overlap the moving plate.
		const float PreviousAlpha = FMath::Clamp((Position - 0.8f) / 0.2f, 0.f, 1.f);
		const float NextAlpha = FMath::Clamp((0.2f - Position) / 0.2f, 0.f, 1.f);
		for (int32 Page = 0; Page < 2; ++Page)
		{
			const FSlateBrush& Icon = Icons[Page];
			const float Alpha = Page == 0 ? PreviousAlpha : NextAlpha;
			if (Alpha <= 0.f || !Icon.GetResourceObject()) continue;
			const float IconH = H * 0.36f;
			const float IconW = IconH * Icon.ImageSize.X / FMath::Max(1.f, Icon.ImageSize.Y);
			const float Seam = Pitch + Skew * 0.5f;
			const float IconX = Page == 0 ? Seam - Skew * 0.7f - IconW : Seam + Skew * 0.7f;
			const FLinearColor Color = Tint.CopyWithNewOpacity(Tint.A * Alpha);
			FSlateDrawElement::MakeBox(Out, Layer + 4, G.ToPaintGeometry(FVector2D(IconW, IconH), FSlateLayoutTransform(FVector2D(IconX, (H - IconH) * 0.5f))), &Icon, ESlateDrawEffect::None, Color);
			const float ArrowLength = H * 0.22f;
			const float ArrowX = Page == 0 ? IconX - H * 0.12f : IconX + IconW + H * 0.12f;
			const float Direction = Page == 0 ? -1.f : 1.f;
			const FVector2D Tip(ArrowX + Direction * ArrowLength, H * 0.5f);
			TArray<FVector2D> Shaft = { FVector2D(ArrowX, H * 0.5f), Tip };
			TArray<FVector2D> Head = { Tip + FVector2D(-Direction * H * 0.075f, -H * 0.075f), Tip, Tip + FVector2D(-Direction * H * 0.075f, H * 0.075f) };
			FSlateDrawElement::MakeLines(Out, Layer + 4, G.ToPaintGeometry(), Shaft, ESlateDrawEffect::None, Light * Color, true, 1.5f);
			FSlateDrawElement::MakeLines(Out, Layer + 4, G.ToPaintGeometry(), Head, ESlateDrawEffect::None, Light * Color, true, 1.5f);
		}
		const float Gap = Size.X * 0.02f;
		const float WidthUnit = (Size.X - TabWidth - 2.f * Gap) / 5.f;
		const float CellWidth = WidthUnit * 2.f;
		for (int32 Status = 0; Status < 2; ++Status)
		{
			const float Left = Status == 0 ? TabWidth + Gap : TabWidth + 2.f * Gap + CellWidth;
			const float Width = WidthUnit * (Status == 0 ? 2.f : 3.f);
			TArray<FSlateVertex> StatusVerts;
			for (const FVector2D& P : {FVector2D(Left + Skew, 0), FVector2D(Left + Width, 0), FVector2D(Left, H), FVector2D(Left + Width - Skew, H)})
				StatusVerts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(), FVector2f(P), FVector2f(0.5f, 0.5f), (Dark * Tint).ToFColor(true)));
			FSlateDrawElement::MakeCustomVerts(Out, Layer + 1, FSlateApplication::Get().GetRenderer()->GetResourceHandle(
				*FCoreStyle::Get().GetBrush("WhiteBrush")), StatusVerts, Indices, nullptr, 0, 0);
			FSlateFontInfo StatusFont = Font;
			StatusFont.Size = FMath::Max(10, FMath::RoundToInt(H * 0.32f));
			FVector2D Extent = Measure->Measure(StatusLabels[Status], StatusFont);
			const float AvailableWidth = FMath::Max(1.f, Width - Skew - H * 0.15f);
			if (Extent.X > AvailableWidth)
			{
				StatusFont.Size = FMath::Max(1, FMath::FloorToInt(StatusFont.Size * AvailableWidth / Extent.X));
				Extent = Measure->Measure(StatusLabels[Status], StatusFont);
			}
			FSlateDrawElement::MakeText(Out, Layer + 2, G.ToPaintGeometry(Extent,
				FSlateLayoutTransform(FVector2D(Left + (Width - Extent.X) * 0.5f, (H - Extent.Y) * 0.5f))),
				StatusLabels[Status], StatusFont, ESlateDrawEffect::None, Light * Tint);
		}
		return Layer + 4;
	}
};

UTAInventoryTabBar::UTAInventoryTabBar(const FObjectInitializer& Initializer) : Super(Initializer)
{
	InventoryLabel = FText::FromString(TEXT("背包"));
	SkillsLabel = FText::FromString(TEXT("技能"));
	CellsLabel = FText::FromString(TEXT("细胞: 0"));
	HealthLabel = FText::FromString(TEXT("HP: —"));
	LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 32);
	LabelFont.FontObject = LoadObject<UFont>(nullptr, TEXT("/Game/Fonts/TA_GlobalFont.TA_GlobalFont"));
}

TSharedRef<SWidget> UTAInventoryTabBar::RebuildWidget()
{
	SlateTabBar = SNew(SInventoryTabBar);
	SlateTabBar->OnRequested = [this](int32 Page) { RequestPage(Page); };
	SynchronizeProperties();
	return SlateTabBar.ToSharedRef();
}

void UTAInventoryTabBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!SlateTabBar) return;
	SlateTabBar->Font = LabelFont;
	SlateTabBar->Dark = DarkColor;
	SlateTabBar->Light = LightColor;
	SlateTabBar->Labels[0] = InventoryLabel;
	SlateTabBar->Labels[1] = SkillsLabel;
	SlateTabBar->StatusLabels[0] = CellsLabel;
	SlateTabBar->StatusLabels[1] = HealthLabel;
	SlateTabBar->Icons[0] = PreviousIconBrush;
	SlateTabBar->Icons[1] = NextIconBrush;
	SlateTabBar->Duration = TransitionDuration;
	SlateTabBar->Select(SelectedPage, false);
	SlateTabBar->Invalidate(EInvalidateWidgetReason::Paint);
}

void UTAInventoryTabBar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SlateTabBar.Reset();
}

void UTAInventoryTabBar::RequestPage(int32 Page) { OnTabRequested.Broadcast(Page); }

void UTAInventoryTabBar::SetPage(int32 Page, bool bAnimate)
{
	SelectedPage = FMath::Clamp(Page, 0, 1);
	if (SlateTabBar) SlateTabBar->Select(SelectedPage, bAnimate);
}

void UTAInventoryTabBar::SetLabels(const FText& Inventory, const FText& Skills, const FSlateFontInfo& Font)
{
	InventoryLabel = Inventory; SkillsLabel = Skills; LabelFont = Font;
	if (SlateTabBar)
	{
		SlateTabBar->Font = LabelFont;
		SlateTabBar->Labels[0] = InventoryLabel;
		SlateTabBar->Labels[1] = SkillsLabel;
		SlateTabBar->Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void UTAInventoryTabBar::UpdateIconBrush(UTexture2D* Texture, TObjectPtr<UMaterialInstanceDynamic>& Material, FSlateBrush& Brush)
{
	Brush = FSlateBrush();
	if (!Texture) { Material = nullptr; return; }
	if (!InvertIconMaterial)
		InvertIconMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UI/Inventory/M_InventoryTabIcon.M_InventoryTabIcon"));
	if (!Material && InvertIconMaterial) Material = UMaterialInstanceDynamic::Create(InvertIconMaterial, this);
	if (Material)
	{
		Material->SetTextureParameterValue(TEXT("IconTexture"), Texture);
		Material->SetVectorParameterValue(TEXT("DarkColor"), DarkColor);
		Material->SetVectorParameterValue(TEXT("LightColor"), LightColor);
		Brush.SetResourceObject(Material);
	}
	else Brush.SetResourceObject(Texture);
	Brush.ImageSize = FVector2D(FMath::Max(1, Texture->GetSizeX()), FMath::Max(1, Texture->GetSizeY()));
}

void UTAInventoryTabBar::SetInputIcons(UTexture2D* Previous, UTexture2D* Next)
{
	UpdateIconBrush(Previous, PreviousIconMaterial, PreviousIconBrush);
	UpdateIconBrush(Next, NextIconMaterial, NextIconBrush);
	if (SlateTabBar)
	{
		SlateTabBar->Icons[0] = PreviousIconBrush;
		SlateTabBar->Icons[1] = NextIconBrush;
		SlateTabBar->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UTAInventoryTabBar::SetStatusLabels(const FText& Cells, const FText& Health)
{
	CellsLabel = Cells; HealthLabel = Health;
	if (SlateTabBar)
	{
		SlateTabBar->StatusLabels[0] = CellsLabel;
		SlateTabBar->StatusLabels[1] = HealthLabel;
		SlateTabBar->Invalidate(EInvalidateWidgetReason::Paint);
	}
}
