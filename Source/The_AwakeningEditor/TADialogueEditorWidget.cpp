// Source/The_AwakeningEditor/TADialogueEditorWidget.cpp
#include "TADialogueEditorWidget.h"
#include "TADialogueLocalizationHelper.h"
#include "Story/TADialogueSubsystem.h"
#include "Story/TADialogueController.h"
#include "Story/TADialogueWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"
#include "Editor.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/Border.h"
#include "Styling/AppStyle.h"
#include "Types/SlateEnums.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UTADialogueRowCombo::HandleGenerateWidget(TSharedPtr<FString> Item) const
{
	const FString ItemText = Item.IsValid() ? *Item : FString();
	return SNew(STextBlock)
		.Text(FText::FromString(ItemText))
		.Font(GetFont());
}

namespace
{
	constexpr float DialogueEditorGeneratedPadding = 4.0f;

	FSlateFontInfo MakeDialogueEditorFont(bool bBold = false, bool bCategory = false)
	{
		const FName StyleName = bCategory
			? FName(TEXT("DetailsView.CategoryFontStyle"))
			: (bBold ? FName(TEXT("PropertyWindow.BoldFont")) : FName(TEXT("PropertyWindow.NormalFont")));
		return FAppStyle::GetFontStyle(StyleName);
	}

	void SetDialogueEditorFont(UWidget* Widget, bool bBold = false, bool bCategory = false)
	{
		const FSlateFontInfo Font = MakeDialogueEditorFont(bBold, bCategory);
		const FTextBlockStyle& NormalTextStyle = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>(
			bCategory ? TEXT("DetailsView.CategoryTextStyle") : TEXT("NormalText"));

		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			Text->SetFont(Font);
			Text->SetColorAndOpacity(NormalTextStyle.ColorAndOpacity);
		}
		else if (UEditableTextBox* TextBox = Cast<UEditableTextBox>(Widget))
		{
			TextBox->WidgetStyle = FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
			TextBox->WidgetStyle.SetFont(Font);
		}
		else if (UMultiLineEditableTextBox* MultiLineBox = Cast<UMultiLineEditableTextBox>(Widget))
		{
			MultiLineBox->WidgetStyle = FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
			FTextBlockStyle TextStyle = MultiLineBox->WidgetStyle.TextStyle;
			TextStyle.SetFont(Font);
			MultiLineBox->SetTextStyle(TextStyle);
		}
		else if (UTADialogueRowCombo* Combo = Cast<UTADialogueRowCombo>(Widget))
		{
			Combo->SetRowFont(Font);
			Combo->SetRowForegroundColor(FSlateColor(FLinearColor::Black));

			FTableRowStyle ItemStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("DetailsView.TreeView.TableRow"));
			const FSlateColorBrush SelectedItemBrush(FAppStyle::Get().GetSlateColor(TEXT("Colors.AccentBlue")));
			ItemStyle.SetActiveBrush(SelectedItemBrush)
				.SetActiveHoveredBrush(SelectedItemBrush)
				.SetInactiveBrush(SelectedItemBrush)
				.SetInactiveHoveredBrush(SelectedItemBrush)
				.SetSelectedTextColor(FAppStyle::Get().GetSlateColor(TEXT("Colors.ForegroundInverted")));
			Combo->SetItemStyle(ItemStyle);
		}
	}

	void AddWidgetWithPadding(UPanelWidget* Parent, UWidget* Child)
	{
		if (!Parent || !Child)
		{
			return;
		}

		Parent->AddChild(Child);
		const FMargin Padding(DialogueEditorGeneratedPadding);

		if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Child->Slot))
		{
			VerticalSlot->SetPadding(Padding);
		}
		else if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(Child->Slot))
		{
			HorizontalSlot->SetPadding(Padding);
		}
		else if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Child->Slot))
		{
			ButtonSlot->SetPadding(Padding);
		}
	}
}

namespace
{
	/** 对象路径规范化：/Game/A/B → /Game/A/B.B */
	FString NormalizeObjectPath(const FString& Path)
	{
		if (Path.IsEmpty() || Path.Contains(TEXT(".")))
		{
			return Path;
		}
		return Path + TEXT(".") + FPaths::GetBaseFilename(Path);
	}

	/** 可选的下拉选项："（结束）" + 全部节点 ID */
	TArray<FString> BuildNodeOptions(const FTAStoryData& Story)
	{
		TArray<FString> Options;
		Options.Add(TEXT(""));
		for (const FTAStoryNode& Node : Story.Nodes)
		{
			Options.Add(Node.Id);
		}
		return Options;
	}
}

// ============================================================
// 生命周期
// ============================================================

void UTADialogueEditorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_NewStory) Button_NewStory->OnClicked.AddDynamic(this, &UTADialogueEditorWidget::NewStory);
	if (Button_Save)     Button_Save->OnClicked.AddDynamic(this, &UTADialogueEditorWidget::SaveStory);
	if (Button_Validate) Button_Validate->OnClicked.AddDynamic(this, &UTADialogueEditorWidget::ValidateStory);
	if (Button_Preview)  Button_Preview->OnClicked.AddDynamic(this, &UTADialogueEditorWidget::TogglePreview);
	SetDialogueEditorFont(Text_Status);

	RebuildSimulationPanel();
	ScanTextures();
	RefreshStories();
}

// ============================================================
// 工具栏
// ============================================================

void UTADialogueEditorWidget::RefreshStories()
{
	UTADialogueSubsystem* Sub = GetDialogueSubsystem();
	const FString Folder = FPaths::ProjectContentDir() / (Sub ? Sub->StoriesFolder : TEXT("Stories"));

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Folder / TEXT("*.json")), true, false);
	Files.Sort();

	if (!ScrollBox_Stories)
	{
		return;
	}
	ScrollBox_Stories->ClearChildren();

	for (const FString& File : Files)
	{
		const FString StoryId = FPaths::GetBaseFilename(File);
		UTADialogueRowButton* B = AddButtonRow(ScrollBox_Stories, StoryId);
		if (B)
		{
			B->OnClickedNative.AddLambda([this, StoryId]() { SelectStory(StoryId); });
		}
	}

	SetStatus(FString::Printf(TEXT("共 %d 个剧情文件（%s）"), Files.Num(), *Folder));
}

