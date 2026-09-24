#include "TALocalizationEditor.h"
#include "TALocalizationDocument.h"
#include "Core/TALocalizeSubsystem.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "TALocalizationEditor"

namespace
{
	const FName LocalizationTabId(TEXT("TA_LocalizationEditor"));
	FDelegateHandle MenuStartupHandle;
	constexpr float KeyWidth = 280.0f;
	constexpr float LanguageWidth = 320.0f;
	using FRowItem = TSharedPtr<FGuid>;

	class STALocalizationEditor final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(STALocalizationEditor) {} SLATE_END_ARGS()
		void Construct(const FArguments& InArgs);
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
		bool CanClose();
		FText GetTitle() const { return FText::FromString(Document.IsDirty() ? TEXT("本地化 *") : TEXT("本地化")); }

	private:
		FTALocalizationDocument Document;
		TArray<FRowItem> VisibleRows;
		TSharedPtr<SListView<FRowItem>> List;
		TSharedPtr<SBox> HeaderHost;
		TSharedPtr<SScrollBox> HeaderScroll;
		TArray<TWeakPtr<SScrollBox>> RowScrolls;
		TSharedPtr<SEditableTextBox> LanguageInput;
		TSharedPtr<SSearchBox> SearchInput;
		FString Search;
		FString Status;
		bool bMissingOnly = false;
		bool bEditingCell = false;
		FGuid ActiveEditId;
		FString ActiveEditLanguage;
		int32 MissingCount = 0;
		FString ValidationError;
		float HorizontalOffset = 0.0f;
		FTALocalizationRow* FindRow(FGuid Id);
		bool HasMissing(const FTALocalizationRow& Row) const;
		void Refresh(bool bRebuildHeader = false);
		void BuildHeader();
		void ScrollLanguages(float Offset);
		void ChangeCell(FGuid Id, const FString& Language, const FText& Text);
		void EndCellEdit(FGuid Id, const FString& Language);
		void UpdateSummary();
		TSharedRef<ITableRow> GenerateRow(FRowItem Item, const TSharedRef<STableViewBase>& Owner);
		TSharedRef<SWidget> Button(const FText& Label, const FText& Tooltip, TFunction<void()> Action, TAttribute<bool> Enabled = true);
		bool Save();
		void Reload();
		void AddRow();
		void DeleteRows();
		void AddLanguage();
		void Undo();
		void Redo();
		void Copy();
		void Paste();
		void Import();
		void Export();
		bool ChooseFile(bool bSave, FString& OutPath);
		FText GetStatus() const;
	};

	TSharedRef<SWidget> STALocalizationEditor::Button(const FText& Label, const FText& Tooltip, TFunction<void()> Action, TAttribute<bool> Enabled)
	{
		return SNew(SButton).Text(Label).ToolTipText(Tooltip).IsEnabled(Enabled)
			.OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); });
	}

	void STALocalizationEditor::Construct(const FArguments& InArgs)
	{
		Document.Load(GetDefault<UTALocalizeSubsystem>()->GetLocalizationDirectory(), Status);
		const auto Loaded = TAttribute<bool>::CreateLambda([this]() { return Document.IsLoaded(); });
		const auto Selected = TAttribute<bool>::CreateLambda([this]() { return List.IsValid() && List->GetNumItemsSelected() > 0; });
		ChildSlot
		[
			SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(SScrollBox).Orientation(Orient_Horizontal).AllowOverscroll(EAllowOverscroll::No)
					+ SScrollBox::Slot()
					[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Save", "保存"), LOCTEXT("SaveHelp", "保存所有语言（Ctrl+S）。空翻译在游戏中显示 Key。"), [this]() { Save(); }, Loaded)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Reload", "重新载入"), LOCTEXT("ReloadHelp", "从项目的本地化文件重新读取。"), [this]() { Reload(); })]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Undo", "撤销"), LOCTEXT("UndoHelp", "撤销上一次表格修改。编辑文本时 Ctrl+Z 撤销文本输入。"), [this]() { Undo(); }, TAttribute<bool>::CreateLambda([this]() { return Document.CanUndo(); }))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Redo", "重做"), LOCTEXT("RedoHelp", "重做上一次表格修改。"), [this]() { Redo(); }, TAttribute<bool>::CreateLambda([this]() { return Document.CanRedo(); }))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Add", "新增 Key"), LOCTEXT("AddHelp", "添加一个所有语言共用的 Key。"), [this]() { AddRow(); }, Loaded)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Delete", "删除选中行"), LOCTEXT("DeleteHelp", "删除选中 Key 的所有语言文本，可以撤销。"), [this]() { DeleteRows(); }, Selected)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Copy", "复制行"), LOCTEXT("CopyHelp", "复制选中行（含表头），可粘贴到电子表格。未选中时复制全部行。"), [this]() { Copy(); }, Loaded)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Paste", "粘贴表格"), LOCTEXT("PasteHelp", "粘贴带 Key / 语言代码表头的表格。相同 Key 合并，空单元格清空对应翻译。"), [this]() { Paste(); }, Loaded)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Import", "导入 CSV"), LOCTEXT("ImportHelp", "按 Key 合并 CSV，只修改导入的行和语言。保存前可撤销。"), [this]() { Import(); }, Loaded)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)[Button(LOCTEXT("Export", "导出 CSV"), LOCTEXT("ExportHelp", "导出全部 Key 和语言，包括尚未保存的修改。"), [this]() { Export(); }, Loaded)]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2, 0, 2, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)
					[
						SAssignNew(SearchInput, SSearchBox).HintText(LOCTEXT("Search", "搜索 Key 或任意语言文本"))
						.OnTextChanged_Lambda([this](const FText& Text) { Search = Text.ToString(); Refresh(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 16, 0)
					[
						SNew(SCheckBox).OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bMissingOnly = State == ECheckBoxState::Checked; Refresh(); })
						[SNew(STextBlock).Text(LOCTEXT("MissingOnly", "仅显示缺失翻译"))]
					]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(130)[SAssignNew(LanguageInput, SEditableTextBox).HintText(LOCTEXT("Language", "语言代码，如 ja"))]]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[Button(LOCTEXT("AddLanguage", "添加语言"), LOCTEXT("LanguageHelp", "使用 zh-CN、en、ja 等语言代码，新增一列。"), [this]() { AddLanguage(); }, Loaded)]
				]
				+ SVerticalBox::Slot().AutoHeight()[SAssignNew(HeaderHost, SBox)]
				+ SVerticalBox::Slot().FillHeight(1)
				[
					SAssignNew(List, SListView<FRowItem>).ListItemsSource(&VisibleRows).SelectionMode(ESelectionMode::Multi)
					.OnGenerateRow(this, &STALocalizationEditor::GenerateRow)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(4, 8, 4, 0)[SNew(STextBlock).AutoWrapText(true).Text(this, &STALocalizationEditor::GetStatus)]
			]
		];
		Refresh(true);
	}

	FTALocalizationRow* STALocalizationEditor::FindRow(FGuid Id)
	{
		return Document.Table.Rows.FindByPredicate([Id](const FTALocalizationRow& Row) { return Row.Id == Id; });
	}

	bool STALocalizationEditor::HasMissing(const FTALocalizationRow& Row) const
	{
		for (const FString& Language : Document.Table.Languages) { if (Row.Texts.FindRef(Language).TrimStartAndEnd().IsEmpty()) { return true; } }
		return false;
	}

	void STALocalizationEditor::Refresh(bool bRebuildHeader)
	{
		if (!List) { return; }
		bEditingCell = false;
		UpdateSummary();
		TSet<FGuid> Selection;
		for (const FRowItem& Item : List->GetSelectedItems()) { Selection.Add(*Item); }
		VisibleRows.Reset();
		for (const FTALocalizationRow& Row : Document.Table.Rows)
		{
			if (bMissingOnly && !HasMissing(Row)) { continue; }
			bool bMatches = Search.IsEmpty() || Row.Key.Contains(Search);
			for (const auto& Pair : Row.Texts) { bMatches |= Pair.Value.Contains(Search); }
			if (bMatches) { VisibleRows.Add(MakeShared<FGuid>(Row.Id)); }
		}
		List->ClearSelection();
		for (const FRowItem& Item : VisibleRows) { if (Selection.Contains(*Item)) { List->SetItemSelection(Item, true); } }
		RowScrolls.RemoveAll([](const TWeakPtr<SScrollBox>& Item) { return !Item.IsValid(); });
		if (bRebuildHeader) { BuildHeader(); }
		List->RebuildList();
	}

	void STALocalizationEditor::BuildHeader()
	{
		TSharedRef<SHorizontalBox> Languages = SNew(SHorizontalBox);
		for (const FString& Language : Document.Table.Languages)
		{
			Languages->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(LanguageWidth)[SNew(SBorder).Padding(8)[SNew(STextBlock).Text(FText::FromString(Language)).Font(FAppStyle::GetFontStyle("BoldFont"))]]];
		}
		HeaderHost->SetContent(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(KeyWidth)[SNew(SBorder).Padding(8)[SNew(STextBlock).Text(LOCTEXT("KeyHeader", "Key")).Font(FAppStyle::GetFontStyle("BoldFont"))]]]
			+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 16, 0)
			[
				SAssignNew(HeaderScroll, SScrollBox).Orientation(Orient_Horizontal).ScrollBarAlwaysVisible(true)
				.AllowOverscroll(EAllowOverscroll::No).OnUserScrolled(this, &STALocalizationEditor::ScrollLanguages)
				+ SScrollBox::Slot()[Languages]
			]);
		HeaderScroll->SetScrollOffset(HorizontalOffset);
	}

	void STALocalizationEditor::ScrollLanguages(float Offset)
	{
		HorizontalOffset = Offset;
		if (HeaderScroll) { HeaderScroll->SetScrollOffset(Offset); }
		for (const TWeakPtr<SScrollBox>& Weak : RowScrolls) { if (TSharedPtr<SScrollBox> Scroll = Weak.Pin()) { Scroll->SetScrollOffset(Offset); } }
	}

	void STALocalizationEditor::UpdateSummary()
	{
		MissingCount = 0;
		for (const FTALocalizationRow& Row : Document.Table.Rows)
		{
			for (const FString& Language : Document.Table.Languages) { MissingCount += Row.Texts.FindRef(Language).TrimStartAndEnd().IsEmpty() ? 1 : 0; }
		}
		ValidationError.Reset(); Document.Validate(ValidationError);
	}

	void STALocalizationEditor::ChangeCell(FGuid Id, const FString& Language, const FText& Text)
	{
		FTALocalizationRow* Row = FindRow(Id);
		if (!Row) { return; }
		const FString Value = Text.ToString();
		if ((Language.IsEmpty() ? Row->Key : Row->Texts.FindRef(Language)) == Value) { return; }
		// Store edits immediately so recycling a scrolled-off row never loses input.
		// One focus/edit session forms a single document undo step.
		if (!bEditingCell || ActiveEditId != Id || ActiveEditLanguage != Language)
		{
			Document.BeginChange(); bEditingCell = true; ActiveEditId = Id; ActiveEditLanguage = Language;
		}
		if (Language.IsEmpty()) { Row->Key = Value; Status = TEXT("Key 已修改；已有剧情、物品和 UI 引用需要同步修改。"); }
		else { Row->Texts.Add(Language, Value); Status.Reset(); }
		UpdateSummary();
	}

	void STALocalizationEditor::EndCellEdit(FGuid Id, const FString& Language)
	{
		if (bEditingCell && ActiveEditId == Id && ActiveEditLanguage == Language)
		{
			bEditingCell = false;
			if (!Search.IsEmpty() || bMissingOnly) { Refresh(); }
		}
	}

	TSharedRef<ITableRow> STALocalizationEditor::GenerateRow(FRowItem Item, const TSharedRef<STableViewBase>& Owner)
	{
		const FGuid Id = *Item;
		TSharedRef<SHorizontalBox> Cells = SNew(SHorizontalBox);
		for (const FString& Language : Document.Table.Languages)
		{
			Cells->AddSlot().AutoWidth()
			[
				SNew(SBox).WidthOverride(LanguageWidth).Padding(2)
				[
					SNew(SMultiLineEditableTextBox).AutoWrapText(true)
					.Text_Lambda([this, Id, Language]() { const FTALocalizationRow* Row = FindRow(Id); return FText::FromString(Row ? Row->Texts.FindRef(Language) : FString()); })
					.HintText(LOCTEXT("Missing", "缺失翻译"))
					.BackgroundColor_Lambda([this, Id, Language]() { const FTALocalizationRow* Row = FindRow(Id); return Row && Row->Texts.FindRef(Language).TrimStartAndEnd().IsEmpty() ? FLinearColor(0.18f, 0.10f, 0.025f) : FLinearColor(0.035f, 0.035f, 0.035f); })
					.OnTextChanged_Lambda([this, Id, Language](const FText& Text) { ChangeCell(Id, Language, Text); })
					.OnTextCommitted_Lambda([this, Id, Language](const FText& Text, ETextCommit::Type Commit) { EndCellEdit(Id, Language); })
				]
			];
		}
		TSharedPtr<SScrollBox> Scroll;
		TSharedRef<ITableRow> Result = SNew(STableRow<FRowItem>, Owner).Padding(0)
		[
			SNew(SBox).HeightOverride(92)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(KeyWidth).Padding(2)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0, 6, 4, 0)
						[
							SNew(SCheckBox).IsChecked_Lambda([this, Item]() { return List->IsItemSelected(Item) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this, Item](ECheckBoxState State) { List->SetItemSelection(Item, State == ECheckBoxState::Checked); })
						]
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Top)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this, Id]() { const FTALocalizationRow* Row = FindRow(Id); return FText::FromString(Row ? Row->Key : FString()); })
							.ToolTipText(LOCTEXT("KeyHelp", "Key 必须唯一。修改 Key 后，需要同步修改使用它的剧情、物品或 UI 引用。"))
							.OnTextChanged_Lambda([this, Id](const FText& Text) { ChangeCell(Id, FString(), Text); })
							.OnTextCommitted_Lambda([this, Id](const FText& Text, ETextCommit::Type Commit) { EndCellEdit(Id, FString()); })
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					SAssignNew(Scroll, SScrollBox).Orientation(Orient_Horizontal).ScrollBarVisibility(EVisibility::Collapsed)
					.ConsumeMouseWheel(EConsumeMouseWheel::Never).AllowOverscroll(EAllowOverscroll::No)
					.OnUserScrolled(this, &STALocalizationEditor::ScrollLanguages)
					+ SScrollBox::Slot()[Cells]
				]
			]
		];
		Scroll->SetScrollOffset(HorizontalOffset); RowScrolls.Add(Scroll);
		return Result;
	}

	bool STALocalizationEditor::Save()
	{
		bEditingCell = false;
		FString Error;
		if (!Document.Save(Error)) { Status = Error; FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error)); return false; }
		Status = TEXT("所有语言已保存。游戏下次载入语言时生效。");
		return true;
	}

	bool STALocalizationEditor::CanClose()
	{
		// Finish the currently edited cell before checking whether the document is dirty.
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
		if (!Document.IsDirty()) { return true; }
		const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNoCancel, LOCTEXT("Unsaved", "本地化有未保存的修改。是否保存？\n是：保存并关闭；否：放弃修改；取消：继续编辑。"));
		return Answer == EAppReturnType::No || (Answer == EAppReturnType::Yes && Save());
	}

	void STALocalizationEditor::Reload()
	{
		if (Document.IsDirty() && FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ReloadConfirm", "重新载入会放弃未保存的修改，是否继续？")) != EAppReturnType::Yes) { return; }
		if (Document.Load(GetDefault<UTALocalizeSubsystem>()->GetLocalizationDirectory(), Status)) { Status = TEXT("已重新载入。"); Refresh(true); }
	}

	void STALocalizationEditor::AddRow()
	{
		Document.BeginChange();
		FString Key = TEXT("New_Key");
		for (int32 Index = 1; Document.Table.Rows.ContainsByPredicate([&Key](const FTALocalizationRow& Row) { return Row.Key == Key; }); ++Index) { Key = FString::Printf(TEXT("New_Key_%d"), Index); }
		FTALocalizationRow& Row = Document.Table.Rows.AddDefaulted_GetRef(); Row.Key = Key;
		const FGuid Id = Row.Id;
		SearchInput->SetText(FText::GetEmpty()); Refresh();
		for (const FRowItem& Item : VisibleRows) { if (*Item == Id) { List->SetSelection(Item); List->RequestScrollIntoView(Item); break; } }
		Status = TEXT("已新增 Key。");
	}

	void STALocalizationEditor::DeleteRows()
	{
		TSet<FGuid> Selection;
		for (const FRowItem& Item : List->GetSelectedItems()) { Selection.Add(*Item); }
		if (Selection.IsEmpty()) { return; }
		Document.BeginChange(); Document.Table.Rows.RemoveAll([&Selection](const FTALocalizationRow& Row) { return Selection.Contains(Row.Id); });
		Status = TEXT("已删除选中行，可撤销。请同步移除使用这些 Key 的引用。"); Refresh();
	}

	void STALocalizationEditor::AddLanguage()
	{
		const FString Language = LanguageInput->GetText().ToString().TrimStartAndEnd();
		if (!FTALocalizationDocument::IsValidLanguage(Language)) { Status = TEXT("请输入有效的语言代码，例如 zh-CN、en、ja。"); return; }
		if (Document.Table.Languages.ContainsByPredicate([&Language](const FString& Existing) { return Existing.Equals(Language, ESearchCase::IgnoreCase); })) { Status = TEXT("该语言已经存在。"); return; }
		Document.BeginChange(); Document.Table.Languages.Add(Language); LanguageInput->SetText(FText::GetEmpty());
		Status = TEXT("已添加语言：") + Language; Refresh(true);
	}

	void STALocalizationEditor::Undo() { if (Document.Undo()) { Status = TEXT("已撤销。"); Refresh(true); } }
	void STALocalizationEditor::Redo() { if (Document.Redo()) { Status = TEXT("已重做。"); Refresh(true); } }

	void STALocalizationEditor::Copy()
	{
		TSet<FGuid> Selection;
		for (const FRowItem& Item : List->GetSelectedItems()) { Selection.Add(*Item); }
		FPlatformApplicationMisc::ClipboardCopy(*Document.ExportDelimited(TEXT('\t'), Selection.IsEmpty() ? nullptr : &Selection));
		Status = TEXT("已复制表格（含表头），可粘贴到电子表格。");
	}

	void STALocalizationEditor::Paste()
	{
		FString Source; FPlatformApplicationMisc::ClipboardPaste(Source);
		if (Document.ImportDelimited(Source, TEXT('\t'), Status)) { Status = TEXT("已按 Key 合并粘贴的表格，可撤销。尚未保存。"); Refresh(true); }
	}

	bool STALocalizationEditor::ChooseFile(bool bSave, FString& OutPath)
	{
		IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
		if (!Desktop) { Status = TEXT("无法打开文件选择器。"); return false; }
		TArray<FString> Paths;
		const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
		const bool bChosen = bSave
			? Desktop->SaveFileDialog(Parent, TEXT("导出本地化"), FPaths::ProjectDir(), TEXT("Localization.csv"), TEXT("CSV (*.csv)|*.csv"), 0, Paths)
			: Desktop->OpenFileDialog(Parent, TEXT("导入本地化"), FPaths::ProjectDir(), TEXT(""), TEXT("CSV (*.csv)|*.csv"), 0, Paths);
		if (!bChosen || Paths.IsEmpty()) { return false; }
		OutPath = Paths[0];
		if (bSave && FPaths::GetExtension(OutPath).IsEmpty()) { OutPath += TEXT(".csv"); }
		return true;
	}

	void STALocalizationEditor::Import()
	{
		FString Path; if (!ChooseFile(false, Path)) { return; }
		FString Source;
		if (!FFileHelper::LoadFileToString(Source, *Path)) { Status = TEXT("无法读取：") + Path; return; }
		if (Document.ImportDelimited(Source, TEXT(','), Status)) { Status = TEXT("已按 Key 合并 CSV，可撤销。尚未保存。"); Refresh(true); }
	}

	void STALocalizationEditor::Export()
	{
		FString Path; if (!ChooseFile(true, Path)) { return; }
		Status = FFileHelper::SaveStringToFile(Document.ExportDelimited(TEXT(',')), *Path, FFileHelper::EEncodingOptions::ForceUTF8)
			? TEXT("已导出：") + Path : TEXT("导出失败：") + Path;
	}

	FText STALocalizationEditor::GetStatus() const
	{
		return FText::FromString(FString::Printf(TEXT("%d / %d 行  ·  %d 种语言  ·  %d 处缺失翻译%s\n%s"), VisibleRows.Num(), Document.Table.Rows.Num(), Document.Table.Languages.Num(), MissingCount, Document.IsDirty() ? TEXT("  ·  未保存") : TEXT(""), *(ValidationError.IsEmpty() ? Status : ValidationError)));
	}

	FReply STALocalizationEditor::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
	{
		if (Event.IsControlDown() && Document.IsLoaded())
		{
			if (Event.GetKey() == EKeys::S) { FSlateApplication::Get().SetKeyboardFocus(AsShared()); Save(); return FReply::Handled(); }
			if (Event.GetKey() == EKeys::Z) { if (Event.IsShiftDown()) { Redo(); } else { Undo(); } return FReply::Handled(); }
			if (Event.GetKey() == EKeys::Y) { Redo(); return FReply::Handled(); }
			if (Event.GetKey() == EKeys::C) { Copy(); return FReply::Handled(); }
			if (Event.GetKey() == EKeys::V) { Paste(); return FReply::Handled(); }
		}
		return SCompoundWidget::OnKeyDown(Geometry, Event);
	}

	TSharedRef<SDockTab> SpawnLocalizationTab(const FSpawnTabArgs& Args)
	{
		TSharedRef<STALocalizationEditor> Editor = SNew(STALocalizationEditor);
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)
			.Label(TAttribute<FText>::CreateSP(&Editor.Get(), &STALocalizationEditor::GetTitle))
			.OnCanCloseTab(SDockTab::FCanCloseTab::CreateSP(Editor, &STALocalizationEditor::CanClose))
			[Editor];
	}

	void RegisterMenu()
	{
		FToolMenuOwnerScoped Owner(LocalizationTabId);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		Menu->FindOrAddSection(TEXT("TheAwakening"), LOCTEXT("Project", "The Awakening")).AddMenuEntry(
			LocalizationTabId, LOCTEXT("Menu", "本地化编辑器"), LOCTEXT("MenuHelp", "并列编辑项目中所有语言的 Key 和文本。"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"),
			FUIAction(FExecuteAction::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(LocalizationTabId); })));
	}
}

void TALocalizationEditor::Register()
{
	if (IsRunningCommandlet()) { return; }
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LocalizationTabId, FOnSpawnTab::CreateStatic(&SpawnLocalizationTab))
		.SetDisplayName(LOCTEXT("Tab", "本地化编辑器")).SetMenuType(ETabSpawnerMenuType::Hidden);
	MenuStartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenu));
}

void TALocalizationEditor::Unregister()
{
	if (IsRunningCommandlet()) { return; }
	UToolMenus::UnRegisterStartupCallback(MenuStartupHandle);
	UToolMenus::UnregisterOwner(LocalizationTabId);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LocalizationTabId);
}

#undef LOCTEXT_NAMESPACE
