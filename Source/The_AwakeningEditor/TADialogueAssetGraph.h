#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "Story/TADialogueTypes.h"
#include "TADialogueAssetGraph.generated.h"

UENUM()
enum class ETADialogueGraphNodeType : uint8
{
	Entry,
	Dialogue,
	Choice,
	End
};

UCLASS()
class UTAStoryEdGraph : public UEdGraph
{
	GENERATED_BODY()
};

UCLASS()
class UTAStoryGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	virtual void AllocateDefaultPins() override;
	virtual void PostPlacedNewNode() override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual void NodeConnectionListChanged() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif

	UPROPERTY(VisibleAnywhere, Category = "Story Node")
	ETADialogueGraphNodeType NodeType = ETADialogueGraphNodeType::Dialogue;

	UPROPERTY(EditAnywhere, Category = "Story Node", meta = (EditCondition = "NodeType != ETADialogueGraphNodeType::Entry"))
	FTAStoryNode NodeData;

	UEdGraphPin* GetFlowOutputPin(int32 ChoiceIndex = INDEX_NONE) const;
	UEdGraphPin* GetFlowInputPin() const;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;

private:
	void SynchronizeChoicePins(const FPropertyChangedEvent& PropertyChangedEvent);
};

UCLASS()
class UTAStoryGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
};
