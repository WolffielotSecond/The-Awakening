#include "Settings/TASettingsSubsystem.h"

#include "Core/TALocalizeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameUserSettings.h"
#include "Engine/LocalPlayer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DefaultValueHelper.h"
#include "The_Awakening.h"

namespace
{
	const TCHAR* SettingsSection = TEXT("TheAwakening.PlayerSettings");

	FTASettingDefinition MakeToggle(FName Id, ETAGameSettingsPage Page, const TCHAR* NameId,
		const TCHAR* DescriptionId, bool bDefault)
	{
		FTASettingDefinition D;
		D.SettingId = Id;
		D.Page = Page;
		D.Type = ETASettingType::Toggle;
		D.NameTextId = NameId;
		D.DescriptionTextId = DescriptionId;
		D.DefaultValue = FTASettingValue::Boolean(bDefault);
		return D;
	}

	FTASettingDefinition MakeChoice(FName Id, ETAGameSettingsPage Page, const TCHAR* NameId,
		const TCHAR* DescriptionId, FName Default, std::initializer_list<TPair<FName, const TCHAR*>> Options)
	{
		FTASettingDefinition D;
		D.SettingId = Id;
		D.Page = Page;
		D.Type = ETASettingType::Choice;
		D.NameTextId = NameId;
		D.DescriptionTextId = DescriptionId;
		D.DefaultValue = FTASettingValue::ChoiceValue(Default);
		for (const TPair<FName, const TCHAR*>& Option : Options)
		{
			FTASettingChoice Choice;
			Choice.Value = Option.Key;
			Choice.NameTextId = Option.Value;
			D.Choices.Add(MoveTemp(Choice));
		}
		return D;
	}

	FTASettingDefinition MakeSlider(FName Id, ETAGameSettingsPage Page, const TCHAR* NameId,
		const TCHAR* DescriptionId, float Default, float Min, float Max, float Step, FName Format,
		bool bSubmenuOnly = false)
	{
		FTASettingDefinition D;
		D.SettingId = Id;
		D.Page = Page;
		D.Type = ETASettingType::Slider;
		D.NameTextId = NameId;
		D.DescriptionTextId = DescriptionId;
		D.DefaultValue = FTASettingValue::Numeric(Default);
		D.Minimum = Min;
		D.Maximum = Max;
		D.Step = Step;
		D.DisplayFormat = Format;
		D.bSubmenuOnly = bSubmenuOnly;
		return D;
	}
}

void UTASettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildBuiltInDefinitions();
	LoadSavedValues();
	for (const FTASettingDefinition& Definition : Definitions)
	{
		const FTASettingValue* Value = CurrentValues.Find(Definition.SettingId);
		ApplyValue(Definition, Value ? *Value : Definition.DefaultValue);
	}
}

void UTASettingsSubsystem::UseDefinitionAsset(const UTASettingsDefinitionAsset* DefinitionAsset)
{
	if (!DefinitionAsset || DefinitionAsset->Definitions.IsEmpty()) return;
	TSet<FName> SeenIds;
	for (const FTASettingDefinition& Definition : DefinitionAsset->Definitions)
	{
		if (Definition.SettingId.IsNone() || SeenIds.Contains(Definition.SettingId))
		{
			UE_LOG(LogThe_Awakening, Warning, TEXT("Settings catalog has an empty or duplicate id; catalog was not applied."));
			return;
		}
		SeenIds.Add(Definition.SettingId);
	}
	Definitions = DefinitionAsset->Definitions;
	LoadSavedValues();
	for (const FTASettingDefinition& Definition : Definitions)
	{
		FTASettingValue Value;
		if (GetValue(Definition.SettingId, Value)) ApplyValue(Definition, Value);
	}
}

