#include "TADocumentUICommandlet.h"
#include "UI/Document/TADocumentReaderWidget.h"
#include "UI/Document/TAJustifiedRichTextBlock.h"
#include "UI/Document/TARichTextImageDecorator.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/ScaleBox.h"
#include "Components/Button.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/Inventory/TAItemActionMenuWidget.h"
#include "UI/Document/TADiaryWidget.h"
#include "UI/Inventory/TAQuantityDialogWidget.h"
#include "UI/Inventory/TAItemInspectWidget.h"
#include "UI/Pickup/TAPickupBannerWidget.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputAction.h"
#include "Animation/AnimSequence.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "Document/TADiaryPreviewStage.h"
#include "Document/TADiaryDocumentComponent.h"
#include "Document/TADocumentDefinition.h"
#include "Document/TADocumentSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "AssetCompilingManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogTADocumentUI, Log, All);

namespace
{
	const TCHAR* ScrollActionPath = TEXT("/Game/Input/Actions/UI/IA_ReaderScroll");
	const TCHAR* UIContextPath = TEXT("/Game/Input/IMC_UI.IMC_UI");
	const TCHAR* ReaderWidgetPath = TEXT("/Game/UI/Document/WBP_DocumentReader");

	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		const bool bOk = UPackage::SavePackage(Package, Asset, *Filename, Args);
		UE_LOG(LogTADocumentUI, Display, TEXT("%s %s."), bOk ? TEXT("Saved") : TEXT("FAILED to save"), *Package->GetName());
		return bOk;
	}

	UInputAction* EnsureAction(const TCHAR* Path, EInputActionValueType ValueType, bool& bOk)
	{
		const FString Name = FPackageName::GetShortName(Path);
		if (UInputAction* Existing = LoadObject<UInputAction>(nullptr, *(FString(Path) + TEXT(".") + Name)))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(Path);
		UInputAction* Action = NewObject<UInputAction>(Package, *Name, RF_Public | RF_Standalone);
		Action->ValueType = ValueType;
		FAssetRegistryModule::AssetCreated(Action);
		bOk &= SaveAsset(Action);
		return Action;
	}

	void EnsureScrollMappings(UInputAction* Action, bool& bOk)
	{
		UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, UIContextPath);
		if (!Context)
		{
			UE_LOG(LogTADocumentUI, Error, TEXT("IMC_UI not found."));
			bOk = false;
			return;
		}
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action == Action)
			{
				UE_LOG(LogTADocumentUI, Display, TEXT("IMC_UI already maps IA_ReaderScroll; not changed."));
				return;
			}
		}
		// 正值 = 正文向下滚
		Context->MapKey(Action, EKeys::Down);
		FEnhancedActionKeyMapping& Up = Context->MapKey(Action, EKeys::Up);
		Up.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		FEnhancedActionKeyMapping& Stick = Context->MapKey(Action, EKeys::Gamepad_RightY);
		Stick.Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));
		Stick.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		Context->MarkPackageDirty();
		bOk &= SaveAsset(Context);
	}

	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Canvas, UWidget* Child, const FAnchors& Anchors)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(FMargin(0.f));
		return Slot;
	}

	void BuildReaderLayout(UWidgetTree* Tree)
	{
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Tree->RootWidget = Root;

		// 背景：变糊 + 变暗，铺满屏幕
		UBackgroundBlur* Blur = Tree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("Blur_Background"));
		Blur->SetBlurStrength(6.f);
		AddToCanvas(Root, Blur, FAnchors(0.f, 0.f, 1.f, 1.f));
		UImage* Dim = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Dim"));
		Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		AddToCanvas(Root, Dim, FAnchors(0.f, 0.f, 1.f, 1.f));

		// 3D 模型：居中，按比例缩放
		UScaleBox* ModelBox = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ScaleBox_Model"));
		ModelBox->SetStretch(EStretch::ScaleToFit);
		AddToCanvas(Root, ModelBox, FAnchors(0.15f, 0.06f, 0.85f, 0.86f));
		UImage* Model = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Model"));
		ModelBox->SetContent(Model);

		// 无底框的居中转译；运行时在模型之上、文字和操作栏之下插入转译虚化暗幕。
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_Translation"));
		Panel->SetBrushColor(FLinearColor::Transparent);
		Panel->SetVerticalAlignment(VAlign_Center);
		Panel->SetPadding(FMargin(0.f, 32.f));
		AddToCanvas(Root, Panel, FAnchors(1.f / 3.f, 0.12f, 2.f / 3.f, 0.88f));
		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_Translation"));
		Panel->SetContent(Column);

		UTextBlock* Title = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Title"));
		FSlateFontInfo TitleFont = Title->GetFont();
		TitleFont.Size = 30;
		Title->SetFont(TitleFont);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		Title->SetAutoWrapText(true);
		UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Fill);

		UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScrollBox_Body"));
		Column->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		URichTextBlock* Body = Tree->ConstructWidget<UTAJustifiedRichTextBlock>(UTAJustifiedRichTextBlock::StaticClass(), TEXT("RichText_Body"));
		Body->SetAutoWrapText(true);
		Body->SetJustification(ETextJustify::Left);
		// DecoratorClasses 不是公开成员，按反射写入
		if (FArrayProperty* Decorators = FindFProperty<FArrayProperty>(URichTextBlock::StaticClass(), TEXT("DecoratorClasses")))
		{
			auto* Classes = Decorators->ContainerPtrToValuePtr<TArray<TSubclassOf<URichTextBlockDecorator>>>(Body);
			Classes->AddUnique(UTARichTextImageDecorator::StaticClass());
		}
		Scroll->AddChild(Body);

		// 底部操作栏：按键提示由 C++ 生成
		UHorizontalBox* Controls = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Controls"));
		UCanvasPanelSlot* ControlsSlot = Root->AddChildToCanvas(Controls);
		ControlsSlot->SetAnchors(FAnchors(0.5f, 0.94f));
		ControlsSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ControlsSlot->SetAutoSize(true);
	}

	FSlateColor InkColor() { return FSlateColor(FLinearColor(0.12f, 0.08f, 0.05f, 1.f)); }

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Name, int32 Size, const FSlateColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(Color);
		Text->SetAutoWrapText(true);
		return Text;
	}

	void BuildItemActionOptionLayout(UWidgetTree* Tree)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Option"));
		Tree->RootWidget = Button;
		UTextBlock* Label = MakeText(Tree, TEXT("Text_Option"), 20, FSlateColor(FLinearColor::White));
		Label->SetAutoWrapText(false);
		Button->SetContent(Label);
		Button->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.9f));
	}

	void BuildItemActionMenuLayout(UWidgetTree* Tree)
	{
		// 全屏：透明接收层接住空白处的点击（关闭菜单），菜单本体由 C++ 移到光标旁
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Tree->RootWidget = Root;
		UImage* Catcher = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_ClickCatcher"));
		Catcher->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.f));
		AddToCanvas(Root, Catcher, FAnchors(0.f, 0.f, 1.f, 1.f));

		UBorder* Frame = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_Menu"));
		Frame->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.045f, 0.92f));
		Frame->SetPadding(FMargin(8.f));
		UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);
		FrameSlot->SetAnchors(FAnchors(0.f, 0.f));
		FrameSlot->SetAutoSize(true);
		UVerticalBox* Options = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_Options"));
		Frame->SetContent(Options);
	}

	void BuildDiaryLayout(UWidgetTree* Tree)
	{
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Tree->RootWidget = Root;

		UBackgroundBlur* Blur = Tree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("Blur_Background"));
		Blur->SetBlurStrength(6.f);
		AddToCanvas(Root, Blur, FAnchors(0.f, 0.f, 1.f, 1.f));
		UImage* Dim = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Dim"));
		Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		AddToCanvas(Root, Dim, FAnchors(0.f, 0.f, 1.f, 1.f));

		// A fixed 3:2 render surface keeps the SceneCapture aspect ratio at every viewport size.
		UScaleBox* BookFit = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ScaleBox_Book"));
		BookFit->SetStretch(EStretch::ScaleToFit);
		AddToCanvas(Root, BookFit, FAnchors(0.08f, 0.03f, 0.92f, 0.88f));
		USizeBox* BookSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_Book"));
		BookSize->SetWidthOverride(1536.f);
		BookSize->SetHeightOverride(1024.f);
		BookFit->SetContent(BookSize);
		UCanvasPanel* Book = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Panel_Book"));
		Book->SetRenderTransformPivot(FVector2D(0.5f));
		BookSize->SetContent(Book);
		UImage* BookImage = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Book"));
		BookImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddToCanvas(Book, BookImage, FAnchors(0.f, 0.f, 1.f, 1.f));

		// Metadata stays in UMG; all physical documents are captured with the book.
		UVerticalBox* Page = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Panel_Page"));
		Page->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddToCanvas(Book, Page, FAnchors(0.25f, 0.80f, 0.75f, 0.86f));
		UTextBlock* Title = MakeText(Tree, TEXT("Text_DocumentTitle"), 22, FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		Page->AddChildToVerticalBox(Title);
		UTextBlock* PageNumber = MakeText(Tree, TEXT("Text_PageNumber"), 24, FSlateColor(FLinearColor::White));
		PageNumber->SetJustification(ETextJustify::Center);
		AddToCanvas(Book, PageNumber, FAnchors(0.35f, 0.88f, 0.65f, 0.93f));
		UTextBlock* Empty = MakeText(Tree, TEXT("Text_Empty"), 24, FSlateColor(FLinearColor::White));
		Empty->SetJustification(ETextJustify::Center);
		AddToCanvas(Book, Empty, FAnchors(0.25f, 0.80f, 0.75f, 0.86f));
		UHorizontalBox* Controls = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Controls"));
		UCanvasPanelSlot* ControlsSlot = Root->AddChildToCanvas(Controls);
		ControlsSlot->SetAnchors(FAnchors(0.5f, 0.93f));
		ControlsSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ControlsSlot->SetAutoSize(true);
	}

	UButton* MakeTextButton(UWidgetTree* Tree, const TCHAR* ButtonName, const TCHAR* TextName, const TCHAR* Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		UTextBlock* Text = MakeText(Tree, TextName, 22, FSlateColor(FLinearColor::White));
		Text->SetAutoWrapText(false);
		Text->SetJustification(ETextJustify::Center);
		Text->SetText(FText::FromString(Label));
		Button->SetContent(Text);
		Button->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.9f));
		return Button;
	}

	void BuildQuantityDialogLayout(UWidgetTree* Tree)
	{
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Tree->RootWidget = Root;
		// 半透明遮罩：接住窗口外的点击，不穿透到背包
		UImage* Blocker = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Blocker"));
		Blocker->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.35f));
		AddToCanvas(Root, Blocker, FAnchors(0.f, 0.f, 1.f, 1.f));

		UBorder* Dialog = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_Dialog"));
		Dialog->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.045f, 0.95f));
		Dialog->SetPadding(FMargin(28.f, 20.f));
		UCanvasPanelSlot* DialogSlot = Root->AddChildToCanvas(Dialog);
		DialogSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		DialogSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		DialogSlot->SetAutoSize(true);

		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_Dialog"));
		Dialog->SetContent(Column);
		UTextBlock* Title = MakeText(Tree, TEXT("Text_Title"), 24, FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

		// < 数量 >
		UHorizontalBox* QuantityRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Quantity"));
		UVerticalBoxSlot* QuantitySlot = Column->AddChildToVerticalBox(QuantityRow);
		QuantitySlot->SetHorizontalAlignment(HAlign_Center);
		QuantitySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
		QuantityRow->AddChildToHorizontalBox(MakeTextButton(Tree, TEXT("Button_Decrease"), TEXT("Text_Decrease"), TEXT("<")))->SetVerticalAlignment(VAlign_Center);
		USizeBox* NumberBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_Quantity"));
		NumberBox->SetMinDesiredWidth(120.f);
		UTextBlock* Number = MakeText(Tree, TEXT("Text_Quantity"), 32, FSlateColor(FLinearColor::White));
		Number->SetAutoWrapText(false);
		Number->SetJustification(ETextJustify::Center);
		NumberBox->SetContent(Number);
		UHorizontalBoxSlot* NumberSlot = QuantityRow->AddChildToHorizontalBox(NumberBox);
		NumberSlot->SetVerticalAlignment(VAlign_Center);
		NumberSlot->SetPadding(FMargin(12.f, 0.f));
		QuantityRow->AddChildToHorizontalBox(MakeTextButton(Tree, TEXT("Button_Increase"), TEXT("Text_Increase"), TEXT(">")))->SetVerticalAlignment(VAlign_Center);

		// 确定 / 取消
		UHorizontalBox* Buttons = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Buttons"));
		UVerticalBoxSlot* ButtonsSlot = Column->AddChildToVerticalBox(Buttons);
		ButtonsSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonsSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		Buttons->AddChildToHorizontalBox(MakeTextButton(Tree, TEXT("Button_Confirm"), TEXT("Text_Confirm"), TEXT("OK")))->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		Buttons->AddChildToHorizontalBox(MakeTextButton(Tree, TEXT("Button_Cancel"), TEXT("Text_Cancel"), TEXT("Cancel")));

		UHorizontalBox* Controls = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Controls"));
		Column->AddChildToVerticalBox(Controls)->SetHorizontalAlignment(HAlign_Center);
	}

	void BuildItemInspectLayout(UWidgetTree* Tree)
	{
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Tree->RootWidget = Root;
		// 背包变暗变模糊
		UBackgroundBlur* Blur = Tree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass(), TEXT("Blur_Background"));
		Blur->SetBlurStrength(6.f);
		AddToCanvas(Root, Blur, FAnchors(0.f, 0.f, 1.f, 1.f));
		UImage* Dim = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Dim"));
		Dim->SetColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
		AddToCanvas(Root, Dim, FAnchors(0.f, 0.f, 1.f, 1.f));
		// 3D 模型在屏幕中央
		UScaleBox* ModelBox = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ScaleBox_Model"));
		ModelBox->SetStretch(EStretch::ScaleToFit);
		AddToCanvas(Root, ModelBox, FAnchors(0.25f, 0.06f, 0.75f, 0.70f));
		ModelBox->SetContent(Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Model")));
		// 名称和介绍在模型下方
		UVerticalBox* Info = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_Info"));
		AddToCanvas(Root, Info, FAnchors(0.25f, 0.72f, 0.75f, 0.90f));
		UTextBlock* Name = MakeText(Tree, TEXT("Text_Name"), 30, FSlateColor(FLinearColor::White));
		Name->SetJustification(ETextJustify::Center);
		Info->AddChildToVerticalBox(Name)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		UTextBlock* Description = MakeText(Tree, TEXT("Text_Description"), 20, FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.f)));
		Description->SetJustification(ETextJustify::Center);
		Info->AddChildToVerticalBox(Description);
		// 底部操作栏
		UHorizontalBox* Controls = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Controls"));
		UCanvasPanelSlot* ControlsSlot = Root->AddChildToCanvas(Controls);
		ControlsSlot->SetAnchors(FAnchors(0.5f, 0.95f));
		ControlsSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ControlsSlot->SetAutoSize(true);
	}

	void BuildPickupBannerLayout(UWidgetTree* Tree)
	{
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		Tree->RootWidget = Root;
		// 贴屏幕右侧；模型区域约占屏幕高 40%、宽 20%（背景图以后由美术替换）
		UBorder* Banner = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_Banner"));
		Banner->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.035f, 0.6f));
		Banner->SetPadding(FMargin(16.f));
		Banner->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddToCanvas(Root, Banner, FAnchors(0.79f, 0.24f, 0.99f, 0.80f));
		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VerticalBox_Banner"));
		Banner->SetContent(Column);
		UScaleBox* ModelBox = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ScaleBox_Model"));
		ModelBox->SetStretch(EStretch::ScaleToFit);
		Column->AddChildToVerticalBox(ModelBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ModelBox->SetContent(Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Model")));
		UTextBlock* Name = MakeText(Tree, TEXT("Text_Name"), 24, FSlateColor(FLinearColor::White));
		Name->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(Name);
		NameSlot->SetHorizontalAlignment(HAlign_Fill);
		NameSlot->SetPadding(FMargin(0.f, 8.f));
		UHorizontalBox* Hint = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HorizontalBox_Hint"));
		Column->AddChildToVerticalBox(Hint)->SetHorizontalAlignment(HAlign_Center);
	}

	/** 通用：创建或（-Rebuild 时备份后）重建一个 WBP */
	void EnsureWidgetBlueprint(const TCHAR* Path, UClass* ParentClass, TFunctionRef<void(UWidgetTree*)> Build, bool bRebuild, bool& bOk)
	{
		const FString Name = FPackageName::GetShortName(Path);
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *(FString(Path) + TEXT(".") + Name));
		const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
		if (Blueprint && !bRebuild)
		{
			UE_LOG(LogTADocumentUI, Display, TEXT("%s already exists; not overwritten (pass -Rebuild to back it up and regenerate)."), Path);
			return;
		}
		if (Blueprint)
		{
			const FString Backup = FPaths::ProjectSavedDir() / TEXT("LayoutBackups") /
				FString::Printf(TEXT("%s_%s.uasset"), *Name, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
			if (IFileManager::Get().Copy(*Backup, *Filename, false) != COPY_OK)
			{
				UE_LOG(LogTADocumentUI, Error, TEXT("Backup to %s failed; refusing to rebuild."), *Backup);
				bOk = false;
				return;
			}
			UE_LOG(LogTADocumentUI, Display, TEXT("Backed up old layout to %s."), *Backup);
			Blueprint->WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
		}
		else
		{
			UPackage* Package = CreatePackage(Path);
			Blueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
				ParentClass, Package, *Name, BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
			if (!Blueprint || !Blueprint->WidgetTree)
			{
				UE_LOG(LogTADocumentUI, Error, TEXT("Failed to create %s."), Path);
				bOk = false;
				return;
			}
			FAssetRegistryModule::AssetCreated(Blueprint);
		}

		Build(Blueprint->WidgetTree);
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		Blueprint->WidgetTree->ForEachWidget([Blueprint](UWidget* Widget)
		{
			Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
		});
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error)
		{
			UE_LOG(LogTADocumentUI, Error, TEXT("%s failed to compile."), Path);
			bOk = false;
			return;
		}
		bOk &= SaveAsset(Blueprint);
	}

	void EnsureReaderWidget(bool bRebuild, bool& bOk)
	{
		const FString Name = FPackageName::GetShortName(ReaderWidgetPath);
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *(FString(ReaderWidgetPath) + TEXT(".") + Name));
		const FString Filename = FPackageName::LongPackageNameToFilename(ReaderWidgetPath, FPackageName::GetAssetPackageExtension());
		if (Blueprint && !bRebuild)
		{
			UE_LOG(LogTADocumentUI, Display, TEXT("%s already exists; not overwritten (pass -Rebuild to back it up and regenerate)."), ReaderWidgetPath);
			return;
		}
		if (Blueprint)
		{
			// 重建前备份，手工调整过的布局可以从备份里找回
			const FString Backup = FPaths::ProjectSavedDir() / TEXT("LayoutBackups") /
				FString::Printf(TEXT("WBP_DocumentReader_%s.uasset"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
			if (IFileManager::Get().Copy(*Backup, *Filename, false) != COPY_OK)
			{
				UE_LOG(LogTADocumentUI, Error, TEXT("Backup to %s failed; refusing to rebuild."), *Backup);
				bOk = false;
				return;
			}
			UE_LOG(LogTADocumentUI, Display, TEXT("Backed up old layout to %s."), *Backup);
			Blueprint->WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
		}
		else
		{
			UPackage* Package = CreatePackage(ReaderWidgetPath);
			Blueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
				UTADocumentReaderWidget::StaticClass(), Package, *Name, BPTYPE_Normal,
				UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
			if (!Blueprint || !Blueprint->WidgetTree)
			{
				UE_LOG(LogTADocumentUI, Error, TEXT("Failed to create %s."), ReaderWidgetPath);
				bOk = false;
				return;
			}
			FAssetRegistryModule::AssetCreated(Blueprint);
		}

		BuildReaderLayout(Blueprint->WidgetTree);
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		Blueprint->WidgetTree->ForEachWidget([Blueprint](UWidget* Widget)
		{
			Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
		});
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error)
		{
			UE_LOG(LogTADocumentUI, Error, TEXT("%s failed to compile."), ReaderWidgetPath);
			bOk = false;
			return;
		}
		bOk &= SaveAsset(Blueprint);
	}
}

