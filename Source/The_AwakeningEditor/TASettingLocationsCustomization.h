#pragma once
#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class FTASettingLocationsCustomization final : public IPropertyTypeCustomization
{
public:
 static TSharedRef<IPropertyTypeCustomization> MakeInstance();
 virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Handle,FDetailWidgetRow& Row,IPropertyTypeCustomizationUtils& Utils) override;
 virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Handle,IDetailChildrenBuilder& Builder,IPropertyTypeCustomizationUtils& Utils) override {}
};
