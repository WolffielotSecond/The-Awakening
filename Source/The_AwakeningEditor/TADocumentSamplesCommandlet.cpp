#include "TADocumentSamplesCommandlet.h"
#include "Document/TADocumentDefinition.h"
#include "Inventory/TAItemDefinition.h"
#include "Engine/Texture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Inventory/TAWorldItem.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogTADocumentSamples, Log, All);

namespace
{
	template <typename T>
	T* CreateAsset(const FString& Path, TFunctionRef<void(T&)> Fill, bool& bOutOk)
	{
		const FString Name = FPackageName::GetShortName(Path);
		if (T* Existing = LoadObject<T>(nullptr, *(Path + TEXT(".") + Name)))
		{
			UE_LOG(LogTADocumentSamples, Display, TEXT("%s already exists; not overwritten."), *Path);
			return Existing;
		}
		UPackage* Package = CreatePackage(*Path);
		T* Asset = NewObject<T>(Package, *Name, RF_Public | RF_Standalone);
		Fill(*Asset);
		FAssetRegistryModule::AssetCreated(Asset);
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogTADocumentSamples, Error, TEXT("Failed to save %s."), *Filename);
			bOutOk = false;
			return nullptr;
		}
		UE_LOG(LogTADocumentSamples, Display, TEXT("Created %s."), *Path);
		return Asset;
	}
}

