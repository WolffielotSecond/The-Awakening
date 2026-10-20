#include "TAPreviewMaterialCommandlet.h"
#include "Document/TAPreviewStage.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Engine/Texture2D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogTAPreviewMaterial, Log, All);

UTAPreviewMaterialCommandlet::UTAPreviewMaterialCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UTAPreviewMaterialCommandlet::Main(const FString& Params)
{
	const FString Path = ATAPreviewStage::DisplayMaterialPackage;
	const FString Name = FPackageName::GetShortName(Path);
	if (LoadObject<UMaterial>(nullptr, *(Path + TEXT(".") + Name)))
	{
		UE_LOG(LogTAPreviewMaterial, Display, TEXT("%s already exists; not overwritten."), *Path);
		return 0;
	}

	UPackage* Package = CreatePackage(*Path);
	UMaterial* Material = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	Material->MaterialDomain = MD_UI;
	Material->BlendMode = BLEND_Translucent;

	auto* Sample = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
	Sample->ParameterName = ATAPreviewStage::DisplayTextureParameter;
	// 默认贴图只为编辑器能编译；运行时由预览台换成自己的 RenderTarget
	Sample->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	Material->GetExpressionCollection().AddExpression(Sample);

	auto* Exposure = NewObject<UMaterialExpressionScalarParameter>(Material);
	Exposure->ParameterName = ATAPreviewStage::DisplayExposureParameter;
	Exposure->DefaultValue = 1.f;
	Material->GetExpressionCollection().AddExpression(Exposure);

	// SceneColorHDR：A = 1 - 不透明度，RGB 已预乘不透明度
	auto MakeCustom = [&](ECustomMaterialOutputType Output, const TCHAR* Code)
	{
		auto* Custom = NewObject<UMaterialExpressionCustom>(Material);
		Custom->Inputs.Reset();
		Custom->OutputType = Output;
		Custom->Code = Code;
		FCustomInput Capture; Capture.InputName = TEXT("Capture"); Capture.Input.Connect(5, Sample); // RGBA
		FCustomInput Scale; Scale.InputName = TEXT("Exposure"); Scale.Input.Connect(0, Exposure);
		Custom->Inputs.Add(Capture);
		Custom->Inputs.Add(Scale);
		Material->GetExpressionCollection().AddExpression(Custom);
		return Custom;
	};
	UMaterialExpressionCustom* Opacity = MakeCustom(CMOT_Float1, TEXT("return saturate(1 - Capture.a);"));
	UMaterialExpressionCustom* Color = MakeCustom(CMOT_Float3,
		TEXT("float Opacity = saturate(1 - Capture.a); return Capture.rgb * Exposure / max(Opacity, 0.0001);"));

	Material->GetEditorOnlyData()->EmissiveColor.Connect(0, Color);
	Material->GetEditorOnlyData()->Opacity.Connect(0, Opacity);
	Material->PostEditChange();
	FAssetRegistryModule::AssetCreated(Material);

	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
	if (!UPackage::SavePackage(Package, Material, *Filename, Args))
	{
		UE_LOG(LogTAPreviewMaterial, Error, TEXT("Failed to save %s."), *Filename);
		return 1;
	}
	UE_LOG(LogTAPreviewMaterial, Display, TEXT("Created %s."), *Path);
	return 0;
}
