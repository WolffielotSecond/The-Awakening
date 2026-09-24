#include "TADialogueGraphNodeCustomization.h"

#include "TADialogueAssetGraph.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"

TSharedRef<IDetailCustomization> FTAStoryGraphNodeDetails::MakeInstance()
{
	return MakeShared<FTAStoryGraphNodeDetails>();
}

void FTAStoryGraphNodeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}

	const UTAStoryGraphNode* Node = Cast<UTAStoryGraphNode>(Objects[0].Get());
	if (!Node)
	{
		return;
	}

	const TSharedRef<IPropertyHandle> NodeDataHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UTAStoryGraphNode, NodeData), UTAStoryGraphNode::StaticClass());
	NodeDataHandle->MarkHiddenByCustomization();
	DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UTAStoryGraphNode, NodeType), UTAStoryGraphNode::StaticClass())->MarkHiddenByCustomization();

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Story Node");
	auto AddNodeField = [&Category, &NodeDataHandle](const FName FieldName)
	{
		if (TSharedPtr<IPropertyHandle> Field = NodeDataHandle->GetChildHandle(FieldName))
		{
			Category.AddProperty(Field.ToSharedRef());
		}
	};

	AddNodeField(GET_MEMBER_NAME_CHECKED(FTAStoryNode, Id));
	if (Node->NodeType == ETADialogueGraphNodeType::Dialogue)
	{
		AddNodeField(GET_MEMBER_NAME_CHECKED(FTAStoryNode, SpeakerId));
		AddNodeField(GET_MEMBER_NAME_CHECKED(FTAStoryNode, SpeakerNameId));
		AddNodeField(GET_MEMBER_NAME_CHECKED(FTAStoryNode, TextId));
		AddNodeField(GET_MEMBER_NAME_CHECKED(FTAStoryNode, Portraits));
		AddNodeField(GET_MEMBER_NAME_CHECKED(FTAStoryNode, EventsOnEnter));
		AddNodeField(GET_MEMBER_NAME_CHECKED(FTAStoryNode, EventsOnExit));
	}
	else if (Node->NodeType == ETADialogueGraphNodeType::Choice)
	{
		AddNodeField(GET_MEMBER_NAME_CHECKED(FTAStoryNode, Choices));
	}
}