void UTADialogueEditorWidget::NewStory()
{
	UTADialogueSubsystem* Sub = GetDialogueSubsystem();
	const FString Folder = FPaths::ProjectContentDir() / (Sub ? Sub->StoriesFolder : TEXT("Stories"));

	FString NewId;
	for (int32 i = 1; ; ++i)
	{
		NewId = FString::Printf(TEXT("NewStory_%d"), i);
		if (!FPaths::FileExists(Folder / (NewId + TEXT(".json"))))
		{
			break;
		}
	}

	FTAStoryData NewStory;
	NewStory.StoryId = NewId;
	NewStory.Entry = TEXT("start");

	FTAStoryNode StartNode;
	StartNode.Id = TEXT("start");
	StartNode.Type = TEXT("dialogue");
	StartNode.TextId = FString::Printf(TEXT("%s_001"), *NewId);
	StartNode.Next = TEXT("end");

	FTAStoryNode EndNode;
	EndNode.Id = TEXT("end");
	EndNode.Type = TEXT("end");

	NewStory.Nodes.Add(MoveTemp(StartNode));
	NewStory.Nodes.Add(MoveTemp(EndNode));
	NewStory.RebuildIndex();

	FString Json;
	if (UTADialogueSubsystem::StoryToJson(NewStory, Json))
	{
		FFileHelper::SaveStringToFile(Json, *(Folder / (NewId + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	RefreshStories();
	SelectStory(NewId);
	SetStatus(FString::Printf(TEXT("已创建 %s.json"), *NewId));
}

void UTADialogueEditorWidget::SaveStory()
{
	if (EditingStoryId.IsEmpty())
	{
		SetStatus(TEXT("没有打开的剧情"));
		return;
	}

	EditingStory.RebuildIndex();

	FString Json;
	if (!UTADialogueSubsystem::StoryToJson(EditingStory, Json))
	{
		SetStatus(TEXT("序列化失败"));
		return;
	}

	UTADialogueSubsystem* Sub = GetDialogueSubsystem();
	const FString Folder = FPaths::ProjectContentDir() / (Sub ? Sub->StoriesFolder : TEXT("Stories"));
	if (FFileHelper::SaveStringToFile(Json, *(Folder / (EditingStoryId + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		SetStatus(FString::Printf(TEXT("已保存 %s.json"), *EditingStoryId));
	}
	else
	{
		SetStatus(TEXT("保存失败"));
	}
}

void UTADialogueEditorWidget::ValidateStory()
{
	TArray<FString> Errors;
	CollectValidationErrors(Errors);

	if (Errors.Num() == 0)
	{
		SetStatus(TEXT("校验通过 ✓"));
		return;
	}

	FString Msg = FString::Printf(TEXT("发现 %d 个问题：\n"), Errors.Num());
	for (const FString& E : Errors)
	{
		Msg += TEXT("• ") + E + TEXT("\n");
	}
	SetStatus(Msg);
}

void UTADialogueEditorWidget::TogglePreview()
{
	if (bPreviewActive)
	{
		StopPreview();
	}
	else
	{
		StartPreview();
	}
}

void UTADialogueEditorWidget::ApplySimulation()
{
	if (PreviewController)
	{
		PreviewController->SetConditionOverrides(BuildPreviewOverrides());
		SetStatus(TEXT("模拟状态已应用"));
	}
	else
	{
		SetStatus(TEXT("请先开始预览"));
	}
}

// ============================================================
// 列表
// ============================================================

void UTADialogueEditorWidget::SelectStory(const FString& StoryId)
{
	UTADialogueSubsystem* Sub = GetDialogueSubsystem();
	const FString Folder = FPaths::ProjectContentDir() / (Sub ? Sub->StoriesFolder : TEXT("Stories"));
	const FString FilePath = Folder / (StoryId + TEXT(".json"));

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		SetStatus(FString::Printf(TEXT("打开失败: %s"), *FilePath));
		return;
	}

	FTAStoryData Parsed;
	FString Error;
	if (!UTADialogueSubsystem::ParseStoryJson(JsonString, StoryId, Parsed, Error))
	{
		SetStatus(FString::Printf(TEXT("解析失败 %s: %s"), *StoryId, *Error));
		return;
	}

	StopPreview();

	EditingStory = MoveTemp(Parsed);
	EditingStoryId = StoryId;

	const int32 EntryIdx = EditingStory.FindNodeIndex(EditingStory.Entry);
	SelectedNodeIndex = EntryIdx != INDEX_NONE ? EntryIdx : 0;

	RebuildNodeList();
	RebuildDetails();
	SetStatus(FString::Printf(TEXT("已打开 %s（%d 个节点）"), *StoryId, EditingStory.Nodes.Num()));
}

void UTADialogueEditorWidget::SelectNode(int32 NodeIndex)
{
	SelectedNodeIndex = NodeIndex;
	RebuildDetails();
}

void UTADialogueEditorWidget::RebuildNodeList()
{
	if (!ScrollBox_Nodes)
	{
		return;
	}
	ScrollBox_Nodes->ClearChildren();

	// 添加 / 删除节点按钮
	UTADialogueRowButton* AddBtn = AddButtonRow(ScrollBox_Nodes, TEXT("＋ 添加节点"));
	if (AddBtn)
	{
		AddBtn->OnClickedNative.AddLambda([this]()
		{
			FString NewId;
			for (int32 i = 1; ; ++i)
			{
				NewId = FString::Printf(TEXT("node_%d"), i);
				if (EditingStory.FindNodeIndex(NewId) == INDEX_NONE)
				{
					break;
				}
			}
			FTAStoryNode NewNode;
			NewNode.Id = NewId;
			NewNode.Type = TEXT("dialogue");
			EditingStory.Nodes.Add(NewNode);
			EditingStory.RebuildIndex();
			SelectedNodeIndex = EditingStory.Nodes.Num() - 1;
			RebuildNodeList();
			RebuildDetails();
		});
	}

	UTADialogueRowButton* RemoveBtn = AddButtonRow(ScrollBox_Nodes, TEXT("✕ 删除选中节点"));
	if (RemoveBtn)
	{
		RemoveBtn->OnClickedNative.AddLambda([this]()
		{
			if (!EditingStory.Nodes.IsValidIndex(SelectedNodeIndex))
			{
				SetStatus(TEXT("未选中节点"));
				return;
			}
			if (EditingStory.Nodes[SelectedNodeIndex].Id == EditingStory.Entry)
			{
				SetStatus(TEXT("入口节点不能删除（先改 entry 或换入口）"));
				return;
			}
			EditingStory.Nodes.RemoveAt(SelectedNodeIndex);
			EditingStory.RebuildIndex();
			SelectedNodeIndex = FMath::Min(SelectedNodeIndex, EditingStory.Nodes.Num() - 1);
			RebuildNodeList();
			RebuildDetails();
		});
	}

	// 节点按钮
	for (int32 i = 0; i < EditingStory.Nodes.Num(); ++i)
	{
		const FString Label = FString::Printf(TEXT("%s%s  %s"),
			EditingStory.Nodes[i].Id == EditingStory.Entry ? TEXT("▶ ") : TEXT(""),
			*EditingStory.Nodes[i].Id,
			*EditingStory.Nodes[i].Type);
		UTADialogueRowButton* B = AddButtonRow(ScrollBox_Nodes, Label);
		if (B)
		{
			const int32 Idx = i;
			B->OnClickedNative.AddLambda([this, Idx]() { SelectNode(Idx); });
		}
	}
}

// ============================================================
// 详情面板
// ============================================================

void UTADialogueEditorWidget::RebuildDetails()
{
	if (!Box_Details)
	{
		return;
	}
	Box_Details->ClearChildren();

	if (!EditingStory.Nodes.IsValidIndex(SelectedNodeIndex))
	{
		SetStatus(TEXT("未选中节点"));
		return;
	}

	const int32 Idx = SelectedNodeIndex;
	FTAStoryNode& Node = EditingStory.Nodes[Idx];

	AddSectionTitle(Box_Details, FString::Printf(TEXT("节点 %s"), *Node.Id));

	// ---- 基础 ----
	AddTextBoxRow(Box_Details, TEXT("ID"), Node.Id)->OnTextChangedNative.AddLambda([this, Idx](const FText& T)
	{
		if (EditingStory.Nodes.IsValidIndex(Idx))
		{
			EditingStory.Nodes[Idx].Id = T.ToString();
			EditingStory.RebuildIndex();
		}
	});

	AddComboRow(Box_Details, TEXT("类型"), { TEXT("dialogue"), TEXT("choice"), TEXT("end") }, Node.Type)
		->OnSelectionNative.AddLambda([this, Idx](FString Selected, ESelectInfo::Type)
	{
		if (!EditingStory.Nodes.IsValidIndex(Idx) || Selected.IsEmpty())
		{
			return;
		}
		EditingStory.Nodes[Idx].Type = Selected;
		RebuildNodeList();
		RebuildDetails();
	});

	if (Node.Type == TEXT("dialogue"))
	{
		AddSectionTitle(Box_Details, TEXT("对话"));

		AddTextBoxRow(Box_Details, TEXT("speakerId"), Node.SpeakerId)->OnTextChangedNative.AddLambda([this, Idx](const FText& T)
		{
			if (EditingStory.Nodes.IsValidIndex(Idx)) EditingStory.Nodes[Idx].SpeakerId = T.ToString();
		});

		AddTextBoxRow(Box_Details, TEXT("speakerNameId"), Node.SpeakerNameId)->OnTextChangedNative.AddLambda([this, Idx](const FText& T)
		{
			if (EditingStory.Nodes.IsValidIndex(Idx)) EditingStory.Nodes[Idx].SpeakerNameId = T.ToString();
		});

		UTADialogueRowButton* NameLocBtn = AddButtonRow(Box_Details, TEXT("✎ 编辑本地化（speakerNameId）"));
		if (NameLocBtn)
		{
			NameLocBtn->OnClickedNative.AddLambda([this, Idx]()
			{
				if (EditingStory.Nodes.IsValidIndex(Idx)) OpenLocalizeKey(EditingStory.Nodes[Idx].SpeakerNameId);
			});
		}

		AddTextBoxRow(Box_Details, TEXT("textId"), Node.TextId)->OnTextChangedNative.AddLambda([this, Idx](const FText& T)
		{
			if (EditingStory.Nodes.IsValidIndex(Idx)) EditingStory.Nodes[Idx].TextId = T.ToString();
		});

		UTADialogueRowButton* TextLocBtn = AddButtonRow(Box_Details, TEXT("✎ 编辑本地化（textId）"));
		if (TextLocBtn)
		{
			TextLocBtn->OnClickedNative.AddLambda([this, Idx]()
			{
				if (EditingStory.Nodes.IsValidIndex(Idx)) OpenLocalizeKey(EditingStory.Nodes[Idx].TextId);
			});
		}

		AddComboRow(Box_Details, TEXT("next"), BuildNodeOptions(EditingStory), Node.Next)
			->OnSelectionNative.AddLambda([this, Idx](FString Selected, ESelectInfo::Type)
		{
			if (EditingStory.Nodes.IsValidIndex(Idx)) EditingStory.Nodes[Idx].Next = Selected;
		});
	}
	else if (Node.Type == TEXT("choice"))
	{
		AddChoiceRows(Box_Details, &Node.Choices);
	}

	// ---- 立绘 / 事件（dialogue 与 choice 通用） ----
	AddPortraitRows(Box_Details, &Node.Portraits);
	AddEventRows(Box_Details, TEXT("进入事件"), &Node.EventsOnEnter);
	AddEventRows(Box_Details, TEXT("离开事件"), &Node.EventsOnExit);
}

void UTADialogueEditorWidget::AddPortraitRows(UPanelWidget* Box, TArray<FTAPortraitEntry>* Portraits)
{
	AddSectionTitle(Box, TEXT("立绘"));

	UTADialogueRowButton* AddBtn = AddButtonRow(Box, TEXT("＋ 添加立绘"));
	if (AddBtn)
	{
		AddBtn->OnClickedNative.AddLambda([this, Portraits]()
		{
			Portraits->Add(FTAPortraitEntry());
			RebuildDetails();
		});
	}

	for (int32 i = 0; i < Portraits->Num(); ++i)
	{
		const int32 PIdx = i;

		// 标题 + 删除
		UHorizontalBox* TitleRow = NewObject<UHorizontalBox>(this);
		AddWidgetWithPadding(Box, TitleRow);
		UTextBlock* TitleText = NewObject<UTextBlock>(this);
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("角色 #%d（%s）"), i, *(*Portraits)[i].CharacterId)));
		SetDialogueEditorFont(TitleText, true);
		AddWidgetWithPadding(TitleRow, TitleText);

		UTADialogueRowButton* RemoveBtn = MakeButtonLocal(TEXT("✕ 删除"));
		AddWidgetWithPadding(TitleRow, RemoveBtn);
		RemoveBtn->OnClickedNative.AddLambda([this, Portraits, PIdx]()
		{
			if (Portraits->IsValidIndex(PIdx))
			{
				Portraits->RemoveAt(PIdx);
				RebuildDetails();
			}
		});

		AddTextBoxRow(Box, TEXT("characterId"), (*Portraits)[i].CharacterId)->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx)) (*Portraits)[PIdx].CharacterId = T.ToString();
		});

		AddImagePathRow(Box, TEXT("base"), (*Portraits)[i].Base)->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx)) (*Portraits)[PIdx].Base = T.ToString();
		});
		AddImagePathRow(Box, TEXT("eyesOpen"), (*Portraits)[i].EyesOpen)->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx)) (*Portraits)[PIdx].EyesOpen = T.ToString();
		});
		AddImagePathRow(Box, TEXT("eyesClosed"), (*Portraits)[i].EyesClosed)->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx)) (*Portraits)[PIdx].EyesClosed = T.ToString();
		});
		AddImagePathRow(Box, TEXT("mouthOpen"), (*Portraits)[i].MouthOpen)->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx)) (*Portraits)[PIdx].MouthOpen = T.ToString();
		});
		AddImagePathRow(Box, TEXT("mouthClosed"), (*Portraits)[i].MouthClosed)->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx)) (*Portraits)[PIdx].MouthClosed = T.ToString();
		});

		AddTextBoxRow(Box, TEXT("position.x"), FString::SanitizeFloat((*Portraits)[i].Position.X))
			->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx))
			{
				(*Portraits)[PIdx].Position.X = FCString::Atof(*T.ToString());
				(*Portraits)[PIdx].bPositionSpecified = true;
			}
		});

		AddTextBoxRow(Box, TEXT("position.y"), FString::SanitizeFloat((*Portraits)[i].Position.Y))
			->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx))
			{
				(*Portraits)[PIdx].Position.Y = FCString::Atof(*T.ToString());
				(*Portraits)[PIdx].bPositionSpecified = true;
			}
		});

		AddTextBoxRow(Box, TEXT("scale"), FString::SanitizeFloat((*Portraits)[i].Scale))
			->OnTextChangedNative.AddLambda([this, Portraits, PIdx](const FText& T)
		{
			if (Portraits->IsValidIndex(PIdx))
			{
				(*Portraits)[PIdx].Scale = FMath::Max(FCString::Atof(*T.ToString()), 0.01f);
				(*Portraits)[PIdx].bScaleSpecified = true;
			}
		});

		AddCheckRow(Box, TEXT("visible"), (*Portraits)[i].bVisible)
			->OnCheckChangedNative.AddLambda([this, Portraits, PIdx](bool bChecked)
		{
			if (Portraits->IsValidIndex(PIdx))
			{
				(*Portraits)[PIdx].bVisible = bChecked;
				(*Portraits)[PIdx].bVisibleSpecified = true;
			}
		});
	}
}

