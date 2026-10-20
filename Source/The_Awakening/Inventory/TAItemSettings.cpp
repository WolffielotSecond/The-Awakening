#include "Inventory/TAItemSettings.h"
#include "Components/MeshComponent.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

UTAItemSettings::UTAItemSettings()
{
	// 默认值写在这里（资源名含中文，避免依赖 ini 的编码）；项目设置里改过的值会覆盖它们
	DefaultItemIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/DeubgItem/棍母.棍母")));
	MissingMaterialFallback = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/DeubgItem/棍母_Mat.棍母_Mat")));
}

UTexture2D* UTAItemSettings::ResolveIcon(UTexture2D* Icon)
{
	return Icon ? Icon : GetDefault<UTAItemSettings>()->DefaultItemIcon.LoadSynchronous();
}

void UTAItemSettings::ApplyMissingMaterialFallback(UMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	UMaterialInterface* Fallback = GetDefault<UTAItemSettings>()->MissingMaterialFallback.LoadSynchronous();
	if (!Fallback)
	{
		return;
	}
	const UMaterialInterface* EngineDefault = UMaterial::GetDefaultMaterial(MD_Surface);
	for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
	{
		const UMaterialInterface* Current = Mesh->GetMaterial(Index);
		if (!Current || Current == EngineDefault)
		{
			Mesh->SetMaterial(Index, Fallback);
		}
	}
}
