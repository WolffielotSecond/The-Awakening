#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Core/TALocalizeSubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontCache.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAFontLanguageTest, "TheAwakening.Fonts.LanguagePreservesLocale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAFontLanguageTest::RunTest(const FString&)
{
    FInternationalization& Intl = FInternationalization::Get();
    const FString PreviousLanguage = Intl.GetCurrentLanguage()->GetName();
    const FString PreviousLocale = Intl.GetCurrentLocale()->GetName();
    ON_SCOPE_EXIT { Intl.SetCurrentLanguage(PreviousLanguage); Intl.SetCurrentLocale(PreviousLocale); };
    Intl.SetCurrentLocale(TEXT("de-DE"));
    UTALocalizeSubsystem* Localization = NewObject<UTALocalizeSubsystem>(NewObject<UGameInstance>());
    for (const FString& Language : { FString(TEXT("en")), FString(TEXT("zh-CN")), FString(TEXT("zh-TW")) })
    {
        TestTrue(TEXT("JSON language loads"), Localization->SetLanguage(Language));
        TestEqual(TEXT("One game language state"), Localization->GetCurrentLanguage(), Language);
        TestEqual(TEXT("UE font language follows game language"), Intl.GetCurrentLanguage()->GetName(), Language);
        TestEqual(TEXT("Numeric/date locale is unchanged"), Intl.GetCurrentLocale()->GetName(), FString(TEXT("de-DE")));
        TestEqual(TEXT("JSON energy translation"), Localization->GetText(TEXT("Puzzle_Energy")).ToString(), Language == TEXT("en") ? FString(TEXT("Energy")) : FString(TEXT("能量")));
    }
    const FString PreviousText = Localization->GetText(TEXT("Puzzle_Energy")).ToString();
    AddExpectedError(TEXT("Unsupported UE language: not-a-real-language"), EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("Invalid culture fails before JSON state changes"), Localization->SetLanguage(TEXT("not-a-real-language")));
    TestEqual(TEXT("Rejected language preserves text"), Localization->GetText(TEXT("Puzzle_Energy")).ToString(), PreviousText);
    TestEqual(TEXT("Rejected language preserves game state"), Localization->GetCurrentLanguage(), FString(TEXT("zh-TW")));
    TestEqual(TEXT("Rejected language preserves UE state"), Intl.GetCurrentLanguage()->GetName(), FString(TEXT("zh-TW")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAFontRoutingTest, "TheAwakening.Fonts.CultureRoutingAndDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAFontRoutingTest::RunTest(const FString&)
{
    UFont* Global = LoadObject<UFont>(nullptr, TEXT("/Game/Fonts/TA_GlobalFont.TA_GlobalFont"));
    if (!TestNotNull(TEXT("Unified font loads"), Global)) return false;
    UTextBlock* Text = NewObject<UTextBlock>();
    TestTrue(TEXT("New native UMG text uses unified font"), Text->GetFont().FontObject == Global);
    if (!TestTrue(TEXT("Slate font services are available"), FSlateApplication::IsInitialized())) return false;
    auto Cache = FSlateApplication::Get().GetRenderer()->GetFontCache();
    FInternationalization& Intl = FInternationalization::Get();
    const FString PreviousLanguage = Intl.GetCurrentLanguage()->GetName();
    ON_SCOPE_EXIT { Intl.SetCurrentLanguage(PreviousLanguage); };
    auto Check = [&](const TCHAR* Language, UTF32CHAR Codepoint, const TCHAR* Expected)
    {
        Intl.SetCurrentLanguage(Language);
        float Scale = 0.f;
        const FFontData& Data = Cache->GetFontDataForCodepoint(Text->GetFont(), Codepoint, Scale);
        TestEqual(TEXT("Resolved original font face"), GetPathNameSafe(Data.GetFontFaceAsset()), FString(Expected));
        TestEqual(TEXT("Original default variable-font instance"), Data.GetSubFaceIndex(), 0);
        TestEqual(TEXT("No font scale compensation"), Scale, 1.f);
        TestTrue(TEXT("Existing widget retains unified font while language changes"), Text->GetFont().FontObject == Global);
    };
    Check(TEXT("en"), 'A', TEXT("/Game/Fonts/HarmonyOS/HarmonyOS_Sans.HarmonyOS_Sans"));
    Check(TEXT("zh-CN"), 0x9aa8, TEXT("/Game/Fonts/HarmonyOS/HarmonyOS_Sans_SC.HarmonyOS_Sans_SC"));
    Check(TEXT("zh-TW"), 0x9aa8, TEXT("/Game/Fonts/HarmonyOS/HarmonyOS_Sans_TC.HarmonyOS_Sans_TC"));
    Check(TEXT("zh-TW"), 0x8fc7, TEXT("/Game/Fonts/HarmonyOS/HarmonyOS_Sans_SC.HarmonyOS_Sans_SC"));
    FSlateFontInfo Font = Text->GetFont();
    Font.TypefaceFontName = TEXT("Regular");
    Text->SetFont(Font);
    Check(TEXT("zh-TW"), 0x9aa8, TEXT("/Game/Fonts/HarmonyOS/HarmonyOS_Sans_TC.HarmonyOS_Sans_TC"));
    return true;
}
#endif
