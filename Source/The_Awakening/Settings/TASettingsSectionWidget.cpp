#include "Settings/TASettingsSectionWidget.h"
#include "Components/TextBlock.h"
void UTASettingsSectionWidget::SetTitle(FText Title) { if (Text_Title) Text_Title->SetText(Title); }
