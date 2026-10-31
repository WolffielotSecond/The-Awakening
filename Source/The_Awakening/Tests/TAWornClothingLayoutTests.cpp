#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/Inventory/TAWornClothingLayout.h"
#include "UI/Inventory/TAInventoryPanelWidget.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "Inventory/TAClothingDefinition.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/NamedSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "RHI.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAWornLayoutTest, "TheAwakening.Inventory.Clothing.WornLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAWornLayoutTest::RunTest(const FString&)
{
	for (FVector2D Area : { FVector2D(350, 540), FVector2D(560, 810), FVector2D(280, 600) })
	{
		for (int32 Count = 0; Count <= 3; ++Count)
		{
			const int32 Combinations = 1 << (Count * 3);
			for (int32 Code = 0; Code < Combinations; ++Code)
			{
				TArray<FIntPoint> Shapes;
				for (int32 P = 0; P < Count; ++P)
				{
					const int32 Shape = (Code >> (P * 3)) & 7;
					Shapes.Add(FIntPoint(1 + Shape % 4, 1 + Shape / 4));
				}
				const auto L = TAWornClothingLayout::Calculate(Area, { FIntPoint(2, 1) }, Shapes);
				TestTrue(TEXT("Inner fits available area"), L.Inner.Position.Y >= 0 && L.Inner.Size.X <= Area.X + .01 && L.Inner.Position.Y + L.Inner.Size.Y <= Area.Y + .01);
				if (Count == 0)
				{
					TestTrue(TEXT("Uncoated inner group vertically centered"), FMath::IsNearlyEqual(L.Inner.Position.Y + L.Inner.Size.Y / 2, Area.Y / 2));
					continue;
				}
				TestTrue(TEXT("Outer fits and stays below inner"), L.Outer.Position.Y > L.Inner.Position.Y + L.Inner.Size.Y && L.Outer.Position.Y + L.Outer.Size.Y <= Area.Y + .01 && L.Outer.Size.X <= Area.X + .01);
				for (int32 P = 0; P < Count; ++P)
				{
					TestTrue(TEXT("All pocket grids share one left edge"), FMath::IsNearlyEqual(L.Outer.Pockets[P].X, L.Inner.Pockets[0].X));
					const float Bottom = L.Outer.Pockets[P].Y + Shapes[P].Y * L.Cell;
					TestTrue(TEXT("Pocket stays inside padded outer frame"), Bottom <= L.Outer.Size.Y - L.Padding + .01f);
					if (P + 1 < Count) TestTrue(TEXT("Pocket groups do not overlap"), Bottom < L.Outer.Pockets[P + 1].Y);
				}
			}
		}
	}
#if WITH_EDITOR
	auto* Def = NewObject<UTAClothingDefinition>();
	Def->PocketCount = 7;
	FPropertyChangedEvent CountEvent(FindFProperty<FProperty>(UTAClothingDefinition::StaticClass(), TEXT("PocketCount")));
	Def->PostEditChangeProperty(CountEvent);
	TestEqual(TEXT("Editor caps pocket count at 3"), Def->Pockets.Num(), 3);
	Def->Pockets[0].Columns = 9; Def->Pockets[0].Rows = 9;
	Def->PostEditChangeProperty(CountEvent);
	TestEqual(TEXT("Editor caps columns at 4"), Def->Pockets[0].Columns, 4);
	TestEqual(TEXT("Editor caps rows at 2"), Def->Pockets[0].Rows, 2);
	FDataValidationContext Validation;
	TestTrue(TEXT("Normalized definition validates"), Def->IsDataValid(Validation) == EDataValidationResult::Valid);
	Def->PocketCount = 0; Def->PostEditChangeProperty(CountEvent);
	TestEqual(TEXT("Editor requires at least one pocket"), Def->Pockets.Num(), 1);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAWornPresentationTest, "TheAwakening.Inventory.Clothing.WornPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAWornPresentationTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	auto* PC = World->SpawnActor<APlayerController>();
	auto* Local = NewObject<ULocalPlayer>(GEngine); Local->PlayerController = PC; PC->Player = Local;
	World->AddController(PC);
	auto* Data = NewObject<UTAInventoryComponent>(PC);
	PC->AddInstanceComponent(Data); Data->RegisterComponent();
	Data->DefaultInnerClothing = LoadObject<UTAClothingDefinition>(nullptr, TEXT("/Game/Inventory/DA_Clothing_Inner_Default.DA_Clothing_Inner_Default"));
	Data->BeginPlay();
	auto* Item = NewObject<UTAItemDefinition>(); Data->TryAddItem(Item, 1);
	auto* Class = LoadClass<UTAInventoryPanelWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_InventoryPanel.WBP_InventoryPanel_C"));
	auto* UI = CreateWidget<UTAInventoryPanelWidget>(PC, Class);
	if (!TestNotNull(TEXT("Actual inventory Blueprint loads"), UI)) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	UI->Init(Data);
	const auto Surface = UI->TakeWidget();
	auto* Named = Cast<UNamedSlot>(UI->GetWidgetFromName(TEXT("NamedSlot_InnerClothing")));
	auto* Panel = Named ? Cast<UTAClothingPanelWidget>(Named->GetContent()) : nullptr;
	if (!TestNotNull(TEXT("Combined panel installed in real inventory"), Panel)) { UI->NativeDestruct(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	auto* Placement = Cast<UCanvasPanelSlot>(Named->Slot);
	TestNotNull(TEXT("Clothing placement is a canvas slot"), Placement);
	if (Placement) TestTrue(TEXT("Combined panel spans the clothing and pocket regions"), FMath::IsNearlyEqual(Placement->GetAnchors().Maximum.X, .912f));
	auto* Image = Cast<UImage>(Panel->GetWidgetFromName(TEXT("Image_Clothing")));
	TestTrue(TEXT("Inner retains its existing placeholder texture"), Image && Image->GetBrush().GetResourceObject() == Data->DefaultInnerClothing->DisplayTexture.LoadSynchronous());
	TestEqual(TEXT("Only inner cells initially"), Panel->GetGeneratedSlots().Num(), 2);
	auto* OuterFrame = Panel->GetWidgetFromName(TEXT("OuterPocketFrame"));
	TestTrue(TEXT("Outer frame hidden without a coat"), OuterFrame && OuterFrame->GetVisibility() == ESlateVisibility::Collapsed);

	FWidgetRenderer* Renderer = !GUsingNullRHI && FParse::Param(FCommandLine::Get(), TEXT("ClothingPreview")) ? new FWidgetRenderer(true) : nullptr;
	auto Capture = [&](const TCHAR* Name)
	{
		if (!Renderer) return;
#if WITH_EDITOR
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
		UTextureRenderTarget2D* Target = nullptr;
		for (int32 Pass = 0; Pass < 4; ++Pass)
		{
			++GFrameCounter; Surface->SlatePrepass(1.f);
			Target = Renderer->DrawWidget(Surface, FVector2D(1600, 1000)); FlushRenderingCommands();
		}
		TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(1600, 1000, Pixels, PNG);
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("ClothingPreview");
		IFileManager::Get().MakeDirectory(*Dir, true);
		TestTrue(TEXT("Preview written"), FFileHelper::SaveArrayToFile(PNG, *(Dir / (FString(Name) + TEXT(".png")))));
		for (const TCHAR* WidgetName : { TEXT("NamedSlot_OuterEquip"), TEXT("NamedSlot_Story0") })
		{
			UWidget* W = UI->GetWidgetFromName(WidgetName);
			const auto Geometry = W->GetCachedGeometry();
			const FVector2D BottomRight = UI->GetCachedGeometry().AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
			TestTrue(TEXT("Existing story/equipment controls remain inside the screen"), BottomRight.X <= 1600.01 && BottomRight.Y <= 1000.01);
		}
		const auto Root = UI->GetCachedGeometry();
		const auto Equipment = UI->GetWidgetFromName(TEXT("NamedSlot_OuterEquip"))->GetCachedGeometry();
		const auto Story = UI->GetWidgetFromName(TEXT("NamedSlot_Story0"))->GetCachedGeometry();
		const auto Inner = Panel->GetWidgetFromName(TEXT("InnerPocketFrame"))->GetCachedGeometry();
		const float LeftGap = Root.AbsoluteToLocal(Story.LocalToAbsolute(FVector2D::ZeroVector)).X - 1600.f * .041f;
		const float RightGap = 1600.f * .952f - Root.AbsoluteToLocal(Equipment.LocalToAbsolute(Equipment.GetLocalSize())).X;
		TestTrue(TEXT("Equipment and story have matching gaps to their decorative rails"), FMath::IsNearlyEqual(LeftGap, RightGap, 1.f));
		const float EquipY = Root.AbsoluteToLocal(Equipment.LocalToAbsolute(Equipment.GetLocalSize() * .5f)).Y;
		const float InnerY = Root.AbsoluteToLocal(Inner.LocalToAbsolute(Inner.GetLocalSize() * .5f)).Y;
		TestTrue(TEXT("Equipment follows the center of the inner-pocket row"), FMath::IsNearlyEqual(EquipY, InnerY, 1.f));


	};
	Capture(TEXT("InnerOnly"));
	auto* Coat = NewObject<UTAClothingDefinition>(); Coat->Layer = ETAClothingLayer::Outer;
	Coat->Icon = Data->DefaultInnerClothing->Icon;
	Coat->DisplayTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Common/T_DefaultItemIcon.T_DefaultItemIcon")));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FTAPocketDef Pocket; Pocket.PocketId = FName(*FString::Printf(TEXT("P%d"), Index)); Pocket.Columns = 4; Pocket.Rows = 2; Coat->Pockets.Add(Pocket);
	}
	FTAClothingInstance Rack; UTAInventoryComponent::BuildClothingInstance(Coat, Rack);
	Data->EquipOuterInstance(Rack); UI->RefreshAll();
	TestEqual(TEXT("All maximum-capacity coat cells generated"), Panel->GetGeneratedSlots().Num(), 26);
	TestTrue(TEXT("Coat supplies the single displayed image"), Image && Image->GetBrush().GetResourceObject() == Coat->DisplayTexture.LoadSynchronous());
	auto* EquipContainer = Cast<UNamedSlot>(UI->GetWidgetFromName(TEXT("NamedSlot_OuterEquip")));
	auto* EquipWidget = EquipContainer ? Cast<UTAInventorySlotWidget>(EquipContainer->GetContent()) : nullptr;
	auto* EquipImage = EquipWidget ? Cast<UImage>(EquipWidget->GetWidgetFromName(TEXT("Image_item"))) : nullptr;
	TestTrue(TEXT("Equipment slot reads its separate small icon"), EquipImage && EquipImage->GetBrush().GetResourceObject() == Coat->Icon.LoadSynchronous());
	TestTrue(TEXT("Equipment and full clothing images can differ"), EquipImage && Image && EquipImage->GetBrush().GetResourceObject() != Image->GetBrush().GetResourceObject());
	TestTrue(TEXT("Outer frame visible with a coat"), OuterFrame->GetVisibility() == ESlateVisibility::HitTestInvisible);
	for (int32 Index = 0; Index < Panel->GetGeneratedSlots().Num(); ++Index)
	{
		auto* Slot = Cast<UTAInventorySlotWidget>(Panel->GetGeneratedSlots()[Index]);
		TestTrue(TEXT("UI cells retain inner-first inventory indices"), Slot && Slot->GetFlatIndex() == Index);
	}
	Capture(TEXT("ThreePocketsMaximum"));
	Coat->Pockets[0].Columns = 1; Coat->Pockets[0].Rows = 1;
	Coat->Pockets[1].Columns = 3; Coat->Pockets[1].Rows = 1;
	Coat->Pockets[2].Columns = 2; Coat->Pockets[2].Rows = 2;
	Rack = {}; Data->SwapOuter(Rack); Rack = {}; UTAInventoryComponent::BuildClothingInstance(Coat, Rack); Data->EquipOuterInstance(Rack); UI->RefreshAll();
	TestEqual(TEXT("Mixed pocket dimensions rebuild cached layout"), Panel->GetGeneratedSlots().Num(), 10);
	Capture(TEXT("MixedPockets"));
	Rack = {}; Data->SwapOuter(Rack); UI->RefreshAll();
	TestEqual(TEXT("Removing coat restores inner capacity"), Panel->GetGeneratedSlots().Num(), 2);
	TestEqual(TEXT("Inner item survives every layout change"), Data->GetItemCount(Item), 1);
	TestTrue(TEXT("Removing coat restores inner image"), Image && Image->GetBrush().GetResourceObject() == Data->DefaultInnerClothing->DisplayTexture.LoadSynchronous());
	auto* RackClass = LoadClass<UTAClothingPanelWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ClothingPanel.WBP_ClothingPanel_C"));
	auto* RackPanel = CreateWidget<UTAClothingPanelWidget>(PC, RackClass);
	RackPanel->TakeWidget(); RackPanel->BuildFromClothing(Rack);
	TestEqual(TEXT("Rack retains its independent clothing/pocket renderer"), RackPanel->GetGeneratedSlots().Num(), 8);
	if (Renderer) BeginCleanup(Renderer);
	UI->NativeDestruct(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}
#endif