void UTASettingsSubsystem::BuildBuiltInDefinitions()
{
	Definitions.Reset();

	FTASettingDefinition Language = MakeChoice(TEXT("Game.Language"), ETAGameSettingsPage::Game,
		TEXT("Settings.Language.Name"), TEXT("Settings.Language.Description"), TEXT("zh-CN"),
		{{TEXT("zh-CN"), TEXT("Settings.Language.zhCN")}, {TEXT("zh-TW"), TEXT("Settings.Language.zhTW")}, {TEXT("en"), TEXT("Settings.Language.en")}});
	Definitions.Add(MoveTemp(Language));

	FTASettingDefinition WindowMode = MakeChoice(TEXT("Display.WindowMode"), ETAGameSettingsPage::Display,
		TEXT("Settings.Display.WindowMode.Name"), TEXT("Settings.Display.WindowMode.Description"), TEXT("WindowedFullscreen"),
		{{TEXT("Fullscreen"), TEXT("Settings.Display.WindowMode.Fullscreen")},
		 {TEXT("WindowedFullscreen"), TEXT("Settings.Display.WindowMode.Borderless")},
		 {TEXT("Windowed"), TEXT("Settings.Display.WindowMode.Windowed")}});
	Definitions.Add(MoveTemp(WindowMode));

	FTASettingDefinition Resolution = MakeChoice(TEXT("Display.Resolution"), ETAGameSettingsPage::Display,
		TEXT("Settings.Display.Resolution.Name"), TEXT("Settings.Display.Resolution.Description"), TEXT("1920x1080"),
		{{TEXT("1280x720"), TEXT("Settings.Display.Resolution.1280x720")},
		 {TEXT("1600x900"), TEXT("Settings.Display.Resolution.1600x900")},
		 {TEXT("1920x1080"), TEXT("Settings.Display.Resolution.1920x1080")}});
	Definitions.Add(MoveTemp(Resolution));

	Definitions.Add(MakeToggle(TEXT("Display.VSync"), ETAGameSettingsPage::Display,
		TEXT("Settings.Display.VSync.Name"), TEXT("Settings.Display.VSync.Description"), false));
	Definitions.Add(MakeSlider(TEXT("Display.FrameRateLimit"), ETAGameSettingsPage::Display,
		TEXT("Settings.Display.FrameRateLimit.Name"), TEXT("Settings.Display.FrameRateLimit.Description"),
		60.0f, 30.0f, 240.0f, 15.0f, TEXT("Integer")));

	FTASettingDefinition BrightnessMenu;
	BrightnessMenu.SettingId = TEXT("Display.BrightnessMenu");
	BrightnessMenu.Page = ETAGameSettingsPage::Display;
	BrightnessMenu.Type = ETASettingType::Submenu;
	BrightnessMenu.NameTextId = TEXT("Settings.Display.Brightness.Name");
	BrightnessMenu.DescriptionTextId = TEXT("Settings.Display.Brightness.Description");
	BrightnessMenu.DefaultValue = FTASettingValue::ChoiceValue(NAME_None);
	BrightnessMenu.SubmenuTarget = ETASettingSubmenuTarget::Brightness;
	Definitions.Add(MoveTemp(BrightnessMenu));

	Definitions.Add(MakeSlider(TEXT("Display.Brightness"), ETAGameSettingsPage::Display,
		TEXT("Settings.Display.Brightness.Name"), TEXT("Settings.Display.Brightness.Description"),
		2.2f, 1.0f, 3.0f, 0.1f, TEXT("Gamma"), true));

	// Other pages intentionally have no entries until the project has a runtime
	// consumer for the setting. This keeps the catalog honest and data-driven.
}

const FTASettingDefinition* UTASettingsSubsystem::FindDefinition(FName SettingId) const
{
	return Definitions.FindByPredicate([SettingId](const FTASettingDefinition& Definition)
	{
		return Definition.SettingId == SettingId;
	});
}

bool UTASettingsSubsystem::GetValue(FName SettingId, FTASettingValue& OutValue) const
{
	if (const FTASettingValue* Found = CurrentValues.Find(SettingId))
	{
		OutValue = *Found;
		return true;
	}
	if (const FTASettingDefinition* Definition = FindDefinition(SettingId))
	{
		OutValue = Definition->DefaultValue;
		return true;
	}
	return false;
}

