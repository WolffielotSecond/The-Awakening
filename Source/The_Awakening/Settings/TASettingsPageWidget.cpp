#include "Settings/TASettingsPageWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ButtonSlot.h"

void UTASettingsPageWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (Button_Page) Button_Page->OnClicked.AddUniqueDynamic(this,&UTASettingsPageWidget::Clicked);
}
void UTASettingsPageWidget::Configure(const FTASettingsPageDefinition& Definition,FText Label,bool bSelected,bool bEnglishLayout)
{
 ApplyLanguageLayout(bEnglishLayout);
 PageDefinition=Definition;
 if (Button_Page)
 {
  if (!bColorInitialized) { NormalColor=Button_Page->GetBackgroundColor(); bColorInitialized=true; }
  auto Style=Button_Page->GetStyle(); Style.Pressed=Style.Normal; Style.Hovered=Style.Normal; Button_Page->SetStyle(Style);
  Button_Page->SetBackgroundColor(bSelected?FLinearColor(0.12f,0.42f,0.82f,1):NormalColor);
 }
 if (Text_Name) Text_Name->SetText(Label);
 if (Image_Highlight) Image_Highlight->SetVisibility(bSelected?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 OnSelectionChanged(bSelected);
}
void UTASettingsPageWidget::ApplyLanguageLayout(bool bEnglish)
{
 if (!Text_Name) return;
 auto* ContentSlot=Cast<UButtonSlot>(Text_Name->Slot);
 if (!ContentSlot) return;
 if (!bLayoutDefaultsCaptured)
 {
  bAuthoredNameWrap=Text_Name->GetAutoWrapText();
  AuthoredContentPadding=ContentSlot->GetPadding();
  AuthoredContentHorizontal=ContentSlot->GetHorizontalAlignment();
  bLayoutDefaultsCaptured=true;
 }
 Text_Name->SetAutoWrapText(bEnglish || bAuthoredNameWrap);
 ContentSlot->SetHorizontalAlignment(bEnglish?HAlign_Fill:AuthoredContentHorizontal);
 ContentSlot->SetPadding(bEnglish?FMargin(12.f,8.f):AuthoredContentPadding);
 // No height override: the existing SizeBox/Auto VBox slot grows with wrapped text.
}
void UTASettingsPageWidget::Clicked() { OnPageActivated.Broadcast(PageDefinition.PageId); }
