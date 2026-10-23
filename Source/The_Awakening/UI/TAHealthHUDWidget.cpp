#include "UI/TAHealthHUDWidget.h"
#include "Health/TAHealthComponent.h"
#include "Core/TALocalizeSubsystem.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void UTAHealthHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Keep the health readout out of the central gameplay area, with DPI-scaled margins.
	if (Text_Health && WidgetTree)
	{
		auto* ScreenCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (!ScreenCanvas)
		{
			UWidget* PreviousRoot = WidgetTree->RootWidget;
			ScreenCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
			WidgetTree->RootWidget = ScreenCanvas;
			if (PreviousRoot && PreviousRoot != Text_Health)
			{
				auto* PreviousSlot = ScreenCanvas->AddChildToCanvas(PreviousRoot);
				PreviousSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
				PreviousSlot->SetOffsets(FMargin(0.f));
			}
		}
		Text_Health->RemoveFromParent();
		auto* HealthSlot = ScreenCanvas->AddChildToCanvas(Text_Health);
		HealthSlot->SetAnchors(FAnchors(1.f, 0.f));
		HealthSlot->SetAlignment(FVector2D(1.f, 0.f));
		HealthSlot->SetPosition(FVector2D(-24.f, 24.f));
		HealthSlot->SetAutoSize(true);
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		// 角色复活不换 Pawn，但仍跟随 Possess 变化，避免 HUD 绑在旧 Pawn 上
		PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &UTAHealthHUDWidget::HandlePossessedPawnChanged);
		ObservePawn(PC->GetPawn());
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.AddUniqueDynamic(this, &UTAHealthHUDWidget::HandleLanguageChanged);
		}
	}
}

void UTAHealthHUDWidget::NativeDestruct()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UTAHealthHUDWidget::HandlePossessedPawnChanged);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTALocalizeSubsystem* Loc = GI->GetSubsystem<UTALocalizeSubsystem>())
		{
			Loc->OnLanguageChanged.RemoveDynamic(this, &UTAHealthHUDWidget::HandleLanguageChanged);
		}
	}
	StopObserving();
	Super::NativeDestruct();
}

void UTAHealthHUDWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	ObservePawn(NewPawn);
}

void UTAHealthHUDWidget::ObservePawn(APawn* Pawn)
{
	UTAHealthComponent* Health = UTAHealthComponent::FindHealthComponent(Pawn);
	if (Health == ObservedHealth.Get())
	{
		return;
	}
	StopObserving();
	ObservedHealth = Health;
	if (Health)
	{
		Health->OnHealthChanged.AddUniqueDynamic(this, &UTAHealthHUDWidget::HandleHealthChanged);
		ApplyPresentation(Health->GetHealth(), Health->GetMaxHealth(), 0.f);
	}
	else
	{
		ApplyPresentation(0.f, 0.f, 0.f);
	}
}

void UTAHealthHUDWidget::StopObserving()
{
	if (UTAHealthComponent* Health = ObservedHealth.Get())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &UTAHealthHUDWidget::HandleHealthChanged);
	}
	ObservedHealth.Reset();
}

void UTAHealthHUDWidget::HandleHealthChanged(float Current, float Max, float Delta)
{
	ApplyPresentation(Current, Max, Delta);
}

void UTAHealthHUDWidget::HandleLanguageChanged()
{
	const UTAHealthComponent* Health = ObservedHealth.Get();
	ApplyPresentation(Health ? Health->GetHealth() : 0.f, Health ? Health->GetMaxHealth() : 0.f, 0.f);
}

void UTAHealthHUDWidget::ApplyPresentation(float Current, float Max, float Delta)
{
	if (ProgressBar_Health)
	{
		ProgressBar_Health->SetPercent(Max > 0.f ? FMath::Clamp(Current / Max, 0.f, 1.f) : 0.f);
	}
	if (Text_Health)
	{
		const UGameInstance* GI = GetGameInstance();
		const UTALocalizeSubsystem* Loc = GI ? GI->GetSubsystem<UTALocalizeSubsystem>() : nullptr;
		const FText Pattern = Loc ? Loc->GetText(TEXT("HUD_Health_Format")) : FText::FromString(TEXT("{Current}/{Max}"));
		// 向上取整：还活着（Health > 0）时不显示 0
		FFormatNamedArguments Args;
		Args.Add(TEXT("Current"), FText::AsNumber(FMath::CeilToInt(FMath::Max(Current, 0.f))));
		Args.Add(TEXT("Max"), FText::AsNumber(FMath::CeilToInt(FMath::Max(Max, 0.f))));
		Text_Health->SetText(FText::Format(FTextFormat(Pattern), Args));
	}
	OnHealthPresentationChanged(Current, Max, Delta);
}
