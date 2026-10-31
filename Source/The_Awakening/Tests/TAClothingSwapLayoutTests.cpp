#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/Inventory/TAClothingSwapWidget.h"
#include "UI/Inventory/TAClothingPanelWidget.h"
#include "UI/Inventory/TAInventorySlotWidget.h"
#include "Inventory/TAClothingDefinition.h"
#include "Inventory/TAClothingRack.h"
#include "Inventory/TAItemDefinition.h"
#include "Components/NamedSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
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
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAClothingSwapLayoutTest, "TheAwakening.Inventory.Clothing.SwapLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAClothingSwapLayoutTest::RunTest(const FString&)
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
	auto MakeCoat = [&]()
	{
		auto* Def = NewObject<UTAClothingDefinition>(); Def->Layer = ETAClothingLayer::Outer; Def->PocketCount = 3;
		Def->Icon = Data->DefaultInnerClothing->Icon;
		Def->DisplayTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Common/T_DefaultItemIcon.T_DefaultItemIcon")));
		// Deliberately different pictures prove that the left reads the rack-only field.
		Def->RackDisplayTexture = Data->DefaultInnerClothing->DisplayTexture;
		for (int32 P = 0; P < 3; ++P)
		{
			FTAPocketDef Pocket; Pocket.PocketId = FName(*FString::Printf(TEXT("P%d"), P)); Pocket.Columns = 4; Pocket.Rows = 2;
			Def->Pockets.Add(Pocket);
		}
		return Def;
	};
	auto* A = MakeCoat(); auto* B = MakeCoat();
	auto* Rack = World->SpawnActor<ATAClothingRack>();
	UTAInventoryComponent::BuildClothingInstance(A, Rack->GetStoredClothingMutable());
	Rack->GetStoredClothingMutable().Pockets[0].Slots[0].ItemDef = Item;
	Rack->GetStoredClothingMutable().Pockets[0].Slots[0].Count = 1;
	auto* Class = LoadClass<UTAClothingSwapWidget>(nullptr, TEXT("/Game/UI/Inventory/WBP_ClothingSwap.WBP_ClothingSwap_C"));
	auto* UI = CreateWidget<UTAClothingSwapWidget>(PC, Class);
	if (!TestNotNull(TEXT("Actual clothing swap Blueprint loads"), UI)) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
	UI->Init(Rack, Data);
	const auto Surface = UI->TakeWidget();
	// This isolated test world has no localization subsystem; use its verified Chinese labels for previews.
	if (auto* Title = Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("Text_RackTitle")))) Title->SetText(FText::FromString(TEXT("衣架")));
	if (auto* Title = Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("Text_PlayerTitle")))) Title->SetText(FText::FromString(TEXT("身上")));
	auto Content = [&](const TCHAR* Name)
	{
		auto* Named = Cast<UNamedSlot>(UI->GetWidgetFromName(Name));
		return Named ? Named->GetContent() : nullptr;
	};
	auto* Left = Cast<UTAClothingPanelWidget>(Content(TEXT("NamedSlot_RackClothing")));
	auto* Right = Cast<UTAClothingPanelWidget>(Content(TEXT("NamedSlot_InnerClothing")));
	auto* RackSlot = Cast<UTAInventorySlotWidget>(Content(TEXT("NamedSlot_RackCoat")));
	auto* PlayerSlot = Cast<UTAInventorySlotWidget>(Content(TEXT("NamedSlot_PlayerCoat")));
	if (!TestNotNull(TEXT("Rack body"), Left) || !TestNotNull(TEXT("Player body"), Right) || !TestNotNull(TEXT("Rack coat slot"), RackSlot) || !TestNotNull(TEXT("Player coat slot"), PlayerSlot))
	{
		UI->Close(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false;
	}
	TestEqual(TEXT("Rack has all 24 coat cells"), Left->GetGeneratedSlots().Num(), 24);
	TestEqual(TEXT("Uncoated player has 2 inner cells"), Right->GetGeneratedSlots().Num(), 2);
	TestTrue(TEXT("Rack does not display a fictitious inner pocket"), Left->GetWidgetFromName(TEXT("InnerPocketFrame"))->GetVisibility() == ESlateVisibility::Collapsed);
	auto* LeftImage = Cast<UImage>(Left->GetWidgetFromName(TEXT("Image_Clothing")));
	TestTrue(TEXT("Rack uses RackDisplayTexture"), LeftImage && LeftImage->GetBrush().GetResourceObject() == A->RackDisplayTexture.LoadSynchronous());

	FWidgetRenderer* Renderer = !GUsingNullRHI && FParse::Param(FCommandLine::Get(), TEXT("ClothingPreview")) ? new FWidgetRenderer(true) : nullptr;
	auto Bounds = [&](UWidget* W)
	{
		const auto Root = UI->GetCachedGeometry(); const auto G = W->GetCachedGeometry();
		const FVector2D TL = Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D BR = Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));
		return FSlateRect(TL.X, TL.Y, BR.X, BR.Y);
	};
	auto Capture = [&](const TCHAR* Name, FVector2D View = FVector2D(1600, 1000))
	{
		if (!Renderer) return;
#if WITH_EDITOR
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
		UTextureRenderTarget2D* Target = nullptr;
		for (int32 Pass = 0; Pass < 5; ++Pass)
		{
			++GFrameCounter; Surface->SlatePrepass(1.f);
			Target = Renderer->DrawWidget(Surface, View); FlushRenderingCommands();
		}
		const FSlateRect LP = Bounds(Left), RP = Bounds(Right);
		TestTrue(TEXT("Equal half-screen body allocation"), FMath::IsNearlyEqual(LP.Right - LP.Left, RP.Right - RP.Left, .1f));
		TestTrue(TEXT("Bodies stay in their own screen halves"), LP.Right < View.X * .5f && RP.Left > View.X * .5f);
		const FSlateRect LPhoto = Bounds(Left->GetWidgetFromName(TEXT("WornClothingPicture")));
		const FSlateRect RPhoto = Bounds(Right->GetWidgetFromName(TEXT("WornClothingPicture")));
		if (Rack->HasClothing())
		{
			const FSlateRect Grid = Bounds(Left->GetWidgetFromName(TEXT("OuterPocketFrame")));
			TestTrue(TEXT("Rack grid fits left half and stays to the left of its picture"), Grid.Left >= LP.Left - .1f && Grid.Right < LPhoto.Left && Grid.Top >= LP.Top - .1f && Grid.Bottom <= LP.Bottom + .1f);
		}
		for (const TCHAR* FrameName : { TEXT("InnerPocketFrame"), TEXT("OuterPocketFrame") })
		{
			UWidget* Frame = Right->GetWidgetFromName(FrameName);
			if (Frame->GetVisibility() == ESlateVisibility::Collapsed) continue;
			const auto Grid = Bounds(Frame);
			TestTrue(TEXT("Worn grid fits right half and stays to the right of its picture"), Grid.Left > RPhoto.Right && Grid.Right <= RP.Right + .1f && Grid.Top >= RP.Top - .1f && Grid.Bottom <= RP.Bottom + .1f);
		}
		if (Rack->HasClothing() && Data->HasOuterClothing())
		{
			const auto LC = Bounds(Left->GetGeneratedSlots()[0]), RC = Bounds(Right->GetGeneratedSlots()[0]);
			TestTrue(TEXT("Both halves use the same scaled cell size"), FMath::IsNearlyEqual(LC.Right - LC.Left, RC.Right - RC.Left, .2f));
		}
		for (UWidget* Coat : { static_cast<UWidget*>(RackSlot), static_cast<UWidget*>(PlayerSlot) })
			TestTrue(TEXT("Coat slots retain their fixed upper position"), FMath::IsNearlyEqual(Bounds(Coat).Top, View.Y * .13f, .2f));
		TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
		TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(int32(View.X), int32(View.Y), Pixels, PNG);
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("ClothingPreview"); IFileManager::Get().MakeDirectory(*Dir, true);
		TestTrue(TEXT("Swap preview saved"), FFileHelper::SaveArrayToFile(PNG, *(Dir / (FString(Name) + TEXT(".png")))));
	};
	Capture(TEXT("SwapRackOccupied"));
	TestTrue(TEXT("Drag rack coat to player"), UI->HandleSlotDrop(RackSlot, PlayerSlot));
	TestFalse(TEXT("Rack is empty after equipping"), Rack->HasClothing());
	TestEqual(TEXT("Player displays inner plus 24 coat cells"), Right->GetGeneratedSlots().Num(), 26);
	TestEqual(TEXT("Rack clears its cells"), Left->GetGeneratedSlots().Num(), 0);
	TestEqual(TEXT("Coat pocket item travels to player"), Data->GetItemCount(Item), 2);
	TestTrue(TEXT("Empty rack hides its coat picture"), LeftImage->GetVisibility() == ESlateVisibility::Hidden);
	Capture(TEXT("SwapRackEmpty"));
	TestTrue(TEXT("Drag player coat back to rack"), UI->HandleSlotDrop(PlayerSlot, RackSlot));
	FTAClothingInstance Other; UTAInventoryComponent::BuildClothingInstance(B, Other);
	Other.Pockets[0].Slots[0].ItemDef = Item; Other.Pockets[0].Slots[0].Count = 1;
	TestTrue(TEXT("Wear second coat"), Data->EquipOuterInstance(Other));
	TestTrue(TEXT("Move a rack item into the player's inner pocket through the UI"), UI->HandleSlotDrop(Cast<UTAInventorySlotWidget>(Left->GetGeneratedSlots()[0]), Cast<UTAInventorySlotWidget>(Right->GetGeneratedSlots()[1])));
	TestTrue(TEXT("Swap two occupied coats through their fixed slots"), UI->HandleSlotDrop(RackSlot, PlayerSlot));
	TestTrue(TEXT("Rack holds coat B"), Rack->GetStoredClothing().Definition == B);
	TestTrue(TEXT("Player wears coat A"), Data->GetOuterClothing().Definition == A);
	TestEqual(TEXT("Inner items survive coat exchange"), Data->GetItemCount(Item), 2);
	TestTrue(TEXT("Other coat keeps its pocket item on the rack"), Rack->GetStoredClothing().Pockets[0].Slots[0].ItemDef == Item);
	Capture(TEXT("SwapBothCoats"));
	Capture(TEXT("SwapBothCoats720"), FVector2D(1280, 720));
	if (Renderer) BeginCleanup(Renderer);
	UI->Close(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}
#endif