UTADocumentUICommandlet::UTADocumentUICommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UTADocumentUICommandlet::Main(const FString& Params)
{
	bool bOk = true;
	if (FParse::Param(*Params, TEXT("ConfigureDiaryInput")))
	{
		auto* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_UI.IMC_UI"));
		auto* Previous = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/UI/IA_DiaryPreviousPage.IA_DiaryPreviousPage"));
		auto* Next = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/UI/IA_DiaryNextPage.IA_DiaryNextPage"));
		if (!Context || !Previous || !Next) return 8;
		const FString Source = FPackageName::LongPackageNameToFilename(Context->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("InputMappingBackups") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) / TEXT("IMC_UI.uasset");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
		if (IFileManager::Get().Copy(*Backup, *Source, false) != COPY_OK) return 9;
		for (UInputAction* Action : {Previous, Next})
		{
			Context->UnmapKey(Action, EKeys::A);
			Context->UnmapKey(Action, EKeys::D);
			for (FKey Key : {Action == Previous ? EKeys::Q : EKeys::E,
				Action == Previous ? EKeys::Gamepad_LeftShoulder : EKeys::Gamepad_RightShoulder})
			{
				bool bFound = false;
				for (const auto& Mapping : Context->GetMappings()) bFound |= Mapping.Action == Action && Mapping.Key == Key;
				if (!bFound) Context->MapKey(Action, Key);
			}
		}
		for (const auto& Mapping : Context->GetMappings())
			if (Mapping.Action == Previous || Mapping.Action == Next)
				UE_LOG(LogTADocumentUI, Display, TEXT("Diary IMC: %s -> %s"), *Mapping.Action->GetName(), *Mapping.Key.ToString());
		return SaveAsset(Context) ? 0 : 10;
	}
	if (FParse::Param(*Params, TEXT("PreviewDiary")))
	{
		// Read-only render of the actual runtime widget; use -AllowCommandletRendering.
		// AllocateScene otherwise creates a null scene for this asset-generation commandlet.
		TGuardValue<bool> PreviewClient(GIsClient, true);
		if (!FSlateApplication::IsInitialized())
			FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer());
		TStrongObjectPtr<UGameInstance> PreviewGame(NewObject<UGameInstance>(GEngine));
		PreviewGame->InitializeStandalone();
		UWorld* World = PreviewGame->GetWorld();
		World->InitializeActorsForPlay(FURL());
		FString PreviewDocument;
		if (FParse::Value(*Params, TEXT("DiaryDocument="), PreviewDocument))
		{
			auto* Document = LoadObject<UTADocumentDefinition>(nullptr, *PreviewDocument);
			if (!Document) return 5;
			PreviewGame->GetSubsystem<UTADocumentSubsystem>()->Collect(Document->DocumentId);
		}
		if (FParse::Value(*Params, TEXT("DiaryNextDocument="), PreviewDocument))
		{
			auto* Document = LoadObject<UTADocumentDefinition>(nullptr, *PreviewDocument);
			if (!Document) return 5;
			PreviewGame->GetSubsystem<UTADocumentSubsystem>()->Collect(Document->DocumentId);
		}
		auto* Player = World->SpawnActor<APlayerController>();
		auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		LocalPlayer->PlayerController = Player;
		Player->Player = LocalPlayer;
		UClass* DiaryClass = LoadClass<UTADiaryWidget>(nullptr, TEXT("/Game/UI/Document/WBP_Diary.WBP_Diary_C"));
		UTADiaryWidget* Diary = DiaryClass ? CreateWidget<UTADiaryWidget>(Player, DiaryClass) : nullptr;
		if (!Diary) return 2;
		TStrongObjectPtr<UTADiaryWidget> KeepDiaryAlive(Diary);
		const bool bQuickReturn = FParse::Param(*Params, TEXT("DiaryQuickReturn"));
		Diary->bSkipOpenAnimation = FParse::Param(*Params, TEXT("DiaryQuick")) || bQuickReturn;
		if (bQuickReturn)
		{
			// The icon has no cached geometry when quick view is constructed, just like a freshly opened inventory.
			auto* Icon = Diary->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AuditReturnIcon"));
			FSlateBrush IconBrush; IconBrush.DrawAs = ESlateBrushDrawType::Box; IconBrush.TintColor = FLinearColor::Black;
			Icon->SetBrush(IconBrush);
			auto* IconSlot = Cast<UCanvasPanel>(Diary->WidgetTree->RootWidget)->AddChildToCanvas(Icon);
			IconSlot->SetPosition(FVector2D(1620.f, 250.f));
			IconSlot->SetSize(FVector2D(80.f, 100.f));
			IconSlot->SetZOrder(-1);
			Diary->SourceIconWidget = Icon;
		}
		if (FParse::Param(*Params, TEXT("DiaryFromIcon")))
		{
			Diary->SourcePosition = FVector2D(1660.f, 300.f);
			Diary->SourceSize = FVector2D(80.f, 100.f);
		}
		const auto Slate = Diary->TakeWidget();
		ATADiaryPreviewStage* Stage = nullptr;
		for (TActorIterator<ATADiaryPreviewStage> It(World); It; ++It) { Stage = *It; break; }
		if (!Stage || !Stage->IsReady()) return 3;
		Stage->Tick(2.f);
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		for (TActorIterator<ATAPreviewStage> It(World); It; ++It) It->SetCaptureEnabled(false);
		World->UpdateWorldComponents(true, false);
		UE_LOG(LogTADocumentUI, Display, TEXT("Diary render: visible=%d registered=%d renderstate=%d bounds=%s camera=%s mesh=%s"),
			Stage->GetSkeletalMeshComponent()->IsVisible(), Stage->GetSkeletalMeshComponent()->IsRegistered(),
			Stage->GetSkeletalMeshComponent()->IsRenderStateCreated(), *Stage->GetSkeletalMeshComponent()->Bounds.ToString(),
			*Stage->GetCaptureComponent()->GetComponentLocation().ToString(), *Stage->GetSkeletalMeshComponent()->GetComponentTransform().ToString());
		TStrongObjectPtr<ATADiaryPreviewStage> KeepStageAlive(Stage);
		if (FParse::Param(*Params, TEXT("DiaryTurn")))
		{
			Diary->TurnPage(1);
		}
		if (FParse::Param(*Params, TEXT("DiaryRead")))
		{
			Stage->BeginReading();
			Stage->UpdateReading(1.f, 1.f, 1.f);
		}
		if (FParse::Param(*Params, TEXT("DiaryHideBook")))
		{
			Stage->GetSkeletalMeshComponent()->SetVisibility(false);
			Stage->GetTurningPageComponent()->SetVisibility(false);
		}
		UE_LOG(LogTADocumentUI, Display, TEXT("Diary document root=%s mesh=%s bounds=%s"),
			*Stage->GetRightDocument()->GetComponentTransform().ToString(),
			*Stage->GetRightDocument()->GetSkeletalMeshComponent()->GetRelativeTransform().ToString(),
			*Stage->GetRightDocument()->GetSkeletalMeshComponent()->Bounds.ToString());
		float SampleTime = 1.2f;
		FParse::Value(*Params, TEXT("DiarySample="), SampleTime);
		for (int32 Frame = 0; Frame < 4; ++Frame)
		{
			++GFrameCounter;
			if (FParse::Param(*Params, TEXT("DiaryTurn"))) Stage->Tick(FMath::Max(0.f, SampleTime) * 0.25f);
			else if (Diary->bSkipOpenAnimation) Stage->Tick(0.016f);
			for (TActorIterator<ATAPreviewStage> It(World); It; ++It)
			{
				if (*It != Stage && It->GetSkeletalMeshComponent()->GetSkeletalMeshAsset())
				{
					It->GetSkeletalMeshComponent()->TickAnimation(0.016f, false);
					It->GetSkeletalMeshComponent()->RefreshBoneTransforms();
					It->GetSkeletalMeshComponent()->MarkRenderDynamicDataDirty();
				}
				if (FParse::Param(*Params, TEXT("DiaryUnlit"))) It->GetCaptureComponent()->ShowFlags.SetLighting(false);
			}
			World->SendAllEndOfFrameUpdates();
			for (TActorIterator<ATAPreviewStage> It(World); It; ++It)
				if (It->GetCaptureComponent()->TextureTarget) It->GetCaptureComponent()->CaptureScene();
			FlushRenderingCommands();
		}
		UE_LOG(LogTADocumentUI, Display, TEXT("Diary final motion=%d progress=%f moving=%s pageVisible=%d bone=%s"),
			int32(Stage->GetMotion()), Stage->GetMotionProgress(), *Stage->GetMovingDocument()->GetComponentTransform().ToString(),
			Stage->GetTurningPageComponent()->IsVisible(), *Stage->GetSkeletalMeshComponent()->GetSocketTransform(TEXT("Document_Attach")).ToString());
		for (TActorIterator<ATAPreviewStage> It(World); It; ++It)
		{
			if (*It == Stage) continue;
			FBufferArchive DocumentPixels;
			FImageUtils::ExportRenderTarget2DAsPNG(It->GetRenderTarget(), DocumentPixels);
			FFileHelper::SaveArrayToFile(DocumentPixels, *(FPaths::ProjectSavedDir() / TEXT("Diary3DDocument.png")));
		}
		FBufferArchive BookPixels;
		FImageUtils::ExportRenderTarget2DAsPNG(Stage->GetRenderTarget(), BookPixels);
		FFileHelper::SaveArrayToFile(BookPixels, *(FPaths::ProjectSavedDir() / TEXT("Diary3DBook.png")));
		FWidgetRenderer Renderer(true);
		auto* Target = Renderer.DrawWidget(Slate, FVector2D(1920, 1080));
		TStrongObjectPtr<UTextureRenderTarget2D> KeepTargetAlive(Target);
		if (bQuickReturn)
		{
			Diary->NativeTick(Diary->GetCachedGeometry(), 0.f);
			UE_LOG(LogTADocumentUI, Display, TEXT("Quick return before close: phase=%d ready=%d reading=%d icon=%s geometry=%s sameStage=%d"),
				int32(Diary->Phase), Stage->IsPresentationReady(), Stage->IsReading(), *GetNameSafe(Diary->SourceIconWidget.Get()),
				Diary->SourceIconWidget.IsValid() ? *Diary->SourceIconWidget->GetCachedGeometry().GetLocalSize().ToString() : TEXT("none"), Diary->BookStage == Stage);
			Diary->RequestClose();
			++GFrameCounter;
			Stage->Tick(Stage->CloseAnimation.Get()->GetPlayLength() * 0.999f);
			Diary->NativeTick(Diary->GetCachedGeometry(), 0.f);
			World->SendAllEndOfFrameUpdates();
			Stage->GetCaptureComponent()->CaptureScene();
			FlushRenderingCommands();
			Renderer.DrawWidget(Target, Slate, FVector2D(1920, 1080), 0.f);
			UE_LOG(LogTADocumentUI, Display, TEXT("Quick return icon: center=%s size=%s phase=%d motion=%d transform=%s"),
				*Diary->SourcePosition.Get(FVector2D::ZeroVector).ToString(), *Diary->SourceSize.Get(FVector2D::ZeroVector).ToString(),
				int32(Diary->Phase), int32(Stage->GetMotion()), *Diary->Panel_Book->GetRenderTransform().Translation.ToString());
		}
		if (FParse::Param(*Params, TEXT("DiaryFromIcon")))
		{
			Diary->NativeTick(Diary->GetCachedGeometry(), 0.f);
			Stage->Tick(FMath::Max(0.f, SampleTime));
			Diary->NativeTick(Diary->GetCachedGeometry(), 0.f);
			World->SendAllEndOfFrameUpdates();
			Stage->GetCaptureComponent()->CaptureScene();
			FlushRenderingCommands();
			Renderer.DrawWidget(Target, Slate, FVector2D(1920, 1080), 0.f);
		}
		FlushRenderingCommands();
		FBufferArchive Pixels;
		bOk = FImageUtils::ExportRenderTarget2DAsPNG(Target, Pixels)
			&& FFileHelper::SaveArrayToFile(Pixels, *(FPaths::ProjectSavedDir() / TEXT("Diary3DPreview.png")));
		Diary->RemoveFromParent();
		PreviewGame->Shutdown();
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return bOk ? 0 : 4;
	}
	if (FParse::Param(*Params, TEXT("RebuildDiaryOnly")))
	{
		EnsureWidgetBlueprint(TEXT("/Game/UI/Document/WBP_Diary"), UTADiaryWidget::StaticClass(),
			[](UWidgetTree* Tree) { BuildDiaryLayout(Tree); }, true, bOk);
		return bOk ? 0 : 1;
	}
	if (UInputAction* Scroll = EnsureAction(ScrollActionPath, EInputActionValueType::Axis1D, bOk))
	{
		EnsureScrollMappings(Scroll, bOk);
	}
	// 显示转译 / 旋转模型：只建动作，按键由策划在 IMC_UI 中自行配置
	EnsureAction(TEXT("/Game/Input/Actions/UI/IA_DocumentTranslate"), EInputActionValueType::Boolean, bOk);
	EnsureAction(TEXT("/Game/Input/Actions/UI/IA_DocumentRotate"), EInputActionValueType::Axis2D, bOk);
	EnsureReaderWidget(FParse::Param(*Params, TEXT("Rebuild")), bOk);

	// 物品交互菜单与日记本（1.8）：输入动作只建资源，按键在 IMC_UI 中自行配置
	EnsureAction(TEXT("/Game/Input/Actions/UI/IA_ItemActions"), EInputActionValueType::Boolean, bOk);
	EnsureAction(TEXT("/Game/Input/Actions/UI/IA_DiaryPreviousPage"), EInputActionValueType::Boolean, bOk);
	EnsureAction(TEXT("/Game/Input/Actions/UI/IA_DiaryNextPage"), EInputActionValueType::Boolean, bOk);
	const bool bRebuildDiary = FParse::Param(*Params, TEXT("RebuildDiary"));
	EnsureWidgetBlueprint(TEXT("/Game/UI/Inventory/WBP_ItemActionOption"), UTAItemActionOptionWidget::StaticClass(),
		[](UWidgetTree* Tree) { BuildItemActionOptionLayout(Tree); }, bRebuildDiary, bOk);
	EnsureWidgetBlueprint(TEXT("/Game/UI/Inventory/WBP_ItemActionMenu"), UTAItemActionMenuWidget::StaticClass(),
		[](UWidgetTree* Tree) { BuildItemActionMenuLayout(Tree); }, bRebuildDiary || FParse::Param(*Params, TEXT("RebuildMenu")), bOk);
	EnsureWidgetBlueprint(TEXT("/Game/UI/Document/WBP_Diary"), UTADiaryWidget::StaticClass(),
		[](UWidgetTree* Tree) { BuildDiaryLayout(Tree); }, bRebuildDiary, bOk);

	// 数量小窗口（拆分堆叠 / 部分丢弃）：调数量的输入动作只建资源，按键在 IMC_UI 中自行配置
	EnsureAction(TEXT("/Game/Input/Actions/UI/IA_QuantityDecrease"), EInputActionValueType::Boolean, bOk);
	EnsureAction(TEXT("/Game/Input/Actions/UI/IA_QuantityIncrease"), EInputActionValueType::Boolean, bOk);
	EnsureWidgetBlueprint(TEXT("/Game/UI/Inventory/WBP_QuantityDialog"), UTAQuantityDialogWidget::StaticClass(),
		[](UWidgetTree* Tree) { BuildQuantityDialogLayout(Tree); }, FParse::Param(*Params, TEXT("RebuildQuantity")), bOk);

	// 物品检视
	EnsureWidgetBlueprint(TEXT("/Game/UI/Inventory/WBP_ItemInspect"), UTAItemInspectWidget::StaticClass(),
		[](UWidgetTree* Tree) { BuildItemInspectLayout(Tree); }, FParse::Param(*Params, TEXT("RebuildInspect")), bOk);

	// 拾取提示
	EnsureWidgetBlueprint(TEXT("/Game/UI/PickUp/WBP_PickupBanner"), UTAPickupBannerWidget::StaticClass(),
		[](UWidgetTree* Tree) { BuildPickupBannerLayout(Tree); }, FParse::Param(*Params, TEXT("RebuildBanner")), bOk);
	return bOk ? 0 : 1;
}
