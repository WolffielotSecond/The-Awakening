#include "TAStoryArrayEntryCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

void TAStoryDetails::BuildArrayEntryHeader(TSharedRef<IPropertyHandle> ElementHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()[ElementHandle->CreatePropertyNameWidget()];
	const TSharedPtr<IPropertyHandle> ParentHandle = ElementHandle->GetParentHandle();
	if (!ParentHandle.IsValid() || !ParentHandle->AsArray().IsValid() || ElementHandle->GetIndexInArray() == INDEX_NONE)
	{
		HeaderRow.ValueContent()[ElementHandle->CreatePropertyValueWidget()];
		return;
	}

	const TWeakPtr<IPropertyUtilities> WeakUtilities = CustomizationUtils.GetPropertyUtilities();
	HeaderRow.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			ElementHandle->CreatePropertyValueWidget()
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			SNew(SButton)
			.Text(NSLOCTEXT("TAStoryDetails", "DeleteEntry", "Delete"))
			.ToolTipText(NSLOCTEXT("TAStoryDetails", "DeleteEntryTooltip", "Delete only this list element."))
			.IsEnabled_Lambda([ElementHandle, WeakUtilities]()
			{
				const TSharedPtr<IPropertyUtilities> Utilities = WeakUtilities.Pin();
				return Utilities.IsValid() && Utilities->IsPropertyEditingEnabled()
					&& ElementHandle->IsValidHandle() && !ElementHandle->IsEditConst();
			})
			.OnClicked_Lambda([ElementHandle, WeakUtilities]()
			{
				if (const TSharedPtr<IPropertyUtilities> Utilities = WeakUtilities.Pin())
				{
					// Delete after Slate dispatch. The property API supplies the undo transaction
					// and change notifications, including the removed array index.
					Utilities->EnqueueDeferredAction(FSimpleDelegate::CreateLambda([ElementHandle, WeakUtilities]()
					{
						if (!ElementHandle->IsValidHandle())
						{
							return;
						}
						const TSharedPtr<IPropertyHandle> ArrayProperty = ElementHandle->GetParentHandle();
						const TSharedPtr<IPropertyHandleArray> Array = ArrayProperty.IsValid() ? ArrayProperty->AsArray() : nullptr;
						const int32 Index = ElementHandle->GetIndexInArray();
						uint32 Count = 0;
						if (Array.IsValid() && Array->GetNumElements(Count) == FPropertyAccess::Success
							&& Index >= 0 && static_cast<uint32>(Index) < Count
							&& Array->DeleteItem(Index) == FPropertyAccess::Success)
						{
							if (const TSharedPtr<IPropertyUtilities> RefreshUtilities = WeakUtilities.Pin())
							{
								RefreshUtilities->RequestRefresh();
							}
						}
					}));
				}
				return FReply::Handled();
			})
		]
	];
}

TSharedRef<IPropertyTypeCustomization> FTAStoryArrayEntryCustomization::MakeInstance()
{
	return MakeShared<FTAStoryArrayEntryCustomization>();
}

void FTAStoryArrayEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TAStoryDetails::BuildArrayEntryHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);
}

void FTAStoryArrayEntryCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);
	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		if (const TSharedPtr<IPropertyHandle> Child = StructPropertyHandle->GetChildHandle(Index))
		{
			StructBuilder.AddProperty(Child.ToSharedRef());
		}
	}
}
