#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "The_AwakeningPlayerController.h"
#include "Core/TALocalizeSubsystem.h"
#include "Story/TADialogueHistoryWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTADialogueHistoryEnglishLayoutTest,"TheAwakening.Dialogue.HistoryEnglishLayout",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTADialogueHistoryEnglishLayoutTest::RunTest(const FString&)
{
 auto& Intl=FInternationalization::Get();
 const FString OriginalLanguage=Intl.GetCurrentLanguage()->GetName();
 ON_SCOPE_EXIT { Intl.SetCurrentLanguage(OriginalLanguage); };
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 ON_SCOPE_EXIT { World->DestroyWorld(false); };
 auto* PCClass=LoadClass<AThe_AwakeningPlayerController>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController.BP_ThirdPersonPlayerController_C"));
 auto* PC=PCClass?World->SpawnActor<AThe_AwakeningPlayerController>(PCClass):nullptr;
 if (!TestNotNull(TEXT("Real player controller"),PC)) return false;
 auto* Local=NewObject<ULocalPlayer>(GEngine); Local->PlayerController=PC; PC->Player=Local; World->AddController(PC);
 auto* Class=LoadClass<UTADialogueHistoryWidget>(nullptr,TEXT("/Game/UI/Dialogue/WBP_DialogueHistory.WBP_DialogueHistory_C"));
 auto* Widget=Class?CreateWidget<UTADialogueHistoryWidget>(PC,Class):nullptr;
 if (!TestNotNull(TEXT("Real History template"),Widget)) return false;
 // A real JSON subsystem supplies the language. The fixture has no game instance
 // on its World, so raw text IDs intentionally supply deterministic long lines.
 auto* Localization=NewObject<UTALocalizeSubsystem>(NewObject<UGameInstance>());
 Widget->LocalizeSubsystem=Localization;
 const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget(); // Keep constructed UI alive through language changes.
 UTextBlock* Defaults=NewObject<UTextBlock>();
 const FSlateFontInfo OriginalFont=Defaults->GetFont();
 TArray<FTAStoryHistoryEntry> Entries;
 FTAStoryHistoryEntry Spoken;
 Spoken.SpeakerNameId=TEXT("Speaker");
 Spoken.TextId=TEXT("A long English dialogue entry should wrap within the History viewport without requiring smaller text or horizontal scrolling. Multiple sentences remain in one entry and its height follows the content.");
 Entries.Add(Spoken);
 FTAStoryHistoryEntry Choice;
 Choice.bIsChoice=true;
 Choice.TextId=TEXT("A long selected option should also wrap and retain its existing choice prefix.");
 Entries.Add(Choice);
 FTAStoryHistoryEntry Plain; Plain.TextId=TEXT("Final entry without a speaker."); Entries.Add(Plain);
 for (const FString& Language:{FString(TEXT("en")),FString(TEXT("zh-CN")),FString(TEXT("zh-TW")),FString(TEXT("en"))})
 {
  TestTrue(TEXT("Actual JSON language switch"),Localization->SetLanguage(Language));
  Widget->SetHistory(Entries);
  const bool English=Language==TEXT("en");
  TestEqual(TEXT("Entry count preserved"),Widget->Box_Entries->GetChildrenCount(),Entries.Num());
  for (int32 Index=0;Index<Entries.Num();++Index)
  {
   auto* Text=Cast<UTextBlock>(Widget->Box_Entries->GetChildAt(Index));
   if (!TestNotNull(TEXT("Generated TextBlock"),Text)) return false;
   auto* EntrySlot=Cast<UVerticalBoxSlot>(Text->Slot);
   TestEqual(TEXT("Wrap enabled only for English"),Text->GetAutoWrapText(),English?true:Defaults->GetAutoWrapText());
   TestEqual(TEXT("English spacing only between entries"),EntrySlot->GetPadding(),FMargin(0,0,0,English && Index+1<Entries.Num()?8.f:0.f));
   TestTrue(TEXT("Content drives entry height"),EntrySlot->GetSize().SizeRule==ESlateSizeRule::Automatic);
   TestTrue(TEXT("Full font appearance retained"),Text->GetFont()==OriginalFont);
   if (English) TestTrue(TEXT("Entry uses ScrollBox width"),EntrySlot->GetHorizontalAlignment()==HAlign_Fill);
  }
  TestEqual(TEXT("Speaker prefix unchanged"),Cast<UTextBlock>(Widget->Box_Entries->GetChildAt(0))->GetText().ToString(),FString(TEXT("Speaker："))+Spoken.TextId);
  TestEqual(TEXT("Choice prefix unchanged"),Cast<UTextBlock>(Widget->Box_Entries->GetChildAt(1))->GetText().ToString(),FString(TEXT("▶ "))+Choice.TextId);
 }
 // Prove a language event itself rebuilds existing rows, without another SetHistory.
 Localization->SetLanguage(TEXT("zh-CN"));
 TestFalse(TEXT("Open History returns to original Chinese wrapping"),Cast<UTextBlock>(Widget->Box_Entries->GetChildAt(0))->GetAutoWrapText());
 Localization->SetLanguage(TEXT("en"));
 TestTrue(TEXT("Open History refreshes English wrapping on event"),Cast<UTextBlock>(Widget->Box_Entries->GetChildAt(0))->GetAutoWrapText());
 Widget->SetHistory({});
 TestEqual(TEXT("Empty history remains empty"),Widget->Box_Entries->GetChildrenCount(),0);
 Widget->NativeDestruct();
 return true;
}
#endif
