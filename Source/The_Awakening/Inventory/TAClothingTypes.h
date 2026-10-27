#pragma once

#include "CoreMinimal.h"
#include "TAClothingTypes.generated.h"

UENUM(BlueprintType)
enum class ETAClothingLayer : uint8
{
	Inner UMETA(DisplayName = "内衬"),
	Outer UMETA(DisplayName = "外套")
};

/** 口袋格子相对 500x500 衣服图的摆放方向（避免挡住衣服） */
UENUM(BlueprintType)
enum class ETAPocketSlotSide : uint8
{
	Left   UMETA(DisplayName = "左侧"),
	Right  UMETA(DisplayName = "右侧"),
	Top    UMETA(DisplayName = "上方"),
	Bottom UMETA(DisplayName = "下方")
};

/**
 * 口袋定义（设计数据）
 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPocketDef
{
	GENERATED_BODY()

	/** 口袋标识，同一件衣服内唯一 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pocket")
	FName PocketId = TEXT("Main");

	/**
	 * 口袋形状：Columns x Rows 个格子（例如 1x2、2x4、2x2），格子数 = Columns * Rows。
	 * 0 表示未设置（旧资产），此时回退到 SlotCount。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pocket", meta = (ClampMin = "1"))
	int32 Columns = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pocket", meta = (ClampMin = "1"))
	int32 Rows = 0;

	/** 已废弃：只在 Columns / Rows 未设置时使用，保证旧的衣服资产继续能读 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pocket", meta = (ClampMin = "1", DisplayName = "Slot Count (Deprecated)"))
	int32 SlotCount = 1;

	/**
	 * (0,0)=左上，(1,1)=右下，口袋位置
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pocket|UI",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D AnchorUV = FVector2D(0.5f, 0.5f);

	/** 格子生成在衣服图的哪一侧 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pocket|UI")
	ETAPocketSlotSide SlotSide = ETAPocketSlotSide::Right;

	/** 是否配置了 Columns x Rows 形状 */
	bool HasGridShape() const { return Columns > 0 && Rows > 0; }

	/** 实际格子数：有形状时 Columns * Rows，否则回退 SlotCount */
	int32 GetSlotCount() const
	{
		return HasGridShape() ? Columns * Rows : FMath::Max(0, SlotCount);
	}

	/**
	 * 界面网格的列数和行数。旧资产没有形状时沿用原来的摆法：
	 * Left/Right 两列（行优先），Top/Bottom 两行（列优先）。
	 */
	void GetGridSize(int32& OutColumns, int32& OutRows) const
	{
		if (HasGridShape())
		{
			OutColumns = Columns;
			OutRows = Rows;
			return;
		}
		const int32 Num = GetSlotCount();
		const int32 Short = FMath::Min(2, FMath::Max(1, Num));
		const int32 Long = FMath::DivideAndRoundUp(FMath::Max(1, Num), 2);
		const bool bSideColumns = SlotSide == ETAPocketSlotSide::Left || SlotSide == ETAPocketSlotSide::Right;
		OutColumns = bSideColumns ? Short : Long;
		OutRows = bSideColumns ? Long : Short;
	}

	/** 第 Index 个格子在网格中的行列。有形状时行优先；旧资产 Top/Bottom 沿用原来的列优先 */
	void GetCellPosition(int32 Index, int32& OutRow, int32& OutColumn) const
	{
		int32 GridColumns = 1;
		int32 GridRows = 1;
		GetGridSize(GridColumns, GridRows);
		const bool bColumnMajor = !HasGridShape() && (SlotSide == ETAPocketSlotSide::Top || SlotSide == ETAPocketSlotSide::Bottom);
		if (bColumnMajor)
		{
			OutColumn = Index / FMath::Max(1, GridRows);
			OutRow = Index % FMath::Max(1, GridRows);
		}
		else
		{
			OutRow = Index / FMath::Max(1, GridColumns);
			OutColumn = Index % FMath::Max(1, GridColumns);
		}
	}
};

/** 外套可选：穿上后改写内衬（默认）口袋在界面上的位置。只影响显示 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTAPocketDisplayOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pocket|UI",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D AnchorUV = FVector2D(0.5f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pocket|UI")
	ETAPocketSlotSide SlotSide = ETAPocketSlotSide::Right;
};