#include "TAPuzzleLayoutCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Puzzle/TAPathPuzzleWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Brushes/SlateColorBrush.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "Puzzle/TAPuzzleScopeLayout.h"
#include "Puzzle/TAPathPuzzleNodeWidget.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Misc/FileHelper.h"
#include "Serialization/BufferArchive.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Modules/ModuleManager.h"

namespace
{
bool CreateScopeMaterial(const TCHAR* Name, bool bMask, bool bLeft)
{
    const FString Path = FString(TEXT("/Game/UI/Minigame/Materials/")) + Name;
    if (LoadObject<UMaterial>(nullptr, *(Path + TEXT(".") + Name))) return true;
    UPackage* Package = CreatePackage(*Path);
    UMaterial* Material = NewObject<UMaterial>(Package, Name, RF_Public | RF_Standalone);
    Material->MaterialDomain = MD_UI; Material->BlendMode = BLEND_Translucent;
    auto* Custom = NewObject<UMaterialExpressionCustom>(Material);
    Custom->Inputs.Reset();
    Custom->OutputType = CMOT_Float1;
    Material->GetExpressionCollection().AddExpression(Custom);
    auto* UV = NewObject<UMaterialExpressionTextureCoordinate>(Material);
    Material->GetExpressionCollection().AddExpression(UV);
    FCustomInput UVInput; UVInput.InputName = TEXT("UV"); UVInput.Input.Expression = UV; Custom->Inputs.Add(UVInput);
    auto Scalar = [&](const TCHAR* Parameter, float Value)
    {
        auto* Expression = NewObject<UMaterialExpressionScalarParameter>(Material);
        Expression->ParameterName = Parameter; Expression->DefaultValue = Value;
        Material->GetExpressionCollection().AddExpression(Expression);
        FCustomInput Input; Input.InputName = Parameter; Input.Input.Expression = Expression; Custom->Inputs.Add(Input);
    };
    if (bMask)
    {
        Scalar(TEXT("CenterX"), .5f); Scalar(TEXT("CenterY"), .5f);
        Scalar(TEXT("Aspect"), 1920.f/1080.f); Scalar(TEXT("Radius"), 300.f/1080.f);
        Custom->Code = TEXT("float2 p=(UV-float2(CenterX,CenterY))*float2(Aspect,1); float d=length(p); return smoothstep(Radius-0.001,Radius+0.001,d);");
    }
    else
    {
        Scalar(TEXT("Fill"), 1.f); Scalar(TEXT("Side"), bLeft ? -1.f : 1.f);
        Scalar(TEXT("Thickness"), .018f);
        Custom->Code = TEXT("float2 p=UV-0.5; float r=length(p); float a=abs(atan2(p.x,-p.y))/3.14159265; float ring=1-smoothstep(Thickness*0.5,Thickness*0.5+0.002,abs(r-0.46)); float side=step(0,p.x*Side); float ends=step(0.06,a)*step(a,0.94); float filled=step(a,0.06+0.88*saturate(Fill)); return ring*side*ends*lerp(0.15,1.0,filled);");
    }
    auto* Color = NewObject<UMaterialExpressionConstant3Vector>(Material);
    Color->Constant = bMask ? FLinearColor::Black : (bLeft ? FLinearColor(1,.8f,.05f) : FLinearColor(.1f,.8f,.2f));
    Material->GetExpressionCollection().AddExpression(Color);
    Material->GetEditorOnlyData()->EmissiveColor.Expression = Color;
    Material->GetEditorOnlyData()->Opacity.Expression = Custom;
    Material->PostEditChange();
    FAssetRegistryModule::AssetCreated(Material);
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
    const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    return UPackage::SavePackage(Package, Material, *Filename, Args);
}
}

