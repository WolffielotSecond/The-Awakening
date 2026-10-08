#include "TASettingLocationsCustomization.h"
#include "Settings/TASettingsTypes.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
 TArray<FTASettingsPageDefinition> ReadPages(const TSharedRef<IPropertyHandle>& Handle)
 {
  TArray<UObject*> Objects; Handle->GetOuterObjects(Objects);
  auto Pages=FTASettingsCatalog::BuildPages();
  for (UObject* Object:Objects) if (auto* Asset=Cast<UTASettingsDefinitionAsset>(Object))
  { if (Asset->PageDefinitionAsset) Pages=Asset->PageDefinitionAsset->Pages; break; }
  Pages.RemoveAll([](const auto& P){return P.PageId.IsNone() || P.PageId==FTASettingsCatalog::FavoritesPageId() || P.PageId==TEXT("Favorites");});
  FTASettingsCatalog::SortPages(Pages); return Pages;
 }
 ECheckBoxState ReadState(const TSharedRef<IPropertyHandle>& Handle,const TArray<FTASettingLocation>& Targets)
 {
  TArray<void*> Raw; Handle->AccessRawData(Raw); int32 Selected=0,Total=0;
  for (void* Ptr:Raw) if (Ptr) for (const auto& L:Targets)
  { ++Total; if (static_cast<FTASettingLocations*>(Ptr)->Items.Contains(L)) ++Selected; }
  return Selected==0?ECheckBoxState::Unchecked:Selected==Total?ECheckBoxState::Checked:ECheckBoxState::Undetermined;
 }
 void WriteSelection(const TSharedRef<IPropertyHandle>& Handle,const TArray<FTASettingLocation>& Targets,bool bSelected)
 {
  if (!Handle->IsValidHandle() || Handle->IsEditConst()) return;
  const FScopedTransaction Transaction(NSLOCTEXT("TASettings", "EditLocations", "Change setting page and section membership"));
  TArray<UObject*> Objects; Handle->GetOuterObjects(Objects); for (UObject* Object:Objects) if (Object) Object->Modify();
  Handle->NotifyPreChange(); TArray<void*> Raw; Handle->AccessRawData(Raw);
  for (void* Ptr:Raw) if (Ptr)
  {
   auto& Items=static_cast<FTASettingLocations*>(Ptr)->Items;
   for (const auto& L:Targets) { if (bSelected) Items.AddUnique(L); else Items.Remove(L); }
  }
  Handle->NotifyPostChange(EPropertyChangeType::ValueSet); Handle->NotifyFinishedChangingProperties();
 }
 FText Label(FName Id,const TSharedPtr<FJsonObject>& Texts)
 {
  FString Value;
  if (Texts && Texts->TryGetStringField(Id.ToString(),Value)) return FText::FromString(Value+TEXT("  [")+Id.ToString()+TEXT("]"));
  return FText::FromName(Id);
 }
 void AddCheck(FMenuBuilder& Menu,const TSharedRef<IPropertyHandle>& Handle,FText Text,TArray<FTASettingLocation> Targets)
 {
  Menu.AddWidget(SNew(SCheckBox)
   .IsEnabled_Lambda([Handle](){return Handle->IsValidHandle() && !Handle->IsEditConst();})
   .IsChecked_Lambda([Handle,Targets](){return ReadState(Handle,Targets);})
   .OnCheckStateChanged_Lambda([Handle,Targets](ECheckBoxState State){WriteSelection(Handle,Targets,State==ECheckBoxState::Checked);})
   [SNew(STextBlock).Text(Text)],FText::GetEmpty());
 }
 TSharedRef<SWidget> BuildMenu(const TSharedRef<IPropertyHandle>& Handle)
 {
  const auto Pages=ReadPages(Handle);
  FString Json; TSharedPtr<FJsonObject> Texts;
  if (FFileHelper::LoadFileToString(Json,*(FPaths::ProjectContentDir()/TEXT("Localization/zh-CN.json"))))
   FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Texts);
  FMenuBuilder Menu(false,nullptr);
  for (const auto& P:Pages)
  {
   Menu.AddSubMenu(Label(P.PageId,Texts),FText::FromName(P.PageId),FNewMenuDelegate::CreateLambda([Handle,P,Texts](FMenuBuilder& Child)
   {
    TArray<FTASettingLocation> All;
    for (const auto& S:P.Sections) if (!S.SectionId.IsNone()) { FTASettingLocation L; L.PageId=P.PageId; L.SectionId=S.SectionId; All.AddUnique(L); }
    if (!All.IsEmpty()) AddCheck(Child,Handle,NSLOCTEXT("TASettings", "AllSections", "Select all sections"),All);
    for (const auto& L:All) AddCheck(Child,Handle,Label(L.SectionId,Texts),{L});
    if (All.IsEmpty()) Child.AddMenuEntry(NSLOCTEXT("TASettings", "NoSections", "Add sections to this page first"),FText::GetEmpty(),FSlateIcon(),FUIAction());
   }));
  }
  // Keep orphaned selections visible and removable after a page/section rename.
  TArray<void*> Raw; Handle->AccessRawData(Raw); TArray<FTASettingLocation> Existing;
  for (void* Ptr:Raw) if (Ptr) for (const auto& L:static_cast<FTASettingLocations*>(Ptr)->Items) Existing.AddUnique(L);
  for (const auto& L:Existing)
  {
   if (L.PageId==FTASettingsCatalog::FavoritesPageId() || L.PageId==TEXT("Favorites")) continue;
   const auto* P=Pages.FindByPredicate([&](const auto& D){return D.PageId==L.PageId;});
   if (!P || !P->Sections.ContainsByPredicate([&](const auto& S){return S.SectionId==L.SectionId;}))
    AddCheck(Menu,Handle,FText::FromString(TEXT("Missing: ")+L.PageId.ToString()+TEXT(" / ")+L.SectionId.ToString()),{L});
  }
  return Menu.MakeWidget();
 }
}
TSharedRef<IPropertyTypeCustomization> FTASettingLocationsCustomization::MakeInstance()
{ return MakeShared<FTASettingLocationsCustomization>(); }
void FTASettingLocationsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Handle,FDetailWidgetRow& Row,IPropertyTypeCustomizationUtils& Utils)
{
 Row.NameContent()[Handle->CreatePropertyNameWidget()]
 .ValueContent().MinDesiredWidth(280)
 [SNew(SComboButton).OnGetMenuContent_Lambda([Handle](){return BuildMenu(Handle);})
  .ButtonContent()[SNew(STextBlock).Text_Lambda([Handle]()
  {
   TArray<void*> Raw; Handle->AccessRawData(Raw); int32 Count=0;
   for (void* Ptr:Raw) if (Ptr) Count=FMath::Max(Count,static_cast<FTASettingLocations*>(Ptr)->Items.Num());
   return FText::Format(NSLOCTEXT("TASettings", "LocationsSummary", "Pages / sections ({0} selected)"),FText::AsNumber(Count));
  })]];
}
