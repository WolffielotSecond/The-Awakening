#include "Inventory/TAClothingDefinition.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"
#endif

void UTAClothingDefinition::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_Standalone) && !HasAnyFlags(RF_NeedLoad | RF_ClassDefaultObject) && Pockets.IsEmpty())
	{
		FTAPocketDef& Pocket = Pockets.AddDefaulted_GetRef();
		Pocket.Columns = 1;
		Pocket.Rows = 1;
	}
}

void UTAClothingDefinition::PostLoad()
{
	Super::PostLoad();
	// Existing inventories/save games depend on the original pocket capacities.
	// Only authoring edits normalize shapes; loading never truncates old pockets.
	PocketCount = FMath::Clamp(Pockets.Num(), 1, 3);
}

#if WITH_EDITOR
void UTAClothingDefinition::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	PocketCount = FMath::Clamp(PocketCount, 1, 3);
	Pockets.SetNum(PocketCount);
	TSet<FName> UsedIds;
	for (int32 Index = 0; Index < Pockets.Num(); ++Index)
	{
		FTAPocketDef& Pocket = Pockets[Index];
		Pocket.Columns = FMath::Clamp(Pocket.Columns, 1, 4);
		Pocket.Rows = FMath::Clamp(Pocket.Rows, 1, 2);
		if (Pocket.PocketId.IsNone() || UsedIds.Contains(Pocket.PocketId))
		{
			int32 Suffix = Index + 1;
			do { Pocket.PocketId = FName(*FString::Printf(TEXT("Pocket%d"), Suffix++)); }
			while (UsedIds.Contains(Pocket.PocketId));
		}
		UsedIds.Add(Pocket.PocketId);
	}
	Super::PostEditChangeProperty(Event);
}

EDataValidationResult UTAClothingDefinition::IsDataValid(FDataValidationContext& Context) const
{
	bool bValid = Super::IsDataValid(Context) != EDataValidationResult::Invalid;
	if (Pockets.Num() < 1 || Pockets.Num() > 3 || PocketCount != Pockets.Num())
	{
		Context.AddError(FText::FromString(TEXT("Clothing requires 1–3 pockets matching PocketCount.")));
		bValid = false;
	}
	TSet<FName> Ids;
	for (const FTAPocketDef& Pocket : Pockets)
	{
		if (Pocket.Rows < 1 || Pocket.Rows > 2 || Pocket.Columns < 1 || Pocket.Columns > 4 || Pocket.PocketId.IsNone() || Ids.Contains(Pocket.PocketId))
		{
			Context.AddError(FText::FromString(TEXT("Pockets require unique IDs, 1–2 rows and 1–4 columns.")));
			bValid = false;
		}
		Ids.Add(Pocket.PocketId);
	}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
