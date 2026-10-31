#pragma once

#include "CoreMinimal.h"

/** Coordinates local to the right-hand pocket column; shapes are (columns, rows). */
namespace TAWornClothingLayout
{
struct FGroup
{
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	TArray<FVector2D> Pockets;
};

struct FLayout
{
	float Cell = 0.f;
	float Padding = 0.f;
	FGroup Inner;
	FGroup Outer;
};

inline FLayout Calculate(FVector2D Area, const TArray<FIntPoint>& Inner, const TArray<FIntPoint>& Outer)
{
	FLayout Result;
	auto Measure = [](const TArray<FIntPoint>& Shapes)
	{
		FIntPoint Size(0, 0);
		for (FIntPoint Shape : Shapes) { Size.X = FMath::Max(Size.X, Shape.X); Size.Y += Shape.Y; }
		return Size;
	};
	const FIntPoint InnerSize = Measure(Inner), OuterSize = Measure(Outer);
	// Reserve the maximum authored coat (4 columns, 6 rows), plus frame/gap space.
	// Also fit legacy capacities without changing the stored inventory.
	const float WidthUnits = FMath::Max(4, FMath::Max(InnerSize.X, OuterSize.X)) + .3f;
	const float InnerUnits = InnerSize.Y + FMath::Max(0, Inner.Num() - 1) * .22f + .3f;
	const float OuterUnits = OuterSize.Y + FMath::Max(0, Outer.Num() - 1) * .22f + .3f;
	const float HeightUnits = FMath::Max(InnerUnits + 6.f + .44f + .3f + .5f,
		InnerUnits + OuterUnits + .5f);
	Result.Cell = FMath::Max(1.f, FMath::Min(Area.X / WidthUnits, Area.Y / HeightUnits));
	Result.Padding = Result.Cell * .15f;
	auto Arrange = [&](const TArray<FIntPoint>& Shapes, FGroup& Group, FIntPoint Size)
	{
		if (Shapes.IsEmpty()) return;
		Group.Size = FVector2D(Size.X * Result.Cell + 2 * Result.Padding,
			Size.Y * Result.Cell + FMath::Max(0, Shapes.Num() - 1) * Result.Cell * .22f + 2 * Result.Padding);
		float Y = Result.Padding;
		for (FIntPoint Shape : Shapes)
		{
			Group.Pockets.Add(FVector2D(Result.Padding, Y));
			Y += Shape.Y * Result.Cell + Result.Cell * .22f;
		}
	};
	Arrange(Inner, Result.Inner, InnerSize);
	Arrange(Outer, Result.Outer, OuterSize);
	const float Gap = Outer.IsEmpty() ? 0.f : Result.Cell * .5f;
	const float TotalHeight = Result.Inner.Size.Y + Result.Outer.Size.Y + Gap;
	Result.Inner.Position = FVector2D(0.f, FMath::Max(0.f, (Area.Y - TotalHeight) * .5f));
	Result.Outer.Position = Result.Inner.Position + FVector2D(0.f, Result.Inner.Size.Y + Gap);
	return Result;
}
}