void UTADialogueEditorWidget::AddEventRows(UPanelWidget* Box, const FString& Title, TArray<FTAStoryEvent>* Events)
{
	AddSectionTitle(Box, Title);

	UTADialogueRowButton* AddBtn = AddButtonRow(Box, TEXT("＋ 添加事件"));
	if (AddBtn)
	{
		AddBtn->OnClickedNative.AddLambda([this, Events]()
		{
			Events->Add(FTAStoryEvent());
			RebuildDetails();
		});
	}

	for (int32 i = 0; i < Events->Num(); ++i)
	{
		const int32 EIdx = i;

		UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
		AddWidgetWithPadding(Box, Row);

		UTextBlock* Label = NewObject<UTextBlock>(this);
		Label->SetText(FText::FromString(TEXT("名字")));
		SetDialogueEditorFont(Label);
		AddWidgetWithPadding(Row, Label);

		UTADialogueRowTextBox* NameBox = NewObject<UTADialogueRowTextBox>(this);
		SetDialogueEditorFont(NameBox);
		NameBox->SetText(FText::FromString((*Events)[i].Name));
		AddWidgetWithPadding(Row, NameBox);
		NameBox->OnTextChangedNative.AddLambda([this, Events, EIdx](const FText& T)
		{
			if (Events->IsValidIndex(EIdx)) (*Events)[EIdx].Name = T.ToString();
		});

		UTextBlock* ParamLabel = NewObject<UTextBlock>(this);
		ParamLabel->SetText(FText::FromString(TEXT("参数(k=v;k2=v2)")));
		SetDialogueEditorFont(ParamLabel);
		AddWidgetWithPadding(Row, ParamLabel);

		UTADialogueRowTextBox* ParamBox = NewObject<UTADialogueRowTextBox>(this);
		SetDialogueEditorFont(ParamBox);
		ParamBox->SetText(FText::FromString(ParamsToString((*Events)[i].Params)));
		AddWidgetWithPadding(Row, ParamBox);
		ParamBox->OnTextChangedNative.AddLambda([this, Events, EIdx](const FText& T)
		{
			if (Events->IsValidIndex(EIdx)) ParseParamsString(T.ToString(), (*Events)[EIdx].Params);
		});

		UTADialogueRowButton* RemoveBtn = MakeButtonLocal(TEXT("✕"));
		AddWidgetWithPadding(Row, RemoveBtn);
		RemoveBtn->OnClickedNative.AddLambda([this, Events, EIdx]()
		{
			if (Events->IsValidIndex(EIdx))
			{
				Events->RemoveAt(EIdx);
				RebuildDetails();
			}
		});
	}
}

