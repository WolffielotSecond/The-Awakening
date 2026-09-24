#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

namespace TAStoryDetails
{
	void BuildArrayEntryHeader(TSharedRef<IPropertyHandle> ElementHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils);
}

/** Shared header for story event and choice array entries. */
class FTAStoryArrayEntryCustomization final : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder,
		IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
};
