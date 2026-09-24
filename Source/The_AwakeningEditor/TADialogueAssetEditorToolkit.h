#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorUndoClient.h"

class IDetailsView;
class SGraphEditor;
class UTAStoryAsset;
class UTAStoryEdGraph;
class UTAStoryGraphNode;
class UEdGraphNode;
class FToolBarBuilder;
class SWidget;
struct FEdGraphEditAction;

enum class ETADialogueCompileSaveMode : uint8
{
	Never,
	OnSuccess,
	Always
};

class FTADialogueAssetEditorToolkit : public FAssetEditorToolkit, public FEditorUndoClient
{
public:
	virtual ~FTADialogueAssetEditorToolkit();
	void InitEditor(const TSharedPtr<IToolkitHost>& InToolkitHost, UTAStoryAsset* InAsset);

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual bool MatchesContext(const FTransactionContext& InContext,
		const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	TSharedRef<SDockTab> SpawnMainTab(const FSpawnTabArgs& Args);
	TSharedRef<SWidget> BuildEditorContents();
	void HandleSelectionChanged(const TSet<UObject*>& Selection);
	void HandleGraphChanged(const FEdGraphEditAction& Action);
	void BindGraphCommands();
	void CopySelectedNodes();
	bool CanCopySelectedNodes() const;
	void PasteNodes();
	bool CanPasteNodes() const;
	void DeleteSelectedNodes();
	bool CanDeleteSelectedNodes() const;
	void UndoGraphAction();
	void RedoGraphAction();
	void AddGraphNode(uint8 NodeType);
	void CompileStory();
	void EnsureGraph();
	UTAStoryGraphNode* CreateGraphNode(uint8 NodeType, int32 X, int32 Y);
	void SetStatus(const FText& Message, bool bSuccess);
	void ExtendToolbar(FToolBarBuilder& ToolbarBuilder);
	TSharedRef<SWidget> BuildCompileOptionsMenu();

	UTAStoryAsset* StoryAsset = nullptr;
	UTAStoryEdGraph* StoryGraph = nullptr;

	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<FUICommandList> GraphCommands;
	TSharedPtr<IDetailsView> StoryDetailsView;
	TSharedPtr<IDetailsView> NodeDetailsView;
	FText StatusMessage;
	bool bLastCompileSucceeded = false;
	ETADialogueCompileSaveMode CompileSaveMode = ETADialogueCompileSaveMode::OnSuccess;
	int32 NextNodeY = 100;
	FDelegateHandle GraphChangedHandle;
	static const FName MainTabId;
};