void UTADialogueEditorWidget::AddConditionRows(UPanelWidget* Box, TArray<FTAStoryCondition>* Conditions)
{
	AddSectionTitle(Box, TEXT("条件"));

	UTADialogueRowButton* AddBtn = AddButtonRow(Box, TEXT("＋ 添加条件"));
	if (AddBtn)
	{
		AddBtn->OnClickedNative.AddLambda([this, Conditions]()
		{
			FTAStoryCondition NewCond;
			NewCond.Type = TEXT("Flag");
			Conditions->Add(NewCond);
			RebuildDetails();
		});
	}

	for (int32 i = 0; i < Conditions->Num(); ++i)
	{
		const int32 CIdx = i;
		const FTAStoryCondition& Cond = (*Conditions)[i];

		AddComboRow(Box, FString::Printf(TEXT("条件 #%d 类型"), i), { TEXT("Flag"), TEXT("HasItem"), TEXT("Money"), TEXT("Attribute") }, Cond.Type)
			->OnSelectionNative.AddLambda([this, Conditions, CIdx](FString Selected, ESelectInfo::Type)
		{
			if (!Conditions->IsValidIndex(CIdx) || Selected.IsEmpty())
			{
				return;
			}
			(*Conditions)[CIdx].Type = Selected;
			RebuildDetails();
		});

		if (Cond.Type == TEXT("Flag"))
		{
			AddTextBoxRow(Box, TEXT("flag"), Cond.Flag)->OnTextChangedNative.AddLambda([this, Conditions, CIdx](const FText& T)
			{
				if (Conditions->IsValidIndex(CIdx)) (*Conditions)[CIdx].Flag = T.ToString();
			});
			AddComboRow(Box, TEXT("op"), { TEXT("=="), TEXT("!="), TEXT(">="), TEXT("<="), TEXT(">"), TEXT("<") }, Cond.Op)
				->OnSelectionNative.AddLambda([this, Conditions, CIdx](FString Selected, ESelectInfo::Type)
			{
				if (Conditions->IsValidIndex(CIdx) && !Selected.IsEmpty()) (*Conditions)[CIdx].Op = Selected;
			});
			AddTextBoxRow(Box, TEXT("value"), Cond.Value)->OnTextChangedNative.AddLambda([this, Conditions, CIdx](const FText& T)
			{
				if (Conditions->IsValidIndex(CIdx)) (*Conditions)[CIdx].Value = T.ToString();
			});
		}
		else if (Cond.Type == TEXT("HasItem"))
		{
			AddTextBoxRow(Box, TEXT("item"), Cond.Item)->OnTextChangedNative.AddLambda([this, Conditions, CIdx](const FText& T)
			{
				if (Conditions->IsValidIndex(CIdx)) (*Conditions)[CIdx].Item = T.ToString();
			});
			AddTextBoxRow(Box, TEXT("count"), FString::FromInt(Cond.Count))->OnTextChangedNative.AddLambda([this, Conditions, CIdx](const FText& T)
			{
				if (Conditions->IsValidIndex(CIdx)) (*Conditions)[CIdx].Count = FMath::Max(0, FCString::Atoi(*T.ToString()));
			});
		}
		else if (Cond.Type == TEXT("Money"))
		{
			AddComboRow(Box, TEXT("op"), { TEXT("=="), TEXT("!="), TEXT(">="), TEXT("<="), TEXT(">"), TEXT("<") }, Cond.Op)
				->OnSelectionNative.AddLambda([this, Conditions, CIdx](FString Selected, ESelectInfo::Type)
			{
				if (Conditions->IsValidIndex(CIdx) && !Selected.IsEmpty()) (*Conditions)[CIdx].Op = Selected;
			});
			AddTextBoxRow(Box, TEXT("value"), Cond.Value)->OnTextChangedNative.AddLambda([this, Conditions, CIdx](const FText& T)
			{
				if (Conditions->IsValidIndex(CIdx)) (*Conditions)[CIdx].Value = T.ToString();
			});
		}
		else if (Cond.Type == TEXT("Attribute"))
		{
			AddTextBoxRow(Box, TEXT("attribute"), Cond.Attribute)->OnTextChangedNative.AddLambda([this, Conditions, CIdx](const FText& T)
			{
				if (Conditions->IsValidIndex(CIdx)) (*Conditions)[CIdx].Attribute = T.ToString();
			});
			AddComboRow(Box, TEXT("op"), { TEXT("=="), TEXT("!="), TEXT(">="), TEXT("<="), TEXT(">"), TEXT("<") }, Cond.Op)
				->OnSelectionNative.AddLambda([this, Conditions, CIdx](FString Selected, ESelectInfo::Type)
			{
				if (Conditions->IsValidIndex(CIdx) && !Selected.IsEmpty()) (*Conditions)[CIdx].Op = Selected;
			});
			AddTextBoxRow(Box, TEXT("value"), Cond.Value)->OnTextChangedNative.AddLambda([this, Conditions, CIdx](const FText& T)
			{
				if (Conditions->IsValidIndex(CIdx)) (*Conditions)[CIdx].Value = T.ToString();
			});
		}

		UTADialogueRowButton* RemoveBtn = AddButtonRow(Box, FString::Printf(TEXT("✕ 删除条件 #%d"), i));
		if (RemoveBtn)
		{
			RemoveBtn->OnClickedNative.AddLambda([this, Conditions, CIdx]()
			{
				if (Conditions->IsValidIndex(CIdx))
				{
					Conditions->RemoveAt(CIdx);
					RebuildDetails();
				}
			});
		}
	}
}

