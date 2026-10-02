#include "Puzzle/TAPathPuzzleWidget.h"
#include "Puzzle/TAPuzzleScopeLayout.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Puzzle/TAPathPuzzleNodeWidget.h"
#include "Puzzle/TAPathPuzzleRules.h"
#include "Puzzle/TAPuzzleRewardReceiver.h"
#include "Puzzle/TAPuzzleRewards.h"
#include "The_AwakeningPlayerController.h"
#include "Core/TAFreezeSubsystem.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Brushes/SlateColorBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Core/TAInputIconSubsystem.h"
#include "Core/TALocalizeSubsystem.h"
#include "UI/TAPromptWidgetUtils.h"
#include "Engine/GameInstance.h"

FTAPuzzleAppearance::FTAPuzzleAppearance()
{
	NodeNormal = FSlateColorBrush(FLinearColor(.82f, .85f, .9f));
	NodeEndpoint = FSlateColorBrush(FLinearColor(1.f, .85f, .3f));
	NodeSelected = FSlateColorBrush(FLinearColor(.25f, .8f, .35f));
	NodeFailed = FSlateColorBrush(FLinearColor(.95f, .2f, .2f));
	EdgeNormal = FSlateColorBrush(FLinearColor(.45f, .48f, .55f));
	EdgeSelected = FSlateColorBrush(FLinearColor(.15f, .9f, .25f));
	EdgeFailed = FSlateColorBrush(FLinearColor(1.f, .15f, .15f));
}

UTAPathPuzzleWidget* UTAPathPuzzleWidget::OpenPuzzle(APlayerController* Player,
	TSubclassOf<UTAPathPuzzleWidget> WidgetClass, UObject* Receiver, int32 Seed)
{
	return OpenPuzzleInternal(Player, WidgetClass, nullptr, Receiver, Seed);
}

UTAPathPuzzleWidget* UTAPathPuzzleWidget::OpenPuzzleWithSettings(APlayerController* Player,
	TSubclassOf<UTAPathPuzzleWidget> WidgetClass, const FTAPuzzleSettings& Settings, UObject* Receiver, int32 Seed)
{
	return OpenPuzzleInternal(Player, WidgetClass, &Settings, Receiver, Seed);
}

UTAPathPuzzleWidget* UTAPathPuzzleWidget::OpenPuzzleInternal(APlayerController* Player,
	TSubclassOf<UTAPathPuzzleWidget> WidgetClass, const FTAPuzzleSettings* Settings, UObject* Receiver, int32 Seed)
{
	if (!Player || !Player->IsLocalController()) return nullptr;
	UTAPathPuzzleWidget* Widget = CreateWidget<UTAPathPuzzleWidget>(Player, WidgetClass ? WidgetClass.Get() : StaticClass());
	if (!Widget) return nullptr;
	// Apply before generation / session initialization; changing these after StartPuzzle is too late.
	if (Settings) Widget->PuzzleSettings = *Settings;
	Widget->bAutoStart = false; Widget->RewardReceiver = Receiver; Widget->PuzzleSeed = Seed;
	if (!Widget->StartPuzzle()) return nullptr;
	Widget->AddToViewport(80);
	return Widget;
}

void UTAPathPuzzleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildFallback();
	// These labels sit under the magnified instrument canvas. Supersample once
	// per instance, preserving the Designer font, position, alignment and bounds.
	for (const FName Name : {FName(TEXT("Text_EnergyLabel")), FName(TEXT("Text_TimeLabel"))})
	{
		if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(Name)))
			if (auto* LabelSlot = Cast<UCanvasPanelSlot>(Label->Slot))
			{
				if (LabelSlot->GetAnchors().Minimum != LabelSlot->GetAnchors().Maximum) continue;
				const FVector2D OldSize = LabelSlot->GetSize();
				const FVector2D Alignment = LabelSlot->GetAlignment();
				const FVector2D Pivot = Label->GetRenderTransformPivot();
				FSlateFontInfo Font = Label->GetFont(); Font.Size *= 4; Label->SetFont(Font);
				LabelSlot->SetSize(OldSize * 4);
				LabelSlot->SetPosition(LabelSlot->GetPosition() + (Alignment - Pivot) * OldSize * 3);
				Label->SetRenderScale(Label->GetRenderTransform().Scale * .25f);
			}
	}
	if (Button_Undo) Button_Undo->OnClicked.AddUniqueDynamic(this, &UTAPathPuzzleWidget::Undo);
	// Retry/reset is intentionally disabled for the microscope version for now.
}