UTAPuzzleLayoutCommandlet::UTAPuzzleLayoutCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UTAPuzzleLayoutCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/UI/Minigame/WBP_TestPuzzle");
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, AssetPath);
    if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(UTAPathPuzzleWidget::StaticClass())) return 1;
    UWidgetTree* WidgetTree = Blueprint->WidgetTree;
    if (Params.Contains(TEXT("AddPrompts")))
    {
        TAPuzzleScopeLayout::AddLocalizedPrompts(WidgetTree);
        WidgetTree->ForEachWidget([Blueprint](UWidget* Widget) {
            if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
                Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
        });
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (Blueprint->Status == BS_Error) return 15;
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
        const FString File = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
        return UPackage::SavePackage(Blueprint->GetOutermost(),Blueprint,*File,Args) ? 0 : 16;
    }
    if (Params.Contains(TEXT("TuneScope")))
    {
        if (!WidgetTree) return 10;
        auto MoveNumber = [WidgetTree](FName Name, FVector2D Position)
        {
            if (UWidget* Widget = WidgetTree->FindWidget(Name))
                if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
                {
                    Slot->SetPosition(Position);
                    return true;
                }
            return false;
        };
        if (!MoveNumber(TEXT("Text_Energy"), FVector2D(155, 485)) ||
            !MoveNumber(TEXT("Text_Time"), FVector2D(355, 485))) return 11;
        if (!WidgetTree->FindWidget(TEXT("Button_Undo")))
        {
            UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
            if (!Root) return 14;
            UButton* UndoButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Undo"));
            UTextBlock* UndoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Undo"));
            UndoText->SetText(FText::FromString(TEXT("Undo")));
            UndoText->SetJustification(ETextJustify::Center);
            UndoButton->AddChild(UndoText);
            UCanvasPanelSlot* Slot = Root->AddChildToCanvas(UndoButton);
            Slot->SetAnchors(FAnchors(1, 1)); Slot->SetAlignment(FVector2D(1, 1));
            Slot->SetOffsets(FMargin(-30, -25, 200, 50));
            UndoButton->SetVisibility(ESlateVisibility::Collapsed);
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (Blueprint->Status == BS_Error) return 12;
        const FString TuneFilename = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
        FSavePackageArgs TuneArgs; TuneArgs.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *TuneFilename, TuneArgs)) return 13;
        UE_LOG(LogTemp, Display, TEXT("PUZZLE_SCOPE_LAYOUT_TUNED"));
        return 0;
    }
    if (Params.Contains(TEXT("Verify")))
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (!WidgetTree || !WidgetTree->RootWidget || Blueprint->Status == BS_Error) return 6;
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        APlayerController* Player = World->SpawnActor<APlayerController>();
        ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
        LocalPlayer->PlayerController = Player;
        Player->Player = LocalPlayer;
        UTAPathPuzzleWidget* Instance = CreateWidget<UTAPathPuzzleWidget>(Player, Blueprint->GeneratedClass.Get());
        bool bBindingsValid = Instance != nullptr;
        for (const FName Name : {FName("PuzzleCanvas"), FName("ScopeFrame"), FName("ScopeInstruments"), FName("Crosshair"),
            FName("Text_Energy"), FName("Text_Time"), FName("Image_ScopeMask"), FName("Image_EnergyArc"), FName("Image_TimeArc")})
        {
            const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(UTAPathPuzzleWidget::StaticClass(), Name);
            if (!Instance || !Property || !Property->GetObjectPropertyValue_InContainer(Instance))
            {
                bBindingsValid = false;
                UE_LOG(LogTemp, Error, TEXT("Missing runtime binding: %s"), *Name.ToString());
            }
        }
        const bool bStarted = bBindingsValid && Instance->StartPuzzle();
        UCanvasPanel* Board = Instance ? Cast<UCanvasPanel>(Instance->GetWidgetFromName(TEXT("PuzzleCanvas"))) : nullptr;
        const bool bValid = bStarted && Board && Board->GetChildrenCount() > 0;
        if (bValid && Params.Contains(TEXT("Render")))
        {
            if (!FSlateApplication::IsInitialized())
                FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer());
            if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
            FWidgetRenderer Renderer(true);
            const auto SlateWidget = Instance->TakeWidget();
            auto* Target = Renderer.DrawWidget(SlateWidget, FVector2D(1920,1080));
            for (int32 Frame = 0; Frame < 3; ++Frame)
            {
                FlushRenderingCommands();
                Instance->RefreshScopePresentation();
                Renderer.DrawWidget(Target, SlateWidget, FVector2D(1920,1080), 0.f);
            }
            FlushRenderingCommands();
            UE_LOG(LogTemp, Display, TEXT("SCOPE_ART nodeclass=%s nodesize=%s normaldraw=%d endpointdraw=%d"),
                *GetNameSafe(Instance->NodeWidgetClass.Get()), *Instance->Appearance.NodeSize.ToString(),
                int32(Instance->Appearance.NodeNormal.DrawAs), int32(Instance->Appearance.NodeEndpoint.DrawAs));
            for (UWidget* Child : Board->GetAllChildren())
                if (auto* Node = Cast<UTAPathPuzzleNodeWidget>(Child))
                {
                    auto* NodeImage = Cast<UImage>(Node->GetWidgetFromName(TEXT("Image_Node")));
                    UE_LOG(LogTemp, Display, TEXT("SCOPE_NODE class=%s size=%s pos=%s image=%s draw=%d"),
                        *Node->GetClass()->GetName(), *Node->GetCachedGeometry().GetLocalSize().ToString(),
                        *Node->GetCachedGeometry().GetAbsolutePosition().ToString(), *GetNameSafe(NodeImage),
                        NodeImage ? int32(NodeImage->GetBrush().DrawAs) : -1);
                }
            FBufferArchive Pixels;
            if (!FImageUtils::ExportRenderTarget2DAsPNG(Target, Pixels) ||
                !FFileHelper::SaveArrayToFile(Pixels, *(FPaths::ProjectSavedDir() / TEXT("PuzzleMicroscopePreview.png")))) return 9;
        }
        UE_LOG(LogTemp, Display, TEXT("PUZZLE_LAYOUT_VERIFY bindings=%d started=%d children=%d valid=%d"),
            bBindingsValid, bStarted, Board ? Board->GetChildrenCount() : 0, bValid);
        World->DestroyWorld(false);
        return bValid ? 0 : 7;
    }
    const bool bMicroscope = Params.Contains(TEXT("Microscope"));
    if (!WidgetTree || (WidgetTree->RootWidget && !bMicroscope))
    {
        UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite an existing Designer layout.")); return 2;
    }
    const FString Filename = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
    const FString Backup = FPaths::ProjectSavedDir() / (bMicroscope ? TEXT("LayoutBackups/WBP_TestPuzzle_before_microscope.uasset") : TEXT("LayoutBackups/WBP_TestPuzzle_before_designer.uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
    if (IFileManager::Get().FileExists(*Backup) || IFileManager::Get().Copy(*Backup, *Filename, false) != COPY_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("Backup unavailable or already exists; refusing migration.")); return 3;
    }
    const FTAPuzzleAppearance Appearance = Blueprint->GeneratedClass->GetDefaultObject<UTAPathPuzzleWidget>()->Appearance;
    if (bMicroscope)
    {
        if (!CreateScopeMaterial(TEXT("M_PuzzleScopeMask"), true, false) ||
            !CreateScopeMaterial(TEXT("M_PuzzleEnergyArc"), false, true) ||
            !CreateScopeMaterial(TEXT("M_PuzzleTimeArc"), false, false)) return 8;
        WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
        TAPuzzleScopeLayout::Build(Blueprint->WidgetTree, Appearance);
        // Replacing a designer tree must also replace its name/GUID registry.
        Blueprint->WidgetVariableNameToGuidMap.Reset();
        Blueprint->WidgetTree->ForEachWidget([Blueprint](UWidget* Widget)
        {
            Blueprint->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
        });
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (Blueprint->Status == BS_Error) return 4;
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args)) return 5;
        UE_LOG(LogTemp, Display, TEXT("PUZZLE_MICROSCOPE_SAVED %s"), *Filename);
        return 0;
    }
    TObjectPtr<UCanvasPanel> PuzzleCanvas;
    TObjectPtr<UProgressBar> Progress_Time;
    TObjectPtr<UTextBlock> Text_Time, Text_Stats, Text_Path, Text_Effects, Text_Undo, Text_Retry;
    TObjectPtr<UButton> Button_Undo, Button_Retry;

	// Background fills the viewport independently of the aspect-preserving puzzle content.
	UCanvasPanel* Screen = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScreenRoot"));
	WidgetTree->RootWidget = Screen;
	auto FillScreen = [Screen](UWidget* Child)
	{
		UCanvasPanelSlot* ScreenSlot = Screen->AddChildToCanvas(Child);
		ScreenSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		ScreenSlot->SetOffsets(FMargin(0.f));
	};
	UImage* Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Background"));
	Background->SetBrush(FSlateColorBrush(FLinearColor(.025f, .035f, .055f, 1.f)));
	FillScreen(Background);
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("Scale_Content"));
	FillScreen(Scale);
	Scale->SetStretch(EStretch::ScaleToFit);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size_Content"));
	Size->SetWidthOverride(Appearance.BoardSize.X + 300); Size->SetHeightOverride(Appearance.BoardSize.Y + 90);
	Scale->AddChild(Size);
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ContentCanvas")); Size->AddChild(Root);
	auto Place = [](UCanvasPanel* Canvas, UWidget* Child, FVector2D Position, FVector2D Extent)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child); CanvasSlot->SetPosition(Position); CanvasSlot->SetSize(Extent);
	};
	PuzzleCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PuzzleCanvas"));
	Place(Root, PuzzleCanvas, FVector2D(15, 55), Appearance.BoardSize);
	Progress_Time = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress_Time"));
	Place(Root, Progress_Time, FVector2D(20, 20), FVector2D(450, 18));
	auto AddText = [&](TObjectPtr<UTextBlock>& Text, FName Name, FVector2D Pos, FVector2D Extent)
	{
		Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont(); Font.Size = 15; Text->SetFont(Font); Text->SetAutoWrapText(true);
		Place(Root, Text, Pos, Extent);
	};
	AddText(Text_Time, TEXT("Text_Time"), FVector2D(485, 16), FVector2D(120, 25));
	const float X = Appearance.BoardSize.X + 30;
	AddText(Text_Stats, TEXT("Text_Stats"), FVector2D(X, 55), FVector2D(250, 140));
	AddText(Text_Path, TEXT("Text_Path"), FVector2D(X, 205), FVector2D(250, 85));
	AddText(Text_Effects, TEXT("Text_Effects"), FVector2D(X, 300), FVector2D(250, 115));
	auto AddButton = [&](TObjectPtr<UButton>& Button, TObjectPtr<UTextBlock>& Text, FName Name, float Y)
	{
		Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name == TEXT("Button_Undo") ? TEXT("Text_Undo") : TEXT("Text_Retry"));
		Text->SetColorAndOpacity(FLinearColor::Black);
		Button->AddChild(Text); Place(Root, Button, FVector2D(X, Y), FVector2D(235, 40));
	};
	AddButton(Button_Undo, Text_Undo, TEXT("Button_Undo"), 440);
	AddButton(Button_Retry, Text_Retry, TEXT("Button_Retry"), 490);

    Background->SetVisibility(ESlateVisibility::HitTestInvisible);
    Progress_Time->SetPercent(1.f);
    Progress_Time->SetFillColorAndOpacity(Appearance.TimerColor);
    Text_Time->SetText(FText::FromString(TEXT("60s")));
    Text_Stats->SetText(FText::FromString(TEXT("Energy: 0 / 0\nUnits: 0 / 0")));
    Text_Path->SetText(FText::FromString(TEXT("Path")));
    Text_Effects->SetText(FText::FromString(TEXT("Effects")));
    Text_Undo->SetText(FText::FromString(TEXT("Undo (0)")));
    Text_Retry->SetText(FText::FromString(TEXT("Retry (0)")));
    // These are Designer placeholders only; C++ updates all gameplay values at runtime.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (Blueprint->Status == BS_Error) return 4;
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    if (!UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, SaveArgs)) return 5;
    UE_LOG(LogTemp, Display, TEXT("PUZZLE_LAYOUT_SAVED %s; backup %s"), *Filename, *Backup);
    return 0;
}