void UTADialogueEditorWidget::AddChoiceRows(UPanelWidget* Box, TArray<FTAStoryChoice>* Choices)
{
	AddSectionTitle(Box, TEXT("分支选项"));

	UTADialogueRowButton* AddBtn = AddButtonRow(Box, TEXT("＋ 添加选项"));
	if (AddBtn)
	{
		AddBtn->OnClickedNative.AddLambda([this, Choices]()
		{
			FTAStoryChoice NewChoice;
			NewChoice.TextId = FString::Printf(TEXT("%s_Choice_%d"), *EditingStoryId, Choices->Num() + 1);
			Choices->Add(NewChoice);
			RebuildDetails();
		});
	}

	for (int32 i = 0; i < Choices->Num(); ++i)
	{
		const int32 ChIdx = i;

		AddSectionTitle(Box, FString::Printf(TEXT("选项 #%d"), i));

		AddTextBoxRow(Box, TEXT("textId"), (*Choices)[i].TextId)->OnTextChangedNative.AddLambda([this, Choices, ChIdx](const FText& T)
		{
			if (Choices->IsValidIndex(ChIdx)) (*Choices)[ChIdx].TextId = T.ToString();
		});

		UTADialogueRowButton* LocBtn = AddButtonRow(Box, FString::Printf(TEXT("✎ 编辑本地化（选项 #%d）"), i));
		if (LocBtn)
		{
			LocBtn->OnClickedNative.AddLambda([this, Choices, ChIdx]()
			{
				if (Choices->IsValidIndex(ChIdx)) OpenLocalizeKey((*Choices)[ChIdx].TextId);
			});
		}

		AddConditionRows(Box, &(*Choices)[i].Conditions);
		AddEventRows(Box, TEXT("选中事件"), &(*Choices)[i].Events);

		AddComboRow(Box, TEXT("target"), BuildNodeOptions(EditingStory), (*Choices)[i].Target)
			->OnSelectionNative.AddLambda([this, Choices, ChIdx](FString Selected, ESelectInfo::Type)
		{
			if (Choices->IsValidIndex(ChIdx)) (*Choices)[ChIdx].Target = Selected;
		});

		UTADialogueRowButton* RemoveBtn = AddButtonRow(Box, FString::Printf(TEXT("✕ 删除选项 #%d"), i));
		if (RemoveBtn)
		{
			RemoveBtn->OnClickedNative.AddLambda([this, Choices, ChIdx]()
			{
				if (Choices->IsValidIndex(ChIdx))
				{
					Choices->RemoveAt(ChIdx);
					RebuildDetails();
				}
			});
		}
	}
}

// ============================================================
// 本地化编辑区
// ============================================================