FReply UTAPathPuzzleWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (!InKeyEvent.IsRepeat()) ConfirmScopeNode();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
void UTAPathPuzzleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (auto* GI = GetGameInstance())
	{
		GI->GetSubsystem<UTAInputIconSubsystem>()->OnInputDeviceChanged.AddUniqueDynamic(this, &UTAPathPuzzleWidget::RefreshLocalizedPrompts);
		GI->GetSubsystem<UTALocalizeSubsystem>()->OnLanguageChanged.AddUniqueDynamic(this, &UTAPathPuzzleWidget::RefreshLocalizedPrompts);
	}
	RefreshLocalizedPrompts();
	// SelfHitTestInvisible skips this screen in Slate's cursor query. Make the
	// fullscreen widget a hit target so UI input mode cannot fall back to Arrow.
	SetVisibility(ESlateVisibility::Visible);
	SetCursor(EMouseCursor::None);
	if (bAutoStart && !Session) StartPuzzle();
	// Freeze registered gameplay actors, not world time or the UMG countdown.
	// Keep the freeze through settlement until this screen is removed.
	if (Session)
	{
		if (UTAFreezeSubsystem* Freeze = GetWorld()->GetSubsystem<UTAFreezeSubsystem>())
			Freeze->RequestFreeze(this);
	}
	if (Session && !Session->IsTerminal() && !bOwnsInputMode)
	{
		if (AThe_AwakeningPlayerController* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
		{
			PC->BeginUIInputMode(this); PC->SetPuzzleScopeWidget(this); bOwnsInputMode = true;
		}
	}
	RefreshChrome();
}

FCursorReply UTAPathPuzzleWidget::NativeOnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return FCursorReply::Cursor(EMouseCursor::None);
}

void UTAPathPuzzleWidget::ReleaseInput()
{
	if (!bOwnsInputMode) return;
	bOwnsInputMode = false;
	if (auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
	{
		PC->SetPuzzleScopeWidget(nullptr);
		PC->EndUIInputMode();
	}
}

void UTAPathPuzzleWidget::NativeDestruct()
{
	if (auto* GI = GetGameInstance())
	{
		GI->GetSubsystem<UTAInputIconSubsystem>()->OnInputDeviceChanged.RemoveDynamic(this, &UTAPathPuzzleWidget::RefreshLocalizedPrompts);
		GI->GetSubsystem<UTALocalizeSubsystem>()->OnLanguageChanged.RemoveDynamic(this, &UTAPathPuzzleWidget::RefreshLocalizedPrompts);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettlementCloseTimer);
		if (UTAFreezeSubsystem* Freeze = World->GetSubsystem<UTAFreezeSubsystem>())
			Freeze->ReleaseFreeze(this);
	}
	const bool bAborting = Session && !Session->IsTerminal();
	if (Session)
	{
		Session->OnChanged.RemoveDynamic(this, &UTAPathPuzzleWidget::HandleChanged);
		Session->OnSettled.RemoveDynamic(this, &UTAPathPuzzleWidget::HandleSettled);
		Session->Abort();
	}
	ReleaseInput();
	if (bAborting) OnAborted.Broadcast();
	Super::NativeDestruct();
}

void UTAPathPuzzleWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	if (Session) Session->AdvanceTime(DeltaSeconds);
	if (Session)
	{
		const auto& P = Session->Progress;
		const float Target = P.MaxEnergy > 0 ? FMath::Clamp(float(P.MaxEnergy - P.EnergyUsed) / P.MaxEnergy, 0.f, 1.f) : 0.f;
		DisplayEnergyFill = DisplayEnergyFill < 0.f ? Target : FMath::FInterpTo(DisplayEnergyFill, Target, DeltaSeconds, EnergyBlendSpeed);
	}
	UpdateScope(DeltaSeconds);
	RefreshChrome();
}

