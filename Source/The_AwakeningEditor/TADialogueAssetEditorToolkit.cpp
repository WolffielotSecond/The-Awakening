#include "TADialogueAssetEditorToolkit.h"

#include "TADialogueAssetGraph.h"
#include "TAStoryGraphClipboard.h"
#include "TAStoryGraphValidation.h"
#include "Story/TAStoryAsset.h"
#include "DetailsViewArgs.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/ScopeExit.h"
#include "Misc/TransactionObjectEvent.h"
#include "IDetailsView.h"
#include "ScopedTransaction.h"
#include "PropertyEditorModule.h"
#include "GraphEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "TADialogueAssetEditor"

const FName FTADialogueAssetEditorToolkit::MainTabId(TEXT("TADialogueAssetEditor_Main"));

FTADialogueAssetEditorToolkit::~FTADialogueAssetEditorToolkit()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
	if (StoryGraph && GraphChangedHandle.IsValid())
	{
		StoryGraph->RemoveOnGraphChangedHandler(GraphChangedHandle);
	}
}

void FTADialogueAssetEditorToolkit::InitEditor(const TSharedPtr<IToolkitHost>& InToolkitHost, UTAStoryAsset* InAsset)
{
	StoryAsset = InAsset;
	EnsureGraph();
	BindGraphCommands();
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
	if (StoryGraph)
	{
		GraphChangedHandle = StoryGraph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateSP(this, &FTADialogueAssetEditorToolkit::HandleGraphChanged));
	}

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("TADialogueAssetEditor_Layout_v1")
		->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split(FTabManager::NewStack()->AddTab(MainTabId, ETabState::OpenedTab)->SetHideTabWell(true)));

	InitAssetEditor(EToolkitMode::Standalone, InToolkitHost, FName(TEXT("TAStoryAssetEditor")), Layout, true, true, InAsset);
	TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FTADialogueAssetEditorToolkit::ExtendToolbar));
	AddToolbarExtender(ToolbarExtender);
	RegenerateMenusAndToolbars();
}

FName FTADialogueAssetEditorToolkit::GetToolkitFName() const
{
	return FName(TEXT("TAStoryAssetEditor"));
}

FText FTADialogueAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Story Editor");
}

FString FTADialogueAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("Story");
}

FLinearColor FTADialogueAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.15f, 0.45f, 0.75f, 1.0f);
}

void FTADialogueAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	InTabManager->RegisterTabSpawner(MainTabId, FOnSpawnTab::CreateSP(this, &FTADialogueAssetEditorToolkit::SpawnMainTab))
		.SetDisplayName(LOCTEXT("MainTabName", "Story"));
}

void FTADialogueAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(MainTabId);
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

TSharedRef<SDockTab> FTADialogueAssetEditorToolkit::SpawnMainTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("MainTabLabel", "Story Graph"))
		[
			BuildEditorContents()
		];
}

