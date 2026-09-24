#include "TAStoryConditionCustomization.h"

#include "TAStoryArrayEntryCustomization.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Story/TADialogueTypes.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FTAStoryConditionCustomization::MakeInstance()
{
	return MakeShared<FTAStoryConditionCustomization>();
}

void FTAStoryConditionCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TAStoryDetails::BuildArrayEntryHeader(StructPropertyHandle, HeaderRow, StructCustomizationUtils);
}

void FTAStoryConditionCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> TypeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTAStoryCondition, Type));
	if (!TypeHandle.IsValid())
	{
		return;
	}
	StructBuilder.AddProperty(TypeHandle.ToSharedRef()).CustomWidget()
	.NameContent()[TypeHandle->CreatePropertyNameWidget()]
	.ValueContent()
	[
		SNew(SComboButton)
		.IsEnabled_Lambda([TypeHandle]() { return TypeHandle->IsValidHandle() && !TypeHandle->IsEditConst(); })
		.OnGetMenuContent_Lambda([TypeHandle]()
		{
			FMenuBuilder Menu(true, nullptr);
			for (const FString Type : { FString(TEXT("Flag")), FString(TEXT("HasItem")), FString(TEXT("Money")), FString(TEXT("Attribute")) })
			{
				Menu.AddMenuEntry(FText::FromString(Type), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([TypeHandle, Type]() { TypeHandle->SetValue(Type); })));
			}
			return Menu.MakeWidget();
		})
		.ButtonContent()
		[
			SNew(STextBlock).Text_Lambda([TypeHandle]()
			{
				FString Type;
				TypeHandle->GetValue(Type);
				return Type.IsEmpty() ? NSLOCTEXT("TAStoryCondition", "SelectType", "Select Type") : FText::FromString(Type);
			})
		]
	];

	auto AddField = [&StructBuilder, StructPropertyHandle, TypeHandle](FName Name, TArray<FString> Types)
	{
		if (const TSharedPtr<IPropertyHandle> Field = StructPropertyHandle->GetChildHandle(Name))
		{
			StructBuilder.AddProperty(Field.ToSharedRef()).Visibility(TAttribute<EVisibility>::CreateLambda([TypeHandle, Types]()
			{
				FString Type;
				TypeHandle->GetValue(Type);
				return Types.ContainsByPredicate([&Type](const FString& Candidate)
				{
					return Candidate.Equals(Type, ESearchCase::IgnoreCase);
				}) ? EVisibility::Visible : EVisibility::Collapsed;
			}));
		}
	};
	AddField(GET_MEMBER_NAME_CHECKED(FTAStoryCondition, Flag), { TEXT("Flag") });
	AddField(GET_MEMBER_NAME_CHECKED(FTAStoryCondition, Item), { TEXT("HasItem") });
	AddField(GET_MEMBER_NAME_CHECKED(FTAStoryCondition, Count), { TEXT("HasItem") });
	AddField(GET_MEMBER_NAME_CHECKED(FTAStoryCondition, Attribute), { TEXT("Attribute") });
	AddField(GET_MEMBER_NAME_CHECKED(FTAStoryCondition, Op), { TEXT("Flag"), TEXT("Money"), TEXT("Attribute") });
	AddField(GET_MEMBER_NAME_CHECKED(FTAStoryCondition, Value), { TEXT("Flag"), TEXT("Money"), TEXT("Attribute") });
}