bool UTASettingsSubsystem::SetValue(FName SettingId, const FTASettingValue& Value)
{
	const FTASettingDefinition* Definition = FindDefinition(SettingId);
	if (!Definition || Definition->Type == ETASettingType::Submenu || Definition->DefaultValue.Kind != Value.Kind)
	{
		return false;
	}

	FTASettingValue Normalized = Value;
	if (Definition->Type == ETASettingType::Slider)
	{
		Normalized.Number = FMath::GridSnap(FMath::Clamp(Value.Number, Definition->Minimum, Definition->Maximum),
			FMath::Max(Definition->Step, KINDA_SMALL_NUMBER));
		Normalized.Number = FMath::Clamp(Normalized.Number, Definition->Minimum, Definition->Maximum);
	}
	else if (Definition->Type == ETASettingType::Choice && !Definition->Choices.ContainsByPredicate(
		[&Normalized](const FTASettingChoice& Choice) { return Choice.Value == Normalized.Choice; }))
	{
		return false;
	}

	CurrentValues.Add(SettingId, Normalized);
	ApplyValue(*Definition, Normalized);
	SaveValues();
	OnSettingValueChanged.Broadcast(SettingId);
	return true;
}

bool UTASettingsSubsystem::AdjustValue(FName SettingId, int32 Direction)
{
	const FTASettingDefinition* Definition = FindDefinition(SettingId);
	if (!Definition || Direction == 0)
	{
		return false;
	}

	FTASettingValue Value;
	if (!GetValue(SettingId, Value)) return false;
	if (Definition->Type == ETASettingType::Toggle)
	{
		Value.bBoolean = !Value.bBoolean;
	}
	else if (Definition->Type == ETASettingType::Choice && Definition->Choices.Num() > 0)
	{
		int32 Index = Definition->Choices.IndexOfByPredicate([&Value](const FTASettingChoice& Choice)
		{
			return Choice.Value == Value.Choice;
		});
		Index = Index < 0 ? 0 : (Index + (Direction < 0 ? -1 : 1) + Definition->Choices.Num()) % Definition->Choices.Num();
		Value.Choice = Definition->Choices[Index].Value;
	}
	else if (Definition->Type == ETASettingType::Slider)
	{
		Value.Number += Definition->Step * (Direction < 0 ? -1.0f : 1.0f);
	}
	else
	{
		return false;
	}
	return SetValue(SettingId, Value);
}

void UTASettingsSubsystem::SetFavorite(FName SettingId, bool bFavorite)
{
	if (!FindDefinition(SettingId)) return;
	if (bFavorite) FavoriteSettingIds.Add(SettingId);
	else FavoriteSettingIds.Remove(SettingId);
	SaveValues();
}

void UTASettingsSubsystem::RestoreDefaults()
{
	CurrentValues.Reset();
	for (const FTASettingDefinition& Definition : Definitions)
	{
		if (Definition.Type != ETASettingType::Submenu)
		{
			CurrentValues.Add(Definition.SettingId, Definition.DefaultValue);
			ApplyValue(Definition, Definition.DefaultValue);
			OnSettingValueChanged.Broadcast(Definition.SettingId);
		}
	}
	SaveValues();
}

void UTASettingsSubsystem::LoadSavedValues()
{
	CurrentValues.Reset();
	FavoriteSettingIds.Reset();
	if (!GConfig) return;

	for (const FTASettingDefinition& Definition : Definitions)
	{
		if (Definition.Type == ETASettingType::Submenu) continue;
		FString Encoded;
		const FString Key = FString::Printf(TEXT("Value_%s"), *Definition.SettingId.ToString());
		if (!GConfig->GetString(SettingsSection, *Key, Encoded, GGameUserSettingsIni))
		{
			continue;
		}
		FTASettingValue Value = Definition.DefaultValue;
		if (Definition.Type == ETASettingType::Toggle && Encoded.StartsWith(TEXT("B:")))
		{
			Value.bBoolean = Encoded.RightChop(2).ToBool();
		}
		else if (Definition.Type == ETASettingType::Slider && Encoded.StartsWith(TEXT("N:")))
		{
			FDefaultValueHelper::ParseFloat(Encoded.RightChop(2), Value.Number);
		}
		else if (Definition.Type == ETASettingType::Choice && Encoded.StartsWith(TEXT("C:")))
		{
			Value.Choice = FName(*Encoded.RightChop(2));
		}
		else continue;

		if (Definition.Type == ETASettingType::Choice && !Definition.Choices.ContainsByPredicate(
			[&Value](const FTASettingChoice& Choice) { return Choice.Value == Value.Choice; })) continue;
		if (Definition.Type == ETASettingType::Slider)
		{
			Value.Number = FMath::Clamp(Value.Number, Definition.Minimum, Definition.Maximum);
		}
		CurrentValues.Add(Definition.SettingId, Value);
	}

	FString FavoriteList;
	if (GConfig->GetString(SettingsSection, TEXT("Favorites"), FavoriteList, GGameUserSettingsIni))
	{
		TArray<FString> Ids;
		FavoriteList.ParseIntoArray(Ids, TEXT(","), true);
		for (const FString& Id : Ids)
		{
			const FName SettingId(*Id);
			if (FindDefinition(SettingId)) FavoriteSettingIds.Add(SettingId);
		}
	}
}

