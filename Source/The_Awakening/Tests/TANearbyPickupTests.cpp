#include "Inventory/TANearbyPickupComponent.h"
#include "Inventory/TAWorldItem.h"
#include "Inventory/TAItemDefinition.h"
#include "Inventory/TAInventoryComponent.h"
#include "Inventory/TAClothingDefinition.h"
#include "Core/TALocalizeSubsystem.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTANearbyPickupTest, "TheAwakening.Pickup.DiscoveryAndStableSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTANearbyPickupTest::RunTest(const FString&)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Owner = World->SpawnActor<AActor>();
    auto* Pickup = NewObject<UTANearbyPickupComponent>(Owner);
    Owner->AddInstanceComponent(Pickup);
    Pickup->RegisterComponent();
    auto* Definition = NewObject<UTAItemDefinition>();
    auto Spawn = [&](float Distance)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Item = World->SpawnActor<ATAWorldItem>(FVector(Distance, 0, 0), FRotator::ZeroRotator, Params);
        Item->SetupItem(Definition, 1);
        Item->RegisterAllComponents();
        return Item;
    };
    auto* A = Spawn(40);
    auto* B = Spawn(100);
    auto* C = Spawn(160);
    auto* Outside = Spawn(230); // Collision overlaps radius, but actor origin lies outside.
    auto* ExtraSphere = NewObject<USphereComponent>(B);
    B->AddInstanceComponent(ExtraSphere);
    ExtraSphere->SetupAttachment(B->GetRootComponent());
    ExtraSphere->InitSphereRadius(20);
    ExtraSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    ExtraSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    ExtraSphere->RegisterComponent();
    Pickup->RefreshNearbyItems();
    TestTrue(TEXT("Item query collision has a physics body"), CastChecked<USphereComponent>(A->GetRootComponent())->IsPhysicsStateCreated());
    if (!TestEqual(TEXT("Each world actor appears only once and outside centers are excluded"), Pickup->GetNearbyItems().Num(), 3))
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        return false;
    }
    TestTrue(TEXT("Initial selection is nearest"), Pickup->GetSelectedItem() == A);
    Pickup->SelectedItem = B;
    C->SetActorLocation(FVector(10, 0, 0));
    Pickup->RefreshNearbyItems();
    TestTrue(TEXT("Moving does not reorder existing rows"), Pickup->GetNearbyItems() == TArray<ATAWorldItem*>({A, B, C}));
    auto* NewItem = Spawn(5);
    Pickup->RefreshNearbyItems();
    TestTrue(TEXT("New nearest item appends without stealing selection"), Pickup->GetSelectedItem() == B && Pickup->GetNearbyItems().Last() == NewItem);
    B->SetupItem(Definition, 4);
    Pickup->RefreshNearbyItems();
    TestEqual(TEXT("Quantity changes enter the presentation snapshot"), Pickup->ObservedCounts[1], 4);
    A->SetActorLocation(FVector(500, 0, 0));
    B->Destroy();
    Pickup->RefreshNearbyItems();
    TestTrue(TEXT("Removal of selected and earlier row selects next surviving row"), Pickup->GetSelectedItem() == C);
    const auto Selection = Pickup->GetSelectedItem();
    Pickup->SelectNext();
    TestTrue(TEXT("No gameplay input ownership denies selection"), Pickup->GetSelectedItem() == Selection);
    TestTrue(TEXT("Denied pickup consumes list press without changing quantity"), Pickup->TryPickupSelected());
    TestEqual(TEXT("Denied pickup preserves world count"), C->GetCount(), 1);
    C->Destroy(); NewItem->Destroy(); Outside->Destroy();
    Pickup->RefreshNearbyItems();
    TestTrue(TEXT("Empty list clears selection"), Pickup->GetNearbyItems().IsEmpty() && !Pickup->GetSelectedItem());
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPickupInventoryTest, "TheAwakening.Pickup.InventoryTransaction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPickupInventoryTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Owner = World->SpawnActor<AActor>();
    auto* Inventory = NewObject<UTAInventoryComponent>(Owner);
    Owner->AddInstanceComponent(Inventory); Inventory->RegisterComponent();
    Owner->AddOwnedComponent(Inventory);
    auto* Clothing = NewObject<UTAClothingDefinition>();
    FTAPocketDef Pocket; Pocket.SlotCount = 1;
    Clothing->Pockets.Add(Pocket);
    Inventory->DefaultInnerClothing = Clothing;
    Inventory->BeginPlay();
    TestEqual(TEXT("Test backpack has one usable slot"), Inventory->GetMaxSlots(), 1);
    TestTrue(TEXT("World pickup can find the inventory component"), Owner->FindComponentByClass<UTAInventoryComponent>() == Inventory);
    auto* Def = NewObject<UTAItemDefinition>();
    Def->bCanStack = true; Def->MaxStack = 3;
    auto* Item = World->SpawnActor<ATAWorldItem>();
    Item->SetupItem(Def, 5);
    ITAInteractable::Execute_OnInteract(Item, Owner);
    TestEqual(TEXT("Partial pickup adds only available capacity"), Inventory->GetItemCount(Def), 3);
    TestEqual(TEXT("Partial pickup keeps remaining world quantity"), Item->GetCount(), 2);
    TestFalse(TEXT("Partial pickup does not destroy item"), Item->IsActorBeingDestroyed());
    ITAInteractable::Execute_OnInteract(Item, Owner);
    TestEqual(TEXT("Full backpack leaves world item unchanged"), Item->GetCount(), 2);
    Inventory->RemoveFromSlot(0, 3);
    ITAInteractable::Execute_OnInteract(Item, Owner);
    TestEqual(TEXT("Final pickup deposits remainder"), Inventory->GetItemCount(Def), 2);
    TestTrue(TEXT("World item destroyed only after full acceptance"), Item->IsActorBeingDestroyed());
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTAPickupResourcesTest, "TheAwakening.Pickup.InputAndLocalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTAPickupResourcesTest::RunTest(const FString&)
{
    auto* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Pickup.IMC_Pickup"));
    if (!TestNotNull(TEXT("Editable pickup IMC exists"), Context)) return false;
    TestEqual(TEXT("Two device mappings per selection action"), Context->GetMappings().Num(), 4);
    for (const auto& Mapping : Context->GetMappings())
    {
        TestNotNull(TEXT("Mapping owns a real action"), Mapping.Action.Get());
        TestTrue(TEXT("Mapping key is valid"), Mapping.Key.IsValid());
    }
    auto* Loc = NewObject<UTALocalizeSubsystem>(NewObject<UGameInstance>());
    for (const FString Language : {TEXT("en"), TEXT("zh-CN"), TEXT("zh-TW")})
    {
        TestTrue(TEXT("Language JSON loads"), Loc->SetLanguage(Language));
        for (const FString Id : {TEXT("Pickup_Previous"), TEXT("Pickup_Next"), TEXT("Pickup_Failed"), TEXT("Settings.Binding.IA_PickupPrevious"), TEXT("Settings.Binding.IA_PickupNext")})
            TestTrue(TEXT("Pickup text resolves in every supported language"), Loc->GetText(Id).ToString() != Id);
    }
    return true;
}
#endif
