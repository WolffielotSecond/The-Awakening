#include "Document/TADocumentSubsystem.h"
#include "Health/TARollbackSubsystem.h"
#include "Document/TADocumentDefinition.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogTADocument, Log, All);

UTADocumentSubsystem* UTADocumentSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UTADocumentSubsystem>() : nullptr;
}

bool UTADocumentSubsystem::Collect(FName DocumentId)
{
	if (DocumentId.IsNone())
	{
		UE_LOG(LogTADocument, Warning, TEXT("Collect: empty DocumentId."));
		return false;
	}
	if (CollectedDocuments.Contains(DocumentId))
	{
		return false;
	}

	const bool bWasRead = ReadDocuments.Contains(DocumentId);
	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this); Rollback && Rollback->IsRecording())
	{
		Rollback->RecordUndo(TEXT("Document.Collect"), [WeakThis = TWeakObjectPtr<UTADocumentSubsystem>(this), DocumentId, bWasRead]
		{
			if (UTADocumentSubsystem* Self = WeakThis.Get())
			{
				Self->CollectedDocuments.RemoveSingle(DocumentId);
				if (!bWasRead)
				{
					Self->ReadDocuments.Remove(DocumentId);
				}
				Self->DiaryPageIndex = Self->ClampPage(Self->DiaryPageIndex);
			}
		});
	}

	const int32 PageIndex = CollectedDocuments.Add(DocumentId);
	ReadDocuments.Add(DocumentId);
	OnDocumentCollected.Broadcast(DocumentId, PageIndex);
	return true;
}

bool UTADocumentSubsystem::MarkRead(FName DocumentId)
{
	if (DocumentId.IsNone())
	{
		UE_LOG(LogTADocument, Warning, TEXT("MarkRead: empty DocumentId."));
		return false;
	}
	if (ReadDocuments.Contains(DocumentId))
	{
		return false;
	}

	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this); Rollback && Rollback->IsRecording())
	{
		Rollback->RecordUndo(TEXT("Document.MarkRead"), [WeakThis = TWeakObjectPtr<UTADocumentSubsystem>(this), DocumentId]
		{
			if (UTADocumentSubsystem* Self = WeakThis.Get())
			{
				Self->ReadDocuments.Remove(DocumentId);
			}
		});
	}

	ReadDocuments.Add(DocumentId);
	return true;
}

bool UTADocumentSubsystem::IsCollected(FName DocumentId) const
{
	return CollectedDocuments.Contains(DocumentId);
}

bool UTADocumentSubsystem::IsRead(FName DocumentId) const
{
	return ReadDocuments.Contains(DocumentId);
}

FName UTADocumentSubsystem::GetDocumentAtPage(int32 PageIndex) const
{
	return CollectedDocuments.IsValidIndex(PageIndex) ? CollectedDocuments[PageIndex] : NAME_None;
}

int32 UTADocumentSubsystem::GetPageOfDocument(FName DocumentId) const
{
	return CollectedDocuments.IndexOfByKey(DocumentId);
}

void UTADocumentSubsystem::SetDiaryPage(int32 PageIndex)
{
	const int32 NewIndex = ClampPage(PageIndex);
	if (NewIndex == DiaryPageIndex)
	{
		return;
	}

	if (UTARollbackSubsystem* Rollback = UTARollbackSubsystem::Get(this); Rollback && Rollback->IsRecording())
	{
		Rollback->RecordUndo(TEXT("Document.DiaryPage"), [WeakThis = TWeakObjectPtr<UTADocumentSubsystem>(this), OldIndex = DiaryPageIndex]
		{
			if (UTADocumentSubsystem* Self = WeakThis.Get())
			{
				Self->DiaryPageIndex = Self->ClampPage(OldIndex);
			}
		});
	}

	DiaryPageIndex = NewIndex;
}

FTADocumentSaveData UTADocumentSubsystem::GetSaveData() const
{
	FTADocumentSaveData Data;
	Data.CollectedDocuments = CollectedDocuments;
	Data.ReadDocuments = ReadDocuments.Array();
	Data.DiaryPageIndex = DiaryPageIndex;
	return Data;
}

void UTADocumentSubsystem::RestoreSaveData(const FTADocumentSaveData& SaveData)
{
	CollectedDocuments.Reset();
	ReadDocuments.Reset();
	for (const FName& Id : SaveData.CollectedDocuments)
	{
		if (!Id.IsNone())
		{
			CollectedDocuments.AddUnique(Id);
			ReadDocuments.Add(Id);
		}
	}
	for (const FName& Id : SaveData.ReadDocuments)
	{
		if (!Id.IsNone())
		{
			ReadDocuments.Add(Id);
		}
	}
	DiaryPageIndex = ClampPage(SaveData.DiaryPageIndex);
}

int32 UTADocumentSubsystem::ClampPage(int32 PageIndex) const
{
	return CollectedDocuments.IsEmpty() ? 0 : FMath::Clamp(PageIndex, 0, CollectedDocuments.Num() - 1);
}

UTADocumentDefinition* UTADocumentSubsystem::FindDefinition(FName DocumentId)
{
	if (DocumentId.IsNone())
	{
		return nullptr;
	}
	if (const TObjectPtr<UTADocumentDefinition>* Found = DefinitionCache.Find(DocumentId))
	{
		return *Found;
	}
	// 新资产或首次查询：重建一次缓存
	RebuildDefinitionCache();
	if (const TObjectPtr<UTADocumentDefinition>* Found = DefinitionCache.Find(DocumentId))
	{
		return *Found;
	}
	UE_LOG(LogTADocument, Warning, TEXT("FindDefinition: no document definition with id '%s'."), *DocumentId.ToString());
	return nullptr;
}

void UTADocumentSubsystem::RebuildDefinitionCache()
{
	DefinitionCache.Reset();
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UTADocumentDefinition::StaticClass()->GetClassPathName(), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		if (UTADocumentDefinition* Definition = Cast<UTADocumentDefinition>(Asset.GetAsset()); Definition && !Definition->DocumentId.IsNone())
		{
			if (DefinitionCache.Contains(Definition->DocumentId))
			{
				UE_LOG(LogTADocument, Warning, TEXT("Duplicate DocumentId '%s' (%s); keeping the first."), *Definition->DocumentId.ToString(), *Asset.GetObjectPathString());
				continue;
			}
			DefinitionCache.Add(Definition->DocumentId, Definition);
		}
	}
}