void UTASettingsSubsystem::SaveValues() const
{
	if (!GConfig) return;
	for (const FTASettingDefinition& Definition : Definitions)
	{
		const FTASettingValue* Value = CurrentValues.Find(Definition.SettingId);
		if (!Value) continue;
		FString Encoded;
		switch (Value->Kind)
		{
		case ETASettingValueKind::Boolean: Encoded = FString::Printf(TEXT("B:%d"), Value->bBoolean ? 1 : 0); break;
		case ETASettingValueKind::Number: Encoded = FString::Printf(TEXT("N:%.4f"), Value->Number); break;
		case ETASettingValueKind::Choice: Encoded = FString::Printf(TEXT("C:%s"), *Value->Choice.ToString()); break;
		default: continue;
		}
		const FString Key = FString::Printf(TEXT("Value_%s"), *Definition.SettingId.ToString());
		GConfig->SetString(SettingsSection, *Key, *Encoded, GGameUserSettingsIni);
	}
	TArray<FString> SortedFavorites;
	for (const FName Id : FavoriteSettingIds) SortedFavorites.Add(Id.ToString());
	SortedFavorites.Sort();
	GConfig->SetString(SettingsSection, TEXT("Favorites"), *FString::Join(SortedFavorites, TEXT(",")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UTASettingsSubsystem::ApplyValue(const FTASettingDefinition& Definition, const FTASettingValue& Value)
{
	if (Definition.SettingId == TEXT("Game.Language") && Value.Kind == ETASettingValueKind::Choice)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UTALocalizeSubsystem* Localize = GameInstance->GetSubsystem<UTALocalizeSubsystem>())
			{
				Localize->SetLanguage(Value.Choice.ToString());
			}
		}
		return;
	}

	UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (!UserSettings) return;

	if (Definition.SettingId == TEXT("Display.WindowMode") && Value.Kind == ETASettingValueKind::Choice)
	{
		EWindowMode::Type Mode = EWindowMode::Windowed;
		if (Value.Choice == TEXT("Fullscreen")) Mode = EWindowMode::Fullscreen;
		else if (Value.Choice == TEXT("WindowedFullscreen")) Mode = EWindowMode::WindowedFullscreen;
		UserSettings->SetFullscreenMode(Mode);
		UserSettings->ApplySettings(false);
		UserSettings->SaveSettings();
	}
	else if (Definition.SettingId == TEXT("Display.Resolution") && Value.Kind == ETASettingValueKind::Choice)
	{
		TArray<FString> Parts;
		Value.Choice.ToString().ParseIntoArray(Parts, TEXT("x"), true);
		int32 Width = 0;
		int32 Height = 0;
		if (Parts.Num() == 2 && LexTryParseString(Width, *Parts[0]) && LexTryParseString(Height, *Parts[1]) && Width > 0 && Height > 0)
		{
			UserSettings->SetScreenResolution(FIntPoint(Width, Height));
			UserSettings->ApplySettings(false);
			UserSettings->SaveSettings();
		}
	}
	else if (Definition.SettingId == TEXT("Display.VSync") && Value.Kind == ETASettingValueKind::Boolean)
	{
		UserSettings->SetVSyncEnabled(Value.bBoolean);
		UserSettings->ApplySettings(false);
		UserSettings->SaveSettings();
	}
	else if (Definition.SettingId == TEXT("Display.FrameRateLimit") && Value.Kind == ETASettingValueKind::Number)
	{
		UserSettings->SetFrameRateLimit(Value.Number);
		UserSettings->ApplySettings(false);
		UserSettings->SaveSettings();
	}
	else if (Definition.SettingId == TEXT("Display.Brightness") && Value.Kind == ETASettingValueKind::Number)
	{
		UserSettings->SetGamma(Value.Number);
		UserSettings->ApplySettings(false);
		UserSettings->SaveSettings();
	}
}
