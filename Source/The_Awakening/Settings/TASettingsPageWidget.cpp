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
 if (Text_Name) Text_Name->SetText(Label);
 if (Image_Highlight) Image_Highlight->SetVisibility(bSelected?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 OnSelectionChanged(bSelected);
}
void UTASettingsPageWidget::Clicked() { OnPageActivated.Broadcast(PageDefinition.PageId); }