TSharedRef<SWidget> FTADialogueAssetEditorToolkit::BuildEditorContents()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bLockable = false;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	StoryDetailsView = PropertyEditor.CreateDetailView(DetailsArgs);
	NodeDetailsView = PropertyEditor.CreateDetailView(DetailsArgs);
	StoryDetailsView->SetObject(StoryAsset);
	NodeDetailsView->SetObject(nullptr);

	SGraphEditor::FGraphEditorEvents GraphEvents;
	GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FTADialogueAssetEditorToolkit::HandleSelectionChanged);
	FGraphAppearanceInfo Appearance;
	Appearance.CornerText = LOCTEXT("StoryCornerText", "STORY");
	GraphEditor = SNew(SGraphEditor)
		.Appearance(Appearance)
		.AdditionalCommands(GraphCommands)
		.GraphToEdit(StoryGraph)
		.IsEditable(true)
		.GraphEvents(GraphEvents)
		.ShowGraphStateOverlay(false);

	return SNew(SSplitter)
		+ SSplitter::Slot()
		.Value(0.22f)
		[
			SNew(SBorder)
			.Padding(6.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("StorySettingsHeader", "STORY SETTINGS"))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					StoryDetailsView.ToSharedRef()
				]
			]
		]
		+ SSplitter::Slot()
		.Value(0.56f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
				[
					SNew(SButton).Text(LOCTEXT("AddDialogue", "+ Dialogue"))
					.OnClicked_Lambda([this]() { AddGraphNode(static_cast<uint8>(ETADialogueGraphNodeType::Dialogue)); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
				[
					SNew(SButton).Text(LOCTEXT("AddChoice", "+ Choice"))
					.OnClicked_Lambda([this]() { AddGraphNode(static_cast<uint8>(ETADialogueGraphNodeType::Choice)); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
				[
					SNew(SButton).Text(LOCTEXT("AddEnd", "+ End"))
					.OnClicked_Lambda([this]() { AddGraphNode(static_cast<uint8>(ETADialogueGraphNodeType::End)); return FReply::Handled(); })
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				GraphEditor.ToSharedRef()
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return StatusMessage; })
				.ColorAndOpacity_Lambda([this]() { return bLastCompileSucceeded ? FSlateColor(FLinearColor(0.25f, 0.8f, 0.35f)) : FSlateColor(FLinearColor(0.9f, 0.55f, 0.2f)); })
			]
		]
		+ SSplitter::Slot()
		.Value(0.22f)
		[
			SNew(SBorder)
			.Padding(6.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("NodeDetailsHeader", "NODE DETAILS"))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					NodeDetailsView.ToSharedRef()
				]
			]
		];
}

void FTADialogueAssetEditorToolkit::HandleSelectionChanged(const TSet<UObject*>& Selection)
{
	UObject* SelectedObject = nullptr;
	for (UObject* Object : Selection)
	{
		if (Cast<UTAStoryGraphNode>(Object))
		{
			SelectedObject = Object;
			break;
		}
	}
	NodeDetailsView->SetObject(SelectedObject);
}

void FTADialogueAssetEditorToolkit::BindGraphCommands()
{
	GraphCommands = MakeShared<FUICommandList>();
	GraphCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::CopySelectedNodes),
		FCanExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::CanCopySelectedNodes));
	GraphCommands->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::PasteNodes),
		FCanExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::CanPasteNodes));
	GraphCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::DeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::CanDeleteSelectedNodes));
	GraphCommands->MapAction(FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::UndoGraphAction));
	GraphCommands->MapAction(FGenericCommands::Get().Redo,
		FExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::RedoGraphAction));
	GraphCommands->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateLambda([this]()
		{
			if (GraphEditor.IsValid())
			{
				GraphEditor->SelectAllNodes();
			}
		}));
}

void FTADialogueAssetEditorToolkit::CopySelectedNodes()
{
	if (CanCopySelectedNodes())
	{
		TAStoryGraphClipboard::CopyNodes(GraphEditor->GetSelectedNodes());
	}
}

bool FTADialogueAssetEditorToolkit::CanCopySelectedNodes() const
{
	if (GraphEditor.IsValid())
	{
		for (UObject* Object : GraphEditor->GetSelectedNodes())
		{
			const UTAStoryGraphNode* Node = Cast<UTAStoryGraphNode>(Object);
			if (Node && Node->GetGraph() == StoryGraph && Node->CanDuplicateNode())
			{
				return true;
			}
		}
	}
	return false;
}

void FTADialogueAssetEditorToolkit::PasteNodes()
{
	if (CanPasteNodes())
	{
		GraphEditor->ClearSelectionSet();
		TAStoryGraphClipboard::PasteNodes(StoryGraph, GraphEditor->GetPasteLocation2f());
	}
}

bool FTADialogueAssetEditorToolkit::CanPasteNodes() const
{
	return StoryAsset && GraphEditor.IsValid() && TAStoryGraphClipboard::CanPaste(StoryGraph);
}

