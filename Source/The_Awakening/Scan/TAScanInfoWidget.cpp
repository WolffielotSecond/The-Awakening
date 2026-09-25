#include "Scan/TAScanInfoWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/TALocalizeSubsystem.h"
#include "Engine/GameInstance.h"

void UTAScanInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		LocalizeSubsystem = GameInstance->GetSubsystem<UTALocalizeSubsystem>();
	}
	if (LocalizeSubsystem)
	{
		LocalizeSubsystem->OnLanguageChanged.AddUniqueDynamic(this, &UTAScanInfoWidget::HandleLanguageChanged);
	}
	RefreshLocalizedText();
}

void UTAScanInfoWidget::NativeDestruct()
{
	if (LocalizeSubsystem)
	{
		LocalizeSubsystem->OnLanguageChanged.RemoveDynamic(this, &UTAScanInfoWidget::HandleLanguageChanged);
		LocalizeSubsystem = nullptr;
	}
	Super::NativeDestruct();
}

void UTAScanInfoWidget::SetTargetInfo(const FTAScanTargetInfo& TargetInfo)
{
	CachedTargetInfo = TargetInfo;
	RefreshLocalizedText();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BP_OnTargetInfoUpdated(TargetInfo);
}

void UTAScanInfoWidget::HandleLanguageChanged()
{
	RefreshLocalizedText();
}

void UTAScanInfoWidget::RefreshLocalizedText()
{
	auto ResolveText = [this](const FText& TextOrId)
	{
		const FString TextId = TextOrId.ToString();
		return LocalizeSubsystem && !TextId.IsEmpty()
			? LocalizeSubsystem->GetText(TextId)
			: TextOrId;
	};

	if (Text_TargetName)
	{
		Text_TargetName->SetText(ResolveText(CachedTargetInfo.Name));
	}
	if (Text_TargetDescription)
	{
		Text_TargetDescription->SetText(ResolveText(CachedTargetInfo.Description));
	}
	if (Text_TargetType)
	{
		const FString TypeTextId = GetTargetTypeTextId();
		Text_TargetType->SetText(LocalizeSubsystem
			? LocalizeSubsystem->GetText(TypeTextId)
			: FText::FromString(TypeTextId));
	}
}

FString UTAScanInfoWidget::GetTargetTypeTextId() const
{
	switch (CachedTargetInfo.TargetType)
	{
	case ETAScanTargetType::Item: return TEXT("Scan_TargetType_Item");
	case ETAScanTargetType::Terminal: return TEXT("Scan_TargetType_Terminal");
	case ETAScanTargetType::Enemy: return TEXT("Scan_TargetType_Enemy");
	case ETAScanTargetType::Generic:
	default: return TEXT("Scan_TargetType_Generic");
	}
}
