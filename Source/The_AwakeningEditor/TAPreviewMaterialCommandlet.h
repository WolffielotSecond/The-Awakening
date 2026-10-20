#pragma once
#include "Commandlets/Commandlet.h"
#include "TAPreviewMaterialCommandlet.generated.h"

/**
 * 生成 3D 预览台的 UI 显示材质 /Game/UI/Preview/M_UI_PreviewStage（已存在时不覆盖）。
 * 预览台以 SceneColorHDR 捕获：RGB 为预乘颜色，A 为反向不透明度；材质把它还原成带透明背景的 UI 图像。
 * 用法：UnrealEditor-Cmd The_Awakening.uproject -run=TAPreviewMaterial
 */
UCLASS()
class UTAPreviewMaterialCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UTAPreviewMaterialCommandlet();
	virtual int32 Main(const FString& Params) override;
};