bool UTAPathPuzzleWidget::StartPuzzle()
{
	if (Session) return false;
	if (!PuzzleCanvas)
	{
		SetupError = FText::FromString(TEXT("Add a Canvas Panel named PuzzleCanvas to the Widget Blueprint."));
		UE_LOG(LogTemp, Error, TEXT("[Puzzle] %s"), *SetupError.ToString()); return false;
	}
	if (RewardReceiver && !RewardReceiver->GetClass()->ImplementsInterface(UTAPuzzleRewardReceiver::StaticClass()))
	{
		SetupError = FText::FromString(TEXT("RewardReceiver must implement TAPuzzleRewardReceiver.")); return false;
	}
	const int32 Seed = PuzzleSeed < 0 ? FMath::Rand() : PuzzleSeed;
	const FTAPuzzleDefinition Definition = TAPathPuzzleRules::Generate(PuzzleSettings, Seed);
	UTAPathPuzzleSession* NewSession = NewObject<UTAPathPuzzleSession>(this);
	FString Error;
	if (!NewSession->Initialize(Definition, PuzzleSettings, Error)) { SetupError = FText::FromString(Error); return false; }
	Session = NewSession; SessionRewardReceiver = RewardReceiver;
	ViewCenter = GetNodePosition(Session->Definition.StartNode);
	Session->OnChanged.AddDynamic(this, &UTAPathPuzzleWidget::HandleChanged);
	Session->OnSettled.AddDynamic(this, &UTAPathPuzzleWidget::HandleSettled);
	RefreshBoard(); return true;
}

void UTAPathPuzzleWidget::ClosePuzzle() { RemoveFromParent(); }
void UTAPathPuzzleWidget::Undo() { if (Session) Session->Undo(); }
void UTAPathPuzzleWidget::Retry() { /* Reserved for a future reset flow. */ }
void UTAPathPuzzleWidget::SelectNode(int32 Index) { if (Session) Session->SelectNode(Index); }
void UTAPathPuzzleWidget::HandleChanged() { RefreshBoard(); }

void UTAPathPuzzleWidget::HandleSettled(const FTAPuzzleResult& InResult)
{
	if (bSettlementDelivered) return;
	bSettlementDelivered = true;
	const FTAPuzzleResult Result = InResult;
	// 成功/失败均应用：策略集中在 Session::Settle 收集 MainGameEffects 的位置，方便以后改掉。
	// Finish all deliveries before external result events. Each effect is attempted exactly once.
	TArray<int32> Applied;
	for (const auto& Effect : Result.MainGameEffects)
	{
		Applied.Add(IsValid(SessionRewardReceiver)
			? ITAPuzzleRewardReceiver::Execute_ApplyPuzzleReward(SessionRewardReceiver, Effect, GetOwningPlayer(), Result)
			: TAPuzzleRewards::ApplyDefault(GetOwningPlayer(), Effect));
	}
	RefreshBoard();
	// 临时结算流程：成功/失败都保留画面 1 秒，再关闭小游戏并返回主游戏。
	// 后续接正式结算界面/剧情时，删除或替换此定时器；保留 ClosePuzzle 作为统一退出入口。
	// 扫描式时停不暂停世界计时器；若事件提前关闭界面，NativeDestruct 会取消此定时器。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SettlementCloseTimer, this,
			&UTAPathPuzzleWidget::ClosePuzzle, 1.f, false);
	}
	for (int32 I = 0; I < Applied.Num(); ++I) OnRewardApplied.Broadcast(Result.MainGameEffects[I], Applied[I]);
	if (Result.bSucceeded) OnSucceeded.Broadcast(Result);
	else OnFailed.Broadcast(Result);
}

void UTAPathPuzzleWidget::BuildFallback()
{
    if (WidgetTree->RootWidget) return;
    TAPuzzleScopeLayout::Build(WidgetTree, Appearance);
    PuzzleCanvas = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("PuzzleCanvas")));
    ScopeFrame = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("ScopeFrame")));
    ScopeInstruments = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("ScopeInstruments")));
    Crosshair = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("Crosshair")));
    Image_ScopeMask = Cast<UImage>(GetWidgetFromName(TEXT("Image_ScopeMask")));
    Image_EnergyArc = Cast<UImage>(GetWidgetFromName(TEXT("Image_EnergyArc")));
    Image_TimeArc = Cast<UImage>(GetWidgetFromName(TEXT("Image_TimeArc")));
    Text_Energy = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Energy")));
    Text_Time = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Time")));
}
void UTAPathPuzzleWidget::RefreshBoard()
{
	if (!Session || !PuzzleCanvas) return;
	PuzzleCanvas->ClearChildren();
	PuzzleCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	const auto& D = Session->Definition;
	const auto& P = Session->Progress;
	auto Position = [&](int32 Node)
	{
		return Appearance.BoardPadding + D.Nodes[Node].Position * (Appearance.BoardSize - Appearance.BoardPadding * 2);
	};
	int32 LastEdge = INDEX_NONE;
	if (P.Path.Num() > 1) LastEdge = TAPathPuzzleRules::FindEdge(D, P.Path[P.Path.Num()-2], P.Path.Last());
	const bool bFailedMove = Session->State == ETAPuzzleState::Failed && Session->Result.Failure != ETAPuzzleFailure::TimeExpired;
	for (int32 Index = 0; Index < D.Edges.Num(); ++Index)
	{
		const auto& E = D.Edges[Index];
		bool bSelected = false;
		for (int32 I = 1; I < P.Path.Num(); ++I)
			bSelected |= (E.From == P.Path[I-1] && E.To == P.Path[I]) || (E.To == P.Path[I-1] && E.From == P.Path[I]);
		UImage* Image = WidgetTree->ConstructWidget<UImage>();
		Image->SetBrush(bFailedMove && Index == LastEdge ? Appearance.EdgeFailed : (bSelected ? Appearance.EdgeSelected : Appearance.EdgeNormal));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		const FVector2D A = Position(E.From), B = Position(E.To), Delta = B - A;
		UCanvasPanelSlot* EdgeSlot = PuzzleCanvas->AddChildToCanvas(Image);
		EdgeSlot->SetAlignment(FVector2D(0, .5f)); EdgeSlot->SetPosition(A); EdgeSlot->SetSize(FVector2D(Delta.Size(), Appearance.EdgeThickness));
		Image->SetRenderTransformPivot(FVector2D(0, .5f));
		Image->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		FString Text = FString::FromInt(E.Energy);
		if (E.Effect.Type != ETAPuzzleEffectType::None && (E.Effect.IsMainGame() ? Session->Settings.bEnableMainGameEffects : Session->Settings.bEnableMinigameEffects)) Text += TEXT("\n") + E.Effect.GetLabel().ToString();
		Label->SetText(FText::FromString(Text));
		// Rasterize at a larger font size before the board render transform magnifies it.
		FSlateFontInfo Font = Label->GetFont(); Font.Size = 44; Label->SetFont(Font);
		Label->SetRenderTransformPivot(FVector2D::ZeroVector);
		Label->SetRenderScale(FVector2D(.25f));
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* LabelSlot = PuzzleCanvas->AddChildToCanvas(Label);
		LabelSlot->SetPosition(A + Delta * .4f + FVector2D(0, -22)); LabelSlot->SetAutoSize(true);
	}
	for (int32 Index = 0; Index < D.Nodes.Num(); ++Index)
	{
		const auto& N = D.Nodes[Index];
		const bool bSelected = P.Path.Contains(Index);
		const bool bFailed = Session->State == ETAPuzzleState::Failed && !P.Path.IsEmpty() && P.Path.Last() == Index;
		const FSlateBrush& Brush = bFailed ? Appearance.NodeFailed : (bSelected ? Appearance.NodeSelected : (Index == D.StartNode || Index == D.EndNode ? Appearance.NodeEndpoint : Appearance.NodeNormal));
		UTAPathPuzzleNodeWidget* Node = CreateWidget<UTAPathPuzzleNodeWidget>(this, NodeWidgetClass ? NodeWidgetClass.Get() : UTAPathPuzzleNodeWidget::StaticClass());
		if (!Node) continue;
		const bool bShowEffect = N.Effect.IsMainGame() ? Session->Settings.bEnableMainGameEffects : Session->Settings.bEnableMinigameEffects;
		Node->Configure(Index, N.Label, bShowEffect ? N.Effect.GetLabel() : FText::GetEmpty(), Brush, Appearance.bShowNodeLabels);
		Node->OnNodeClicked.AddDynamic(this, &UTAPathPuzzleWidget::SelectNode);
		Node->SetIsEnabled(!Session->IsTerminal());
		UCanvasPanelSlot* NodeSlot = PuzzleCanvas->AddChildToCanvas(Node);
		NodeSlot->SetAlignment(FVector2D(.5f, .5f)); NodeSlot->SetPosition(Position(Index)); NodeSlot->SetSize(Appearance.NodeSize); NodeSlot->SetZOrder(1);
	}
	RefreshChrome();
}