void UTADialogueEditorWidget::OpenLocalizeKey(const FString& Key)
{
	if (Key.IsEmpty())
	{
		SetStatus(TEXT("Key 为空，无法编辑本地化"));
		return;
	}

	CurrentLocalizeKey = Key;
	FTADialogueLocalizationHelper::EnsureKey(Key, FTADialogueLocalizationHelper::GetText(Key, TEXT("zh-CN"), LanguageCache), LanguageCache);
	RebuildLocalizeSection();
	SetStatus(FString::Printf(TEXT("正在编辑本地化 Key：%s"), *Key));
}

void UTADialogueEditorWidget::RebuildLocalizeSection()
{
	if (!Box_Localize)
	{
		return;
	}
	Box_Localize->ClearChildren();

	if (CurrentLocalizeKey.IsEmpty())
	{
		return;
	}

	const FString Key = CurrentLocalizeKey;

	AddTextBoxRow(Box_Localize, TEXT("Key"), Key)->SetIsReadOnly(true);

	for (const FString& Lang : FTADialogueLocalizationHelper::GetLanguageCodes())
	{
		UTextBlock* Label = NewObject<UTextBlock>(this);
		Label->SetText(FText::FromString(Lang));
		SetDialogueEditorFont(Label, true);
		AddWidgetWithPadding(Box_Localize, Label);

		UTADialogueRowMultiLineBox* MB = NewObject<UTADialogueRowMultiLineBox>(this);
		SetDialogueEditorFont(MB);
		MB->SetText(FText::FromString(FTADialogueLocalizationHelper::GetText(Key, Lang, LanguageCache)));
		AddWidgetWithPadding(Box_Localize, MB);
		MB->OnTextChangedNative.AddLambda([this, Key, Lang](const FText& T)
		{
			FTADialogueLocalizationHelper::SetText(Key, Lang, T.ToString(), LanguageCache);
		});
	}
}

// ============================================================
// 模拟面板
// ============================================================

void UTADialogueEditorWidget::RebuildSimulationPanel()
{
	if (!Box_Simulation)
	{
		return;
	}
	Box_Simulation->ClearChildren();

	AddSectionTitle(Box_Simulation, TEXT("模拟状态（预览条件判断用）"));

	SimMoneyInput = AddTextBoxRow(Box_Simulation, TEXT("金钱"), TEXT("0"));
	SimItemsInput = AddTextBoxRow(Box_Simulation, TEXT("持有物品(逗号分隔)"), TEXT(""));

	UHorizontalBox* FlagRow = NewObject<UHorizontalBox>(this);
	AddWidgetWithPadding(Box_Simulation, FlagRow);
	UTextBlock* FlagLabel = NewObject<UTextBlock>(this);
	FlagLabel->SetText(FText::FromString(TEXT("旗标名")));
	SetDialogueEditorFont(FlagLabel);
	AddWidgetWithPadding(FlagRow, FlagLabel);
	SimFlagInput = NewObject<UTADialogueRowTextBox>(this);
	SetDialogueEditorFont(SimFlagInput);
	AddWidgetWithPadding(FlagRow, SimFlagInput);
	UTextBlock* ValueLabel = NewObject<UTextBlock>(this);
	ValueLabel->SetText(FText::FromString(TEXT("值")));
	SetDialogueEditorFont(ValueLabel);
	AddWidgetWithPadding(FlagRow, ValueLabel);
	SimFlagValueInput = NewObject<UTADialogueRowTextBox>(this);
	SetDialogueEditorFont(SimFlagValueInput);
	SimFlagValueInput->SetText(FText::FromString(TEXT("true")));
	AddWidgetWithPadding(FlagRow, SimFlagValueInput);

	UTADialogueRowButton* SetFlagBtn = AddButtonRow(Box_Simulation, TEXT("设置旗标"));
	if (SetFlagBtn)
	{
		SetFlagBtn->OnClickedNative.AddLambda([this]()
		{
			UTADialogueSubsystem* Sub = GetDialogueSubsystem();
			if (!Sub || !SimFlagInput)
			{
				return;
			}
			const FString Flag = SimFlagInput->GetText().ToString();
			const FString Value = SimFlagValueInput ? SimFlagValueInput->GetText().ToString() : TEXT("true");
			if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
			{
				Sub->SetFlagBool(Flag, Value.Equals(TEXT("true"), ESearchCase::IgnoreCase));
			}
			else
			{
				Sub->SetFlagInt(Flag, FCString::Atoi(*Value));
			}
			SetStatus(FString::Printf(TEXT("已设置旗标 %s = %s"), *Flag, *Value));
		});
	}

	UTADialogueRowButton* ApplyBtn = AddButtonRow(Box_Simulation, TEXT("应用到预览"));
	if (ApplyBtn)
	{
		ApplyBtn->OnClicked.AddDynamic(this, &UTADialogueEditorWidget::ApplySimulation);
	}
}

FTADialogueConditionContext UTADialogueEditorWidget::BuildPreviewOverrides() const
{
	FTADialogueConditionContext Ctx;
	Ctx.bUseOverrides = true;
	Ctx.OverrideMoney = SimMoneyInput ? FCString::Atoi(*SimMoneyInput->GetText().ToString()) : 0;

	if (SimItemsInput)
	{
		TArray<FString> Items;
		SimItemsInput->GetText().ToString().ParseIntoArray(Items, TEXT(","), true);
		for (FString& Item : Items)
		{
			Item.TrimStartAndEndInline();
			if (!Item.IsEmpty())
			{
				Ctx.OverrideItems.Add(Item);
			}
		}
	}

	return Ctx;
}

// ============================================================
// 预览
// ============================================================

void UTADialogueEditorWidget::StartPreview()
{
	if (bPreviewActive)
	{
		StopPreview();
	}

	if (EditingStory.Nodes.Num() == 0)
	{
		SetStatus(TEXT("没有可预览的剧情"));
		return;
	}

	UTADialogueSubsystem* Sub = GetDialogueSubsystem();
	if (!Sub)
	{
		SetStatus(TEXT("找不到 UTADialogueSubsystem"));
		return;
	}

	if (Sub->DialogueWidgetClass.IsNull())
	{
		SetStatus(TEXT("未配置 DialogueWidgetClass（DefaultGame.ini [/Script/The_Awakening.TADialogueSubsystem]）"));
		return;
	}

	if (!Box_PreviewHost)
	{
		SetStatus(TEXT("缺少 Box_PreviewHost"));
		return;
	}

	EditingStory.RebuildIndex();

	PreviewController = NewObject<UTADialogueController>(this);
	PreviewController->Initialize(Sub, EditingStory, this);

	PreviewWidget = CreateWidget<UTADialogueWidget>(this, Sub->DialogueWidgetClass.LoadSynchronous());
	if (!PreviewWidget)
	{
		SetStatus(TEXT("创建预览 UI 失败"));
		PreviewController = nullptr;
		return;
	}

	PreviewWidget->Setup(Sub, PreviewController);
	PreviewWidget->SetPreviewMode(true);
	AddWidgetWithPadding(Box_PreviewHost, PreviewWidget);

	PreviewController->SetConditionOverrides(BuildPreviewOverrides());
	PreviewController->Start();

	bPreviewActive = true;
	SetStatus(TEXT("预览中（点击预览按钮停止）"));
}