bool FTADialogueAssetEditorToolkit::CanDeleteSelectedNodes() const
{
	if (!StoryAsset || !StoryGraph || !GraphEditor.IsValid())
	{
		return false;
	}
	for (UObject* Object : GraphEditor->GetSelectedNodes())
	{
		const UTAStoryGraphNode* Node = Cast<UTAStoryGraphNode>(Object);
		if (Node && Node->GetGraph() == StoryGraph && Node->CanUserDeleteNode())
		{
			return true;
		}
	}
	return false;
}

void FTADialogueAssetEditorToolkit::DeleteSelectedNodes()
{
	if (!CanDeleteSelectedNodes())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteStoryNodes", "Delete Story Nodes"));
	StoryAsset->Modify();
	StoryGraph->Modify();
	const TSet<UObject*> Selection = GraphEditor->GetSelectedNodes();
	// Release Details and graph selection references before removing their nodes.
	GraphEditor->ClearSelectionSet();
	NodeDetailsView->SetObject(nullptr);
	for (UObject* Object : Selection)
	{
		UTAStoryGraphNode* Node = Cast<UTAStoryGraphNode>(Object);
		if (Node && Node->GetGraph() == StoryGraph && Node->CanUserDeleteNode())
		{
			Node->Modify();
			// The engine removes the node from the graph and breaks both ends of its links.
			Node->DestroyNode();
		}
	}
	StoryGraph->NotifyGraphChanged();
}

void FTADialogueAssetEditorToolkit::UndoGraphAction()
{
	if (GEditor)
	{
		GraphEditor->ClearSelectionSet();
		GEditor->UndoTransaction();
	}
}

void FTADialogueAssetEditorToolkit::RedoGraphAction()
{
	if (GEditor)
	{
		GraphEditor->ClearSelectionSet();
		GEditor->RedoTransaction();
	}
}

bool FTADialogueAssetEditorToolkit::MatchesContext(const FTransactionContext& InContext,
	const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	if (!StoryAsset)
	{
		return false;
	}
	for (const auto& ObjectContext : TransactionObjectContexts)
	{
		if (ObjectContext.Key && (ObjectContext.Key == StoryAsset || ObjectContext.Key->IsIn(StoryAsset)))
		{
			return true;
		}
	}
	return false;
}

void FTADialogueAssetEditorToolkit::PostUndo(bool bSuccess)
{
	if (bSuccess && GraphEditor.IsValid())
	{
		GraphEditor->ClearSelectionSet();
		NodeDetailsView->SetObject(nullptr);
		StoryDetailsView->ForceRefresh();
		GraphEditor->NotifyGraphChanged();
	}
}

void FTADialogueAssetEditorToolkit::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

void FTADialogueAssetEditorToolkit::HandleGraphChanged(const FEdGraphEditAction& Action)
{
	(void)Action;
	if (StoryAsset)
	{
		StoryAsset->Modify();
		StoryAsset->InvalidateCompiledStory();
		StoryAsset->MarkPackageDirty();
		SetStatus(LOCTEXT("NeedsCompile", "Story graph changed. Run Compile / Validate before playing it."), false);
	}
}

void FTADialogueAssetEditorToolkit::EnsureGraph()
{
	if (!StoryAsset)
	{
		return;
	}
	if (!StoryAsset->StoryGraph)
	{
		StoryAsset->Modify();
		StoryAsset->StoryGraph = NewObject<UTAStoryEdGraph>(StoryAsset, TEXT("StoryGraph"), RF_Transactional);
	}
	StoryGraph = Cast<UTAStoryEdGraph>(StoryAsset->StoryGraph);
	if (!StoryGraph)
	{
		return;
	}
	StoryGraph->Schema = UTAStoryGraphSchema::StaticClass();

	if (StoryGraph->Nodes.IsEmpty())
	{
		UTAStoryGraphNode* EntryNode = CreateGraphNode(static_cast<uint8>(ETADialogueGraphNodeType::Entry), 0, 0);
		UTAStoryGraphNode* DialogueNode = CreateGraphNode(static_cast<uint8>(ETADialogueGraphNodeType::Dialogue), 300, 0);
		if (EntryNode && DialogueNode && EntryNode->GetFlowOutputPin() && DialogueNode->GetFlowInputPin())
		{
			StoryGraph->GetSchema()->TryCreateConnection(EntryNode->GetFlowOutputPin(), DialogueNode->GetFlowInputPin());
		}
	}
}