UTADocumentSamplesCommandlet::UTADocumentSamplesCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UTADocumentSamplesCommandlet::Main(const FString& Params)
{
	bool bOk = true;
	CreateAsset<UTAItemDefinition>(TEXT("/Game/Document/DA_Item_Diary"), [](UTAItemDefinition& Item)
	{
		// 物品名称字段存放本地化 ID，与现有物品一致
		Item.DisplayName = FText::AsCultureInvariant(TEXT("Item_Diary_Name"));
		Item.Description = FText::AsCultureInvariant(TEXT("Item_Diary_Desc"));
		Item.ItemType = ETAItemType::QuestItem;
		Item.StorySlotIndex = 0;
	}, bOk);

	auto MakeDocument = [&bOk](const TCHAR* AssetName, const TCHAR* Id)
	{
		CreateAsset<UTADocumentDefinition>(FString(TEXT("/Game/Document/")) + AssetName, [Id](UTADocumentDefinition& Doc)
		{
			const FString Base(Id);
			Doc.DocumentId = FName(Id);
			Doc.TitleTextId = Base + TEXT("_Title");
			Doc.BodyTextId = Base + TEXT("_Body");
			Doc.ScanDescriptionTextId = Base + TEXT("_Scan");
		}, bOk);
	};
	MakeDocument(TEXT("DA_Document_Sample01"), TEXT("Doc_Sample01"));
	MakeDocument(TEXT("DA_Document_Sample02"), TEXT("Doc_Sample02"));

	// DA_Document_67：模型为骨骼网格体 SK_A4_Paper_With_Fold，没有扫描介绍
	CreateAsset<UTADocumentDefinition>(TEXT("/Game/Document/DA_Document_67"), [](UTADocumentDefinition& Doc)
	{
		Doc.DocumentId = TEXT("Doc_67");
		Doc.TitleTextId = TEXT("Doc_67_Title");
		Doc.BodyTextId = TEXT("Doc_67_Body");
		Doc.Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Models/Document/SK_A4_Paper_With_Fold.SK_A4_Paper_With_Fold"));
	}, bOk);
	// 资产已存在（手工创建）时只补空着的字段：模型、DocumentId；已填的值不动
	if (UTADocumentDefinition* Doc67 = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_67.DA_Document_67")))
	{
		bool bChanged = false;
		if (!Doc67->Mesh)
		{
			Doc67->Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Models/Document/SK_A4_Paper_With_Fold.SK_A4_Paper_With_Fold"));
			bChanged = Doc67->Mesh != nullptr;
		}
		if (Doc67->DocumentId.IsNone())
		{
			Doc67->DocumentId = TEXT("Doc_67");
			bChanged = true;
		}
		if (bChanged)
		{
			FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
			const FString Filename = FPackageName::LongPackageNameToFilename(Doc67->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
			bOk &= UPackage::SavePackage(Doc67->GetOutermost(), Doc67, *Filename, Args);
		}
		UE_LOG(LogTADocumentSamples, Display, TEXT("DA_Document_67: DocumentId=%s, Title=%s, Body=%s, Mesh=%s%s"),
			*Doc67->DocumentId.ToString(), *Doc67->TitleTextId, *Doc67->BodyTextId, *GetNameSafe(Doc67->Mesh), bChanged ? TEXT(" (updated)") : TEXT(""));
	}

	// 示例插图：Sample02 正文里的 <img id="photo"/>，缺少时补上（使用工程里现有的调试贴图）
	if (UTADocumentDefinition* Doc = LoadObject<UTADocumentDefinition>(nullptr, TEXT("/Game/Document/DA_Document_Sample02.DA_Document_Sample02")))
	{
		if (!Doc->InlineImages.Contains(TEXT("photo")))
		{
			UTexture2D* Photo = LoadObject<UTexture2D>(nullptr, TEXT("/Game/DeubgItem/5751a89d24597722aa0e8db0-512.5751a89d24597722aa0e8db0-512"));
			if (Photo)
			{
				Doc->InlineImages.Add(TEXT("photo"), Photo);
				FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
				const FString Filename = FPackageName::LongPackageNameToFilename(Doc->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
				bOk &= UPackage::SavePackage(Doc->GetOutermost(), Doc, *Filename, Args);
				UE_LOG(LogTADocumentSamples, Display, TEXT("Added sample image 'photo' to DA_Document_Sample02."));
			}
			else
			{
				UE_LOG(LogTADocumentSamples, Warning, TEXT("Sample image texture not found; DA_Document_Sample02 left without 'photo'."));
			}
		}
	}
	// 日记本做成地上可拾取的物品：基础世界物品，物品设为 DA_Item_Diary（没有模型时显示占位方块）
	const TCHAR* DiaryPickupPath = TEXT("/Game/Document/BP_WorldItem_Diary");
	UTAItemDefinition* Diary = LoadObject<UTAItemDefinition>(nullptr, TEXT("/Game/Document/DA_Item_Diary.DA_Item_Diary"));
	if (!LoadObject<UBlueprint>(nullptr, TEXT("/Game/Document/BP_WorldItem_Diary.BP_WorldItem_Diary")) && Diary)
	{
		UPackage* Package = CreatePackage(DiaryPickupPath);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ATAWorldItem::StaticClass(), Package, TEXT("BP_WorldItem_Diary"),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (Blueprint && Blueprint->GeneratedClass)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			Blueprint->GeneratedClass->GetDefaultObject<ATAWorldItem>()->SetupItem(Diary, 1);
			FAssetRegistryModule::AssetCreated(Blueprint);
			FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
			const FString Filename = FPackageName::LongPackageNameToFilename(DiaryPickupPath, FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			bOk &= UPackage::SavePackage(Package, Blueprint, *Filename, Args);
			UE_LOG(LogTADocumentSamples, Display, TEXT("Created %s."), DiaryPickupPath);
		}
	}

	// 现有物品丢弃时生成对应的世界物品蓝图（只补空着的）
	auto LinkWorldItem = [&bOk](const TCHAR* ItemPath, const TCHAR* ClassPath)
	{
		UTAItemDefinition* Item = LoadObject<UTAItemDefinition>(nullptr, ItemPath);
		UClass* WorldClass = LoadClass<ATAWorldItem>(nullptr, ClassPath);
		if (!Item || !WorldClass || !Item->WorldItemClass.IsNull())
		{
			return;
		}
		Item->WorldItemClass = WorldClass;
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString Filename = FPackageName::LongPackageNameToFilename(Item->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		bOk &= UPackage::SavePackage(Item->GetOutermost(), Item, *Filename, Args);
		UE_LOG(LogTADocumentSamples, Display, TEXT("%s now drops as %s."), *Item->GetName(), *WorldClass->GetName());
	};
	LinkWorldItem(TEXT("/Game/DeubgItem/DA_Item_Herb.DA_Item_Herb"), TEXT("/Game/DeubgItem/BP_WorldItem_Herb.BP_WorldItem_Herb_C"));
	LinkWorldItem(TEXT("/Game/DeubgItem/DA_Item_Keycard.DA_Item_Keycard"), TEXT("/Game/DeubgItem/BP_WorldItem_Keycard.BP_WorldItem_Keycard_C"));
	LinkWorldItem(TEXT("/Game/DeubgItem/DA_Money.DA_Money"), TEXT("/Game/DeubgItem/BP_WorldItem_Money.BP_WorldItem_Money_C"));
	return bOk ? 0 : 1;
}