void UTADialogueEditorWidget::StopPreview()
{
	if (PreviewWidget)
	{
		PreviewWidget->CloseDialogue();
		PreviewWidget = nullptr;
	}
	if (PreviewController)
	{
		PreviewController->Shutdown();
		PreviewController = nullptr;
	}
	bPreviewActive = false;
}

UTADialogueSubsystem* UTADialogueEditorWidget::GetDialogueSubsystem() const
{
	if (UWorld* W = GetWorld())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UTADialogueSubsystem* Sub = GI->GetSubsystem<UTADialogueSubsystem>())
			{
				return Sub;
			}
		}
	}

	if (GEditor)
	{
		if (UWorld* W = GEditor->GetEditorWorldContext().World())
		{
			if (UGameInstance* GI = W->GetGameInstance())
			{
				return GI->GetSubsystem<UTADialogueSubsystem>();
			}
		}
	}

	return nullptr;
}

// ============================================================
// 校验
// ============================================================

void UTADialogueEditorWidget::CollectValidationErrors(TArray<FString>& OutErrors)
{
	OutErrors.Reset();

	// 载入 zh-CN 检查本地化 Key
	FTADialogueLocalizationHelper::LoadInto(TEXT("zh-CN"), LanguageCache);
	const TMap<FString, FString>& ZhMap = LanguageCache[TEXT("zh-CN")];

	auto CheckLocKey = [&](const FString& Key, const FString& Where)
	{
		if (!Key.IsEmpty() && !ZhMap.Contains(Key))
		{
			OutErrors.Add(FString::Printf(TEXT("%s：缺少本地化 Key \"%s\"（zh-CN）"), *Where, *Key));
		}
	};

	auto CheckTexturePath = [&](const FString& Path, const FString& Where)
	{
		if (Path.IsEmpty())
		{
			return;
		}
		const FAssetData Asset = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(NormalizeObjectPath(Path)));
		if (!Asset.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s：贴图不存在 %s"), *Where, *Path));
		}
	};

	if (EditingStory.Nodes.Num() == 0)
	{
		OutErrors.Add(TEXT("剧情没有节点"));
		return;
	}

	if (EditingStory.FindNodeIndex(EditingStory.Entry) == INDEX_NONE)
	{
		OutErrors.Add(FString::Printf(TEXT("入口节点 %s 不存在"), *EditingStory.Entry));
	}

	// 重复 ID
	TSet<FString> Ids;
	for (const FTAStoryNode& Node : EditingStory.Nodes)
	{
		Ids.Add(Node.Id);
	}
	if (Ids.Num() != EditingStory.Nodes.Num())
	{
		OutErrors.Add(TEXT("存在重复的节点 ID"));
	}

	for (const FTAStoryNode& Node : EditingStory.Nodes)
	{
		const FString Where = FString::Printf(TEXT("节点 %s"), *Node.Id);

		if (Node.Type != TEXT("dialogue") && Node.Type != TEXT("choice") && Node.Type != TEXT("end"))
		{
			OutErrors.Add(FString::Printf(TEXT("%s：未知类型 %s"), *Where, *Node.Type));
		}

		if (Node.Type == TEXT("dialogue"))
		{
			if (!Node.Next.IsEmpty() && EditingStory.FindNodeIndex(Node.Next) == INDEX_NONE)
			{
				OutErrors.Add(FString::Printf(TEXT("%s：next 目标 %s 不存在"), *Where, *Node.Next));
			}
			CheckLocKey(Node.TextId, Where);
			CheckLocKey(Node.SpeakerNameId, Where);
		}
		else if (Node.Type == TEXT("choice"))
		{
			if (Node.Choices.Num() == 0)
			{
				OutErrors.Add(FString::Printf(TEXT("%s：分支节点没有选项"), *Where));
			}
			for (int32 i = 0; i < Node.Choices.Num(); ++i)
			{
				const FTAStoryChoice& Choice = Node.Choices[i];
				const FString CWhere = FString::Printf(TEXT("%s 选项#%d"), *Where, i);
				if (Choice.TextId.IsEmpty())
				{
					OutErrors.Add(FString::Printf(TEXT("%s：textId 为空"), *CWhere));
				}
				else
				{
					CheckLocKey(Choice.TextId, CWhere);
				}
				if (!Choice.Target.IsEmpty() && EditingStory.FindNodeIndex(Choice.Target) == INDEX_NONE)
				{
					OutErrors.Add(FString::Printf(TEXT("%s：target 目标 %s 不存在"), *CWhere, *Choice.Target));
				}
			}
		}

		for (const FTAPortraitEntry& Entry : Node.Portraits)
		{
			const FString PWhere = FString::Printf(TEXT("%s 立绘 %s"), *Where, *Entry.CharacterId);
			if (Entry.CharacterId.IsEmpty())
			{
				OutErrors.Add(FString::Printf(TEXT("%s：characterId 为空"), *Where));
			}
			CheckTexturePath(Entry.Base, PWhere);
			CheckTexturePath(Entry.EyesOpen, PWhere);
			CheckTexturePath(Entry.EyesClosed, PWhere);
			CheckTexturePath(Entry.MouthOpen, PWhere);
			CheckTexturePath(Entry.MouthClosed, PWhere);
		}
	}
}

// ============================================================
// 工具
// ============================================================

void UTADialogueEditorWidget::SetStatus(const FString& Text)
{
	if (Text_Status)
	{
		Text_Status->SetText(FText::FromString(Text));
	}
}

void UTADialogueEditorWidget::ScanTextures()
{
	ScannedTexturePaths.Reset();

	FAssetRegistryModule& ARMod = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game/Portraits"));
	Filter.PackagePaths.Add(TEXT("/Game/Characters"));
	Filter.PackagePaths.Add(TEXT("/Game/Textures"));

	TArray<FAssetData> Assets;
	ARMod.Get().GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.GetSoftObjectPath().GetLongPackageName() < B.GetSoftObjectPath().GetLongPackageName();
	});

	for (const FAssetData& Asset : Assets)
	{
		ScannedTexturePaths.Add(Asset.GetSoftObjectPath().GetLongPackageName());
	}
}

