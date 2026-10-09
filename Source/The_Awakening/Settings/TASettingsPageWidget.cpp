#include "Settings/TASettingsPageWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UTASettingsPageWidget::NativeConstruct()
{
 Super::NativeConstruct();
 if (Button_Page) Button_Page->OnClicked.AddUniqueDynamic(this,&UTASettingsPageWidget::Clicked);
}
void UTASettingsPageWidget::Configure(const FTASettingsPageDefinition& Definition,FText Label,bool bSelected)
{
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
void UTASettingsPageWidget::Clicked() { OnPageActivated.Broadcast(PageDefinition.PageId); }
