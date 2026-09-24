#include "TAPortraitEntryCustomization.h"
#include "TAStoryArrayEntryCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "PropertyHandle.h"
#include "Story/TADialogueTypes.h"

TSharedRef<IPropertyTypeCustomization> FTAStoryPortraitEntryCustomization::MakeInstance()
{
	return MakeShared<FTAStoryPortraitEntryCustomization>();
}

void FTAStoryPortraitEntryCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TAStoryDetails::BuildArrayEntryHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);
}

void FTAStoryPortraitEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder,
	IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	(void)StructCustomizationUtils;
	auto Child = [&StructPropertyHandle](const FName Name) -> TSharedPtr<IPropertyHandle>
	{
		return StructPropertyHandle->GetChildHandle(Name);
	};
	auto AddToGroup = [](IDetailGroup& Group, const TSharedPtr<IPropertyHandle>& Property)
	{
		if (Property.IsValid())
		{
			Group.AddPropertyRow(Property.ToSharedRef());
		}
	};

	if (TSharedPtr<IPropertyHandle> CharacterId = Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, CharacterId)))
	{
		StructBuilder.AddProperty(CharacterId.ToSharedRef());
	}

	IDetailGroup& Images = StructBuilder.AddGroup(TEXT("PortraitImages"), NSLOCTEXT("TAPortraitDetails", "Images", "Images"), true);
	AddToGroup(Images, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, bImagesSpecified)));
	AddToGroup(Images, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, Base)));
	AddToGroup(Images, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, EyesOpen)));
	AddToGroup(Images, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, EyesClosed)));
	AddToGroup(Images, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, MouthOpen)));
	AddToGroup(Images, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, MouthClosed)));

	IDetailGroup& Position = StructBuilder.AddGroup(TEXT("PortraitPosition"), NSLOCTEXT("TAPortraitDetails", "Position", "Position"), true);
	AddToGroup(Position, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, bPositionSpecified)));
	if (TSharedPtr<IPropertyHandle> PositionHandle = Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, Position)))
	{
		AddToGroup(Position, PositionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTAPortraitPosition, X)));
		AddToGroup(Position, PositionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTAPortraitPosition, Y)));
	}

	IDetailGroup& Size = StructBuilder.AddGroup(TEXT("PortraitSize"), NSLOCTEXT("TAPortraitDetails", "Size", "Size"), true);
	AddToGroup(Size, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, bScaleSpecified)));
	AddToGroup(Size, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, Scale)));

	IDetailGroup& Visibility = StructBuilder.AddGroup(TEXT("PortraitVisibility"), NSLOCTEXT("TAPortraitDetails", "Visibility", "Visibility"), true);
	AddToGroup(Visibility, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, bVisibleSpecified)));
	AddToGroup(Visibility, Child(GET_MEMBER_NAME_CHECKED(FTAPortraitEntry, bVisible)));
}