void UTAPathPuzzleWidget::RefreshChrome()
{
	RefreshUndoPrompt();
	if (Text_Status) Text_Status->SetVisibility(ESlateVisibility::Collapsed);
	if (!Session) return;
	const auto& P = Session->Progress;
	if (Progress_Time) { Progress_Time->SetPercent(Session->TimeRemaining / Session->Definition.TimeLimit); Progress_Time->SetFillColorAndOpacity(Appearance.TimerColor); }
	if (Text_Time) Text_Time->SetText(FText::AsNumber(FMath::Max(0, FMath::CeilToInt(Session->TimeRemaining))));
	const int32 RemainingEnergy = FMath::Max(0, P.MaxEnergy - P.EnergyUsed);
	if (Text_Energy) Text_Energy->SetText(FText::AsNumber(RemainingEnergy));
	if (DisplayEnergyFill < 0.f) DisplayEnergyFill = P.MaxEnergy > 0 ? FMath::Clamp(float(RemainingEnergy) / P.MaxEnergy, 0.f, 1.f) : 0.f;
	// Flip only the gauge artwork: its remaining bright section now drains from top to bottom.
	if (Image_EnergyArc) Image_EnergyArc->SetRenderScale(FVector2D(1,-1));
	if (Image_TimeArc) Image_TimeArc->SetRenderScale(FVector2D(1,-1));
	if (Image_EnergyArc)
		if (auto* Material = Image_EnergyArc->GetDynamicMaterial()) Material->SetScalarParameterValue(TEXT("Fill"), DisplayEnergyFill);
	if (Image_TimeArc)
		if (auto* Material = Image_TimeArc->GetDynamicMaterial()) Material->SetScalarParameterValue(TEXT("Fill"), Session->Definition.TimeLimit > 0 ? FMath::Clamp(Session->TimeRemaining / Session->Definition.TimeLimit, 0.f, 1.f) : 0.f);
	if (Text_Stats) Text_Stats->SetText(FText::FromString(FString::Printf(TEXT("Energy: %d / %d\nUnits: %d / %d"), P.EnergyUsed, P.MaxEnergy, P.UnitsUsed, P.MaxUnits)));
	if (Text_Path)
	{
		FString Path;
		for (int32 Index : P.Path) { if (!Path.IsEmpty()) Path += TEXT(" > "); Path += Session->Definition.Nodes[Index].Label.ToString(); }
		Text_Path->SetText(FText::FromString(Path));
	}
	if (Text_Effects)
	{
		FString Effects;
		for (const auto& E : P.CollectedEffects) { if (!Effects.IsEmpty()) Effects += TEXT("\n"); Effects += E.GetLabel().ToString(); }
		Text_Effects->SetText(FText::FromString(Effects));
	}
	if (Button_Undo) { Button_Undo->SetIsEnabled(Session->CanUndo()); Button_Undo->SetVisibility(Session->Settings.bEnableUndo ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
	if (Button_Retry) { Button_Retry->SetIsEnabled(false); Button_Retry->SetVisibility(ESlateVisibility::Collapsed); }
	if (Text_Undo) Text_Undo->SetText(FText::FromString(FString::Printf(TEXT("Undo (%d)"), Session->GetUndoRemaining())));
	if (Text_Retry) Text_Retry->SetText(FText::FromString(FString::Printf(TEXT("Retry (%d)"), Session->GetRetryRemaining())));
}

void UTAPathPuzzleWidget::RefreshLocalizedPrompts()
{
	auto* GI = GetGameInstance();
	if (!GI) return;
	auto* Icons = GI->GetSubsystem<UTAInputIconSubsystem>();
	auto* Loc = GI->GetSubsystem<UTALocalizeSubsystem>();
	if (!Icons || !Loc) return;
	const bool Gamepad = Icons->GetCurrentDeviceType() != EInputDeviceType::KeyboardMouse;
	const TCHAR* TextNames[] = {TEXT("PromptText_0"),TEXT("PromptText_1"),TEXT("PromptText_2"),TEXT("Text_EnergyLabel"),TEXT("Text_TimeLabel")};
	const TCHAR* TextIds[] = {TEXT("Puzzle_Select"),TEXT("Puzzle_MoveView"),TEXT("Puzzle_Undo"),TEXT("Puzzle_Energy"),TEXT("Puzzle_TimeRemaining")};
	for (int32 I=0; I<5; ++I)
		if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(TextNames[I]))) Text->SetText(Loc->GetText(TextIds[I]));
	const FKey Keys[] = {EKeys::W,EKeys::A,EKeys::S,EKeys::D};
	for (int32 Row=0; Row<3; ++Row)
		for (int32 I=0; I<(Row==1 ? 4 : 1); ++I)
			if (auto* Icon=Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("PromptIcon_%d_%d"),Row,I)))))
			{
				if (Gamepad && Row==1 && I>0) { Icon->SetVisibility(ESlateVisibility::Collapsed); continue; }
				const FKey Key = Row==0 ? (Gamepad ? EKeys::Gamepad_FaceButton_Bottom : EKeys::LeftMouseButton) : Row==2 ? (Gamepad ? EKeys::Gamepad_FaceButton_Right : EKeys::RightMouseButton) : (Gamepad ? EKeys::Gamepad_LeftThumbstick : Keys[I]);
				FTAPromptWidgetUtils::ApplyKeyIcon(Icon,Icons->GetIconForKey(Key),32.f);
				// Auto width plus centered height keeps the brush aspect ratio instead
				// of stretching a 32px icon to the full 40px row height.
				if (auto* IconSlot = Cast<UHorizontalBoxSlot>(Icon->Slot))
				{
					IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
					IconSlot->SetHorizontalAlignment(HAlign_Center);
					IconSlot->SetVerticalAlignment(VAlign_Center);
				}
			}
	RefreshUndoPrompt();
}

