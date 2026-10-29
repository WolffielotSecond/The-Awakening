#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TARoomCameraProfile.generated.h"

/** 一个区域的镜头参数。偏移按屏幕方向填写，左右 / 上下可以不对称。 */
USTRUCT(BlueprintType)
struct THE_AWAKENING_API FTARoomCameraSettings
{
	GENERATED_BODY()

	/** Camera Boom 长度（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "50.0", UIMin = "50.0"))
	float ArmLengthCm = 750.f;

	/** 镜头高度偏移（cm），写入 Boom 的 SocketOffset.Z。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float HeightOffsetCm = 80.f;

	/** 鼠标 / 右摇杆向屏幕左侧最多能移动多少（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookLeftCm = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookRightCm = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookUpCm = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LookDownCm = 150.f;

	/** 进入这个区域时，从旧参数过渡到本参数用的秒数。0 为立即切换。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BlendSeconds = 0.6f;
};

/** 可在多个区域间共用的镜头参数资产，例如 Bedroom / Corridor / Outdoor。 */
UCLASS(BlueprintType)
class THE_AWAKENING_API UTARoomCameraProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ShowOnlyInnerProperties))
	FTARoomCameraSettings Settings;
};