UTAStoryGraphNode* FTADialogueAssetEditorToolkit::CreateGraphNode(uint8 RawNodeType, int32 X, int32 Y)
{
	if (!StoryGraph)
	{
		return nullptr;
	}

	StoryGraph->Modify();
	UTAStoryGraphNode* Node = NewObject<UTAStoryGraphNode>(StoryGraph, NAME_None, RF_Transactional);
	Node->NodeType = static_cast<ETADialogueGraphNodeType>(RawNodeType);
	Node->NodeData.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Node->NodeData.Type = Node->NodeType == ETADialogueGraphNodeType::Choice ? TEXT("choice")
		: Node->NodeType == ETADialogueGraphNodeType::End ? TEXT("end") : TEXT("dialogue");
	if (Node->NodeType == ETADialogueGraphNodeType::Choice)
	{
		Node->NodeData.Choices.SetNum(2);
	}
	Node->NodePosX = X;
	Node->NodePosY = Y;
	StoryGraph->AddNode(Node, true, true);
	Node->CreateNewGuid();
	Node->AllocateDefaultPins();
	StoryAsset->MarkPackageDirty();
	return Node;
}

void FTADialogueAssetEditorToolkit::AddGraphNode(uint8 NodeType)
{
	const FScopedTransaction Transaction(LOCTEXT("AddStoryNodeTransaction", "Add Story Node"));
	if (UTAStoryGraphNode* Node = CreateGraphNode(NodeType, 300, NextNodeY))
	{
		NextNodeY += 160;
		if (GraphEditor.IsValid())
		{
			GraphEditor->SetNodeSelection(Node, true);
		}
	}
}

void FTADialogueAssetEditorToolkit::CompileStory()
{
	bool bCompileSucceeded = false;
	ON_SCOPE_EXIT
	{
		if (!bCompileSucceeded && StoryAsset)
		{
			StoryAsset->Modify();
			StoryAsset->InvalidateCompiledStory();
			StoryAsset->MarkPackageDirty();
		}
		if (CompileSaveMode == ETADialogueCompileSaveMode::Always
			|| (CompileSaveMode == ETADialogueCompileSaveMode::OnSuccess && bCompileSucceeded))
		{
			SaveAsset_Execute();
		}
	};

	if (!StoryAsset || !StoryGraph)
	{
		SetStatus(LOCTEXT("MissingGraph", "Cannot compile: story graph is not available."), false);
		return;
	}

	TArray<UTAStoryGraphNode*> EntryNodes;
	TArray<UTAStoryGraphNode*> StoryNodes;
	TSet<FString> NodeIds;
	for (UEdGraphNode* GraphNode : StoryGraph->Nodes)
	{
		UTAStoryGraphNode* Node = Cast<UTAStoryGraphNode>(GraphNode);
		if (!Node)
		{
			continue;
		}
		if (Node->NodeType == ETADialogueGraphNodeType::Entry)
		{
			EntryNodes.Add(Node);
			continue;
		}
		if (Node->NodeData.Id.IsEmpty())
		{
			SetStatus(LOCTEXT("NodeMissingId", "Compile failed: every story node needs an ID."), false);
			return;
		}
		if (NodeIds.Contains(Node->NodeData.Id))
		{
			SetStatus(FText::Format(LOCTEXT("DuplicateNodeId", "Compile failed: duplicate node ID {0}."), FText::FromString(Node->NodeData.Id)), false);
			return;
		}
		NodeIds.Add(Node->NodeData.Id);
		StoryNodes.Add(Node);
	}

	if (EntryNodes.Num() != 1)
	{
		SetStatus(LOCTEXT("EntryCountInvalid", "Compile failed: the graph must contain exactly one Entry node."), false);
		return;
	}
	UEdGraphPin* EntryOutput = EntryNodes[0]->GetFlowOutputPin();
	if (!EntryOutput || EntryOutput->LinkedTo.Num() != 1)
	{
		SetStatus(LOCTEXT("EntryNotConnected", "Compile failed: connect Entry to the first story node."), false);
		return;
	}
	UTAStoryGraphNode* EntryTarget = Cast<UTAStoryGraphNode>(EntryOutput->LinkedTo[0]->GetOwningNode());
	if (!EntryTarget || EntryTarget->NodeType == ETADialogueGraphNodeType::Entry)
	{
		SetStatus(LOCTEXT("EntryTargetInvalid", "Compile failed: Entry must connect to a dialogue, choice, or end node."), false);
		return;
	}

	FTAStoryData Compiled;
	Compiled.StoryId = StoryAsset->StoryId.IsEmpty() ? StoryAsset->GetName() : StoryAsset->StoryId;
	Compiled.Entry = EntryTarget->NodeData.Id;
	for (UTAStoryGraphNode* Node : StoryNodes)
	{
		FTAStoryNode CompiledNode = Node->NodeData;
		CompiledNode.Type = Node->NodeType == ETADialogueGraphNodeType::Choice ? TEXT("choice")
			: Node->NodeType == ETADialogueGraphNodeType::End ? TEXT("end") : TEXT("dialogue");

		if (Node->NodeType == ETADialogueGraphNodeType::Dialogue)
		{
			CompiledNode.Next.Reset();
			UEdGraphPin* Output = Node->GetFlowOutputPin();
			if (Output && Output->LinkedTo.Num() > 1)
			{
				SetStatus(FText::Format(LOCTEXT("DialogueMultipleNext", "Compile failed: dialogue node {0} has multiple outgoing links."), FText::FromString(Node->NodeData.Id)), false);
				return;
			}
			if (Output && Output->LinkedTo.Num() == 1)
			{
				const UTAStoryGraphNode* Target = Cast<UTAStoryGraphNode>(Output->LinkedTo[0]->GetOwningNode());
				if (!Target || Target->NodeType == ETADialogueGraphNodeType::Entry)
				{
					SetStatus(LOCTEXT("DialogueTargetInvalid", "Compile failed: a dialogue link points to an invalid node."), false);
					return;
				}
				CompiledNode.Next = Target->NodeData.Id;
			}
		}
		else if (Node->NodeType == ETADialogueGraphNodeType::Choice)
		{
			if (CompiledNode.Choices.IsEmpty())
			{
				SetStatus(FText::Format(LOCTEXT("ChoiceEmpty", "Compile failed: choice node {0} has no options."), FText::FromString(Node->NodeData.Id)), false);
				return;
			}
			for (int32 ChoiceIndex = 0; ChoiceIndex < CompiledNode.Choices.Num(); ++ChoiceIndex)
			{
				CompiledNode.Choices[ChoiceIndex].Target.Reset();
				if (UEdGraphPin* Output = Node->GetFlowOutputPin(ChoiceIndex))
				{
					if (Output->LinkedTo.Num() > 1)
					{
						SetStatus(FText::Format(LOCTEXT("ChoiceMultipleTargets", "Compile failed: option {0} has multiple targets."), FText::FromString(CompiledNode.Choices[ChoiceIndex].TextId)), false);
						return;
					}
					if (Output->LinkedTo.Num() == 1)
					{
						const UTAStoryGraphNode* Target = Cast<UTAStoryGraphNode>(Output->LinkedTo[0]->GetOwningNode());
						if (!Target || Target->NodeType == ETADialogueGraphNodeType::Entry)
						{
							SetStatus(LOCTEXT("ChoiceTargetInvalid", "Compile failed: a choice link points to an invalid node."), false);
							return;
						}
						CompiledNode.Choices[ChoiceIndex].Target = Target->NodeData.Id;
					}
				}
			}
		}
		Compiled.Nodes.Add(MoveTemp(CompiledNode));
	}

	if (!NodeIds.Contains(Compiled.Entry))
	{
		SetStatus(LOCTEXT("EntryTargetNotStoryNode", "Compile failed: Entry target is not part of the story."), false);
		return;
	}
	Compiled.RebuildIndex();
	FText PathError;
	FString ErrorNodeId;
	if (!TAStoryGraphValidation::ValidateEndPaths(Compiled, PathError, ErrorNodeId))
	{
		SetStatus(PathError, false);
		if (GraphEditor.IsValid())
		{
			for (UTAStoryGraphNode* Node : StoryNodes)
			{
				if (Node->NodeData.Id == ErrorNodeId)
				{
					GraphEditor->ClearSelectionSet();
					GraphEditor->SetNodeSelection(Node, true);
					break;
				}
			}
		}
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("CompileStoryTransaction", "Compile Story"));
	StoryAsset->Modify();
	if (StoryAsset->StoryId.IsEmpty())
	{
		StoryAsset->StoryId = Compiled.StoryId;
	}
	StoryAsset->SetCompiledStory(Compiled);
	StoryAsset->MarkPackageDirty();
	SetStatus(FText::Format(LOCTEXT("CompileSucceeded", "Compiled {0} story nodes."), FText::AsNumber(Compiled.Nodes.Num())), true);
	bCompileSucceeded = true;
}

