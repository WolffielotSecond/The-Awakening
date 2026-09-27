#include "Puzzle/TAPathPuzzleTypes.h"

bool FTAPuzzleEffect::IsPenalty() const
{
	return Type == ETAPuzzleEffectType::EnergyUsed ? Amount > 0 : Amount < 0;
}

FText FTAPuzzleEffect::GetLabel() const
{
	if (!Label.IsEmpty()) return Label;
	const TCHAR* Name = TEXT("");
	switch (Type)
	{
	case ETAPuzzleEffectType::EnergyLimit: Name = TEXT("Energy limit"); break;
	case ETAPuzzleEffectType::UnitLimit: Name = TEXT("Unit limit"); break;
	case ETAPuzzleEffectType::EnergyUsed: Name = TEXT("Energy used"); break;
	case ETAPuzzleEffectType::Money: Name = TEXT("Money"); break;
	case ETAPuzzleEffectType::MaxHealth: Name = TEXT("Max HP"); break;
	case ETAPuzzleEffectType::Health: Name = TEXT("HP"); break;
	case ETAPuzzleEffectType::Item: Name = TEXT("Item"); break;
	case ETAPuzzleEffectType::GameplayEffect: return FText::FromString(TEXT("Buff"));
	default: return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("%s %+d"), Name, Amount));
}