void UTADialogueEditorWidget::AddSectionTitle(UPanelWidget* Box, const FString& Title)
{
	UTextBlock* Text = NewObject<UTextBlock>(this);
	Text->SetText(FText::FromString(FString::Printf(TEXT("—— %s ——"), *Title)));
	SetDialogueEditorFont(Text, false, true);
	UBorder* Header = NewObject<UBorder>(this);
	Header->SetBrush(*FAppStyle::GetBrush(TEXT("DetailsView.CategoryTop")));
	Header->SetPadding(FMargin(4.0f, 2.0f));
	Header->SetContent(Text);
	AddWidgetWithPadding(Box, Header);
}

UTADialogueRowButton* UTADialogueEditorWidget::MakeButtonLocal(const FString& Label)
{
	UTADialogueRowButton* B = NewObject<UTADialogueRowButton>(this);
	B->SetStyle(FAppStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("NoBorder")));
	UTextBlock* T = NewObject<UTextBlock>(this);
	T->SetText(FText::FromString(Label));
	SetDialogueEditorFont(T);
	AddWidgetWithPadding(B, T);
	return B;
}

UTADialogueRowTextBox* UTADialogueEditorWidget::AddTextBoxRow(UPanelWidget* Box, const FString& Label, const FString& Initial)
{
	UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
	AddWidgetWithPadding(Box, Row);

	UTextBlock* L = NewObject<UTextBlock>(this);
	L->SetText(FText::FromString(Label));
	SetDialogueEditorFont(L);
	AddWidgetWithPadding(Row, L);
	if (UHorizontalBoxSlot* LS = Cast<UHorizontalBoxSlot>(L->Slot))
	{
		LS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UTADialogueRowTextBox* T = NewObject<UTADialogueRowTextBox>(this);
	SetDialogueEditorFont(T);
	T->SetText(FText::FromString(Initial));
	AddWidgetWithPadding(Row, T);
	if (UHorizontalBoxSlot* TS = Cast<UHorizontalBoxSlot>(T->Slot))
	{
		TS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	return T;
}

UTADialogueRowCombo* UTADialogueEditorWidget::AddComboRow(UPanelWidget* Box, const FString& Label, const TArray<FString>& Options, const FString& Selected)
{
	UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
	AddWidgetWithPadding(Box, Row);

	UTextBlock* L = NewObject<UTextBlock>(this);
	L->SetText(FText::FromString(Label));
	SetDialogueEditorFont(L);
	AddWidgetWithPadding(Row, L);
	if (UHorizontalBoxSlot* LS = Cast<UHorizontalBoxSlot>(L->Slot))
	{
		LS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UTADialogueRowCombo* C = NewObject<UTADialogueRowCombo>(this);
	SetDialogueEditorFont(C);
	for (const FString& Option : Options)
	{
		C->AddOption(Option);
	}
	if (!Selected.IsEmpty())
	{
		C->SetSelectedOption(Selected);
	}
	AddWidgetWithPadding(Row, C);
	if (UHorizontalBoxSlot* CS = Cast<UHorizontalBoxSlot>(C->Slot))
	{
		CS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	return C;
}

UTADialogueRowCheckBox* UTADialogueEditorWidget::AddCheckRow(UPanelWidget* Box, const FString& Label, bool bInitial)
{
	UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
	AddWidgetWithPadding(Box, Row);

	UTextBlock* L = NewObject<UTextBlock>(this);
	L->SetText(FText::FromString(Label));
	SetDialogueEditorFont(L);
	AddWidgetWithPadding(Row, L);

	UTADialogueRowCheckBox* C = NewObject<UTADialogueRowCheckBox>(this);
	C->SetIsChecked(bInitial);
	AddWidgetWithPadding(Row, C);

	return C;
}

UTADialogueRowButton* UTADialogueEditorWidget::AddButtonRow(UPanelWidget* Box, const FString& Label)
{
	UTADialogueRowButton* B = MakeButtonLocal(Label);
	AddWidgetWithPadding(Box, B);
	return B;
}

UTADialogueRowTextBox* UTADialogueEditorWidget::AddImagePathRow(UPanelWidget* Box, const FString& Label, const FString& Initial)
{
	UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
	AddWidgetWithPadding(Box, Row);

	UTextBlock* L = NewObject<UTextBlock>(this);
	L->SetText(FText::FromString(Label));
	SetDialogueEditorFont(L);
	AddWidgetWithPadding(Row, L);
	if (UHorizontalBoxSlot* LS = Cast<UHorizontalBoxSlot>(L->Slot))
	{
		LS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UTADialogueRowTextBox* T = NewObject<UTADialogueRowTextBox>(this);
	SetDialogueEditorFont(T);
	T->SetText(FText::FromString(Initial));
	AddWidgetWithPadding(Row, T);
	if (UHorizontalBoxSlot* TS = Cast<UHorizontalBoxSlot>(T->Slot))
	{
		TS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UTADialogueRowCombo* C = NewObject<UTADialogueRowCombo>(this);
	SetDialogueEditorFont(C);
	C->AddOption(TEXT("（选择贴图…）"));
	for (const FString& Path : ScannedTexturePaths)
	{
		C->AddOption(Path);
	}
	C->SetSelectedOption(TEXT("（选择贴图…）"));
	AddWidgetWithPadding(Row, C);
	if (UHorizontalBoxSlot* CS = Cast<UHorizontalBoxSlot>(C->Slot))
	{
		CS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	C->OnSelectionNative.AddLambda([T](FString Selected, ESelectInfo::Type)
	{
		if (!Selected.IsEmpty() && Selected != TEXT("（选择贴图…）"))
		{
			T->SetText(FText::FromString(Selected));
		}
	});

	return T;
}

FString UTADialogueEditorWidget::ParamsToString(const TMap<FString, FString>& Params)
{
	FString Result;
	for (const auto& Pair : Params)
	{
		if (!Result.IsEmpty())
		{
			Result += TEXT(";");
		}
		Result += Pair.Key + TEXT("=") + Pair.Value;
	}
	return Result;
}

void UTADialogueEditorWidget::ParseParamsString(const FString& Text, TMap<FString, FString>& OutParams)
{
	OutParams.Reset();

	TArray<FString> Pairs;
	Text.ParseIntoArray(Pairs, TEXT(";"), true);
	for (const FString& Pair : Pairs)
	{
		FString Key;
		FString Value;
		if (Pair.Split(TEXT("="), &Key, &Value))
		{
			OutParams.Add(Key.TrimStartAndEnd(), Value.TrimStartAndEnd());
		}
	}
}