void UTAPathPuzzleWidget::RefreshUndoPrompt()
{
	if (auto* Row = GetWidgetFromName(TEXT("PromptRow_2")))
		Row->SetRenderOpacity(Session && Session->CanUndo() ? 1.f : .6f);
	auto* GI = GetGameInstance();
	auto* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
	if (Loc)
		if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("PromptText_2"))))
			Text->SetText(FText::Format(Loc->GetText(TEXT("Puzzle_UndoCount")),
				FText::AsNumber(Session ? Session->GetUndoRemaining() : 0)));
}

FVector2D UTAPathPuzzleWidget::GetNodePosition(int32 Index) const
{
    if (!Session || !Session->Definition.Nodes.IsValidIndex(Index)) return FVector2D::ZeroVector;
    return Appearance.BoardPadding + Session->Definition.Nodes[Index].Position * (Appearance.BoardSize - Appearance.BoardPadding * 2);
}

void UTAPathPuzzleWidget::UpdateScope(float DeltaSeconds)
{
    if (!Session || !PuzzleCanvas || !ScopeFrame || !ScopeInstruments || !Crosshair) return;
    const FVector2D ScreenSize = GetCachedGeometry().GetLocalSize();
    const FVector2D InstrumentSize = ScopeInstruments->GetCachedGeometry().GetLocalSize();
    if (ScreenSize.Y > 1 && InstrumentSize.Y > 1)
    {
        ScopeDisplayScale = ScreenSize.Y * ScopeHeightFraction / InstrumentSize.Y;
        ScopeInstruments->SetRenderTransformPivot(FVector2D(.5f));
        ScopeInstruments->SetRenderScale(FVector2D(ScopeDisplayScale));
    }
    SetCursor(EMouseCursor::None);
    if (auto* PC = GetOwningPlayer()) PC->SetShowMouseCursor(false);
    const float Zoom = FMath::Max(.1f, BoardZoom) * ScopeDisplayScale;
    if (!Session->IsTerminal())
        if (const auto* PC = Cast<AThe_AwakeningPlayerController>(GetOwningPlayer()))
            PanBoard(PC->GetPuzzlePanInput(), DeltaSeconds);
    // Every node can reach the reticle, but the board cannot drift indefinitely into empty space.
    ViewCenter.X = FMath::Clamp(ViewCenter.X, 0.0, FMath::Max(0.0, Appearance.BoardSize.X));
    ViewCenter.Y = FMath::Clamp(ViewCenter.Y, 0.0, FMath::Max(0.0, Appearance.BoardSize.Y));
    const FGeometry AimGeometry = Crosshair->GetCachedGeometry();
    const FVector2D AimAbsolute = AimGeometry.LocalToAbsolute(AimGeometry.GetLocalSize() * .5f);
    const FVector2D AimLocal = ScopeFrame->GetCachedGeometry().AbsoluteToLocal(AimAbsolute);
    PuzzleCanvas->SetRenderTransformPivot(FVector2D::ZeroVector);
    PuzzleCanvas->SetRenderScale(FVector2D(Zoom));
    const auto* BoardSlot = Cast<UCanvasPanelSlot>(PuzzleCanvas->Slot);
    PuzzleCanvas->SetRenderTranslation(AimLocal - ViewCenter * Zoom - (BoardSlot ? BoardSlot->GetPosition() : FVector2D::ZeroVector));
    if (Image_ScopeMask)
    {
        const FGeometry MaskGeometry = Image_ScopeMask->GetCachedGeometry();
        const FGeometry ScopeGeometry = ScopeInstruments->GetCachedGeometry();
        const FVector2D Size = MaskGeometry.GetLocalSize();
        if (Size.X > 1 && Size.Y > 1)
        {
            const FVector2D ScopeSize = ScopeGeometry.GetLocalSize();
            const FVector2D Center = MaskGeometry.AbsoluteToLocal(ScopeGeometry.LocalToAbsolute(ScopeSize * .5f));
            const FVector2D Edge = MaskGeometry.AbsoluteToLocal(ScopeGeometry.LocalToAbsolute(ScopeSize * .5f + FVector2D(FMath::Min(ScopeSize.X, ScopeSize.Y) * .5f, 0)));
            if (auto* Material = Image_ScopeMask->GetDynamicMaterial())
            {
                Material->SetScalarParameterValue(TEXT("CenterX"), Center.X / Size.X);
                Material->SetScalarParameterValue(TEXT("CenterY"), Center.Y / Size.Y);
                Material->SetScalarParameterValue(TEXT("Aspect"), Size.X / Size.Y);
                Material->SetScalarParameterValue(TEXT("Radius"), FVector2D::Distance(Center, Edge) / Size.Y);
            }
        }
    }
    AimedNode = INDEX_NONE;
    double Best = FMath::Square(FMath::Max(1.f, AimRadius));
    if (!Session->IsTerminal())
        for (int32 Index = 0; Index < Session->Definition.Nodes.Num(); ++Index)
        {
            const double Distance = FVector2D::DistSquared(ViewCenter, GetNodePosition(Index));
            if (Distance <= Best) { Best = Distance; AimedNode = Index; }
        }
    int32 Index = 0;
    for (UWidget* Child : PuzzleCanvas->GetAllChildren())
        if (auto* Node = Cast<UTAPathPuzzleNodeWidget>(Child))
            Node->SetColorAndOpacity(Index++ == AimedNode ? FLinearColor(.6f,1.f,1.f,1.f) : FLinearColor::White);
}

void UTAPathPuzzleWidget::ConfirmScopeNode()
{
    if (!Session || Session->IsTerminal()) return;
    UpdateScope(0.f);
    if (AimedNode != INDEX_NONE) SelectNode(AimedNode);
}

void UTAPathPuzzleWidget::PanBoard(FVector2D Input, float DeltaSeconds)
{
    if (!Session || Session->IsTerminal()) return;
	// ViewCenter is the board point placed under the fixed reticle. Moving that
	// point in the input direction makes the board itself travel in the requested
	// inverse direction on screen (W down, S up, A right, D left by default).
	ViewCenter += Input.GetClampedToMaxSize(1.f) * FMath::Max(0.f, BoardPanSpeed) * FMath::Max(0.f, DeltaSeconds) / FMath::Max(.1f, BoardZoom);
    ViewCenter.X = FMath::Clamp(ViewCenter.X, 0.0, FMath::Max(0.0, Appearance.BoardSize.X));
    ViewCenter.Y = FMath::Clamp(ViewCenter.Y, 0.0, FMath::Max(0.0, Appearance.BoardSize.Y));
}
