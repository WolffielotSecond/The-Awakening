#pragma once

#include "CoreMinimal.h"

struct FTALocalizationRow
{
	FGuid Id = FGuid::NewGuid();
	FString Key;
	TMap<FString, FString> Texts;
};

struct FTALocalizationTable
{
	TArray<FString> Languages;
	TArray<FTALocalizationRow> Rows;
};

/** Editor-only document; disk writes happen only on explicit Save. */
class FTALocalizationDocument
{
public:
	FTALocalizationTable Table;
	bool Load(const FString& InDirectory, FString& OutError);
	bool Save(FString& OutError);
	bool Validate(FString& OutError) const;
	bool ImportDelimited(const FString& Source, TCHAR Delimiter, FString& OutError);
	FString ExportDelimited(TCHAR Delimiter, const TSet<FGuid>* Selection = nullptr) const;
	void BeginChange();
	bool Undo();
	bool Redo();
	bool IsDirty() const;
	bool CanUndo() const { return !UndoStack.IsEmpty(); }
	bool CanRedo() const { return !RedoStack.IsEmpty(); }
	bool IsLoaded() const { return bLoaded; }
	static bool IsValidLanguage(const FString& Code);

private:
	FString Directory;
	uint64 Revision = 0;
	uint64 SavedRevision = 0;
	uint64 NextRevision = 1;
	TMap<FString, FString> OriginalFiles;
	struct FSnapshot
	{
		FTALocalizationTable Table;
		uint64 Revision;
	};
	TArray<FSnapshot> UndoStack;
	TArray<FSnapshot> RedoStack;
	bool bLoaded = false;
};