void FTADialogueAssetEditorToolkit::ExtendToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FTADialogueAssetEditorToolkit::CompileStory)),
		NAME_None,
		LOCTEXT("CompileToolbarLabel", "Compile"),
		LOCTEXT("CompileToolbarTooltip", "Compile and validate the story graph."));
	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &FTADialogueAssetEditorToolkit::BuildCompileOptionsMenu),
		LOCTEXT("CompileOptions", "Compile Options"),
		LOCTEXT("CompileOptionsTooltip", "Choose whether compiling also saves this story asset."));
}

TSharedRef<SWidget> FTADialogueAssetEditorToolkit::BuildCompileOptionsMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	auto AddMode = [this, &MenuBuilder](ETADialogueCompileSaveMode Mode, const FText& Label, const FText& Tooltip)
	{
		MenuBuilder.AddMenuEntry(
			Label,
			Tooltip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, Mode]() { CompileSaveMode = Mode; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, Mode]() { return CompileSaveMode == Mode; })),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	};
	AddMode(ETADialogueCompileSaveMode::OnSuccess, LOCTEXT("SaveOnSuccess", "Save on Successful Compile"), LOCTEXT("SaveOnSuccessTooltip", "Save this asset only when compilation succeeds."));
	AddMode(ETADialogueCompileSaveMode::Always, LOCTEXT("AlwaysSave", "Always Save"), LOCTEXT("AlwaysSaveTooltip", "Save this asset after every compile attempt."));
	AddMode(ETADialogueCompileSaveMode::Never, LOCTEXT("NeverSave", "Never Save"), LOCTEXT("NeverSaveTooltip", "Leave saving to the normal Save command."));
	return MenuBuilder.MakeWidget();
}

void FTADialogueAssetEditorToolkit::SetStatus(const FText& Message, bool bSuccess)
{
	StatusMessage = Message;
	bLastCompileSucceeded = bSuccess;
}

#undef LOCTEXT_NAMESPACE
