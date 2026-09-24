#include "TALocalizationDocument.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString QuoteCell(FString Value, TCHAR Delimiter)
	{
		if (Value.Contains(FString::Chr(Delimiter)) || Value.Contains(TEXT("\"")) || Value.Contains(TEXT("\n")) || Value.Contains(TEXT("\r")))
		{
			Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
			return TEXT("\"") + Value + TEXT("\"");
		}
		return Value;
	}

	bool ParseTable(const FString& Source, TCHAR Delimiter, TArray<TArray<FString>>& OutRows, FString& OutError)
	{
		TArray<FString> Row;
		FString Cell;
		bool bQuoted = false;
		bool bClosedQuote = false;
		for (int32 Index = 0; Index < Source.Len(); ++Index)
		{
			const TCHAR Char = Source[Index];
			if (Index == 0 && Char == 0xfeff) { continue; }
			if (bQuoted)
			{
				if (Char == TEXT('"'))
				{
					if (Index + 1 < Source.Len() && Source[Index + 1] == TEXT('"')) { Cell += Char; ++Index; }
					else { bQuoted = false; bClosedQuote = true; }
				}
				else { Cell += Char; }
				continue;
			}
			if (Char == Delimiter || Char == TEXT('\n') || Char == TEXT('\r'))
			{
				Row.Add(Cell); Cell.Reset(); bClosedQuote = false;
				if (Char != Delimiter)
				{
					if (Char == TEXT('\r') && Index + 1 < Source.Len() && Source[Index + 1] == TEXT('\n')) { ++Index; }
					if (Row.Num() != 1 || !Row[0].IsEmpty()) { OutRows.Add(MoveTemp(Row)); }
					Row.Reset();
				}
			}
			else if (Char == TEXT('"') && Cell.IsEmpty() && !bClosedQuote) { bQuoted = true; }
			else if (bClosedQuote || Char == TEXT('"')) { OutError = TEXT("表格的引号格式不正确。"); return false; }
			else { Cell += Char; }
		}
		if (bQuoted) { OutError = TEXT("表格中存在未闭合的引号。"); return false; }
		if (!Row.IsEmpty() || !Cell.IsEmpty() || bClosedQuote) { Row.Add(Cell); OutRows.Add(MoveTemp(Row)); }
		return true;
	}
}

bool FTALocalizationDocument::IsValidLanguage(const FString& Code)
{
	if (Code.IsEmpty() || Code.Len() > 40 || !FChar::IsAlpha(Code[0]) || Code[0] > 127) { return false; }
	for (TCHAR Char : Code)
	{
		if (Char > 127 || (!FChar::IsAlnum(Char) && Char != TEXT('-') && Char != TEXT('_'))) { return false; }
	}
	// Windows reserved filenames cannot be used even with a .json extension.
	const FString Upper = Code.ToUpper();
	if (Upper == TEXT("CON") || Upper == TEXT("PRN") || Upper == TEXT("AUX") || Upper == TEXT("NUL")) { return false; }
	if (Upper.Len() == 4 && (Upper.StartsWith(TEXT("COM")) || Upper.StartsWith(TEXT("LPT"))) && FChar::IsDigit(Upper[3])) { return false; }
	return true;
}

bool FTALocalizationDocument::Load(const FString& InDirectory, FString& OutError)
{
	FTALocalizationTable Loaded;
	TMap<FString, FString> LoadedFiles;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(InDirectory / TEXT("*.json")), true, false);
	Files.Sort();
	// Keep the existing project languages at the left, followed by future languages.
	for (const FString& Preferred : { FString(TEXT("en.json")), FString(TEXT("zh-TW.json")), FString(TEXT("zh-CN.json")) })
	{
		if (Files.Remove(Preferred)) { Files.Insert(Preferred, 0); }
	}
	TMap<FString, int32> RowIndices;
	for (const FString& File : Files)
	{
		const FString Language = FPaths::GetBaseFilename(File);
		FString Source;
		TSharedPtr<FJsonObject> Root;
		if (!IsValidLanguage(Language) || !FFileHelper::LoadFileToString(Source, *(InDirectory / File)) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Root) || !Root.IsValid())
		{
			OutError = FString::Printf(TEXT("无法读取 %s。请修复文件后重新载入。"), *File); return false;
		}
		Loaded.Languages.Add(Language);
		LoadedFiles.Add(File, Source);
		for (const auto& Pair : Root->Values)
		{
			FString Value;
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String || !Pair.Value->TryGetString(Value))
			{
				OutError = FString::Printf(TEXT("%s 的 %s 不是文本，未载入以避免丢失数据。"), *File, *Pair.Key); return false;
			}
			int32* Existing = RowIndices.Find(Pair.Key);
			int32 RowIndex;
			if (Existing) { RowIndex = *Existing; }
			else { RowIndex = Loaded.Rows.AddDefaulted(); Loaded.Rows[RowIndex].Key = Pair.Key; RowIndices.Add(Pair.Key, RowIndex); }
			Loaded.Rows[RowIndex].Texts.Add(Language, Value);
		}
	}
	if (Loaded.Languages.IsEmpty()) { Loaded.Languages = { TEXT("zh-CN"), TEXT("zh-TW"), TEXT("en") }; }
	Loaded.Rows.Sort([](const FTALocalizationRow& A, const FTALocalizationRow& B) { return A.Key < B.Key; });
	Table = MoveTemp(Loaded); Directory = InDirectory; OriginalFiles = MoveTemp(LoadedFiles);
	UndoStack.Reset(); RedoStack.Reset(); bLoaded = true; Revision = NextRevision++; SavedRevision = Revision;
	return true;
}

bool FTALocalizationDocument::Validate(FString& OutError) const
{
	TSet<FString> Seen;
	for (const FTALocalizationRow& Row : Table.Rows)
	{
		if (Row.Key.TrimStartAndEnd().IsEmpty()) { OutError = TEXT("存在空 Key，请填写或删除对应行。"); return false; }
		if (Seen.Contains(Row.Key)) { OutError = TEXT("重复 Key：") + Row.Key; return false; }
		Seen.Add(Row.Key);
	}
	Seen.Reset();
	for (const FString& Language : Table.Languages)
	{
		if (!IsValidLanguage(Language) || Seen.Contains(Language.ToLower())) { OutError = TEXT("语言代码无效或重复：") + Language; return false; }
		Seen.Add(Language.ToLower());
	}
	return true;
}

void FTALocalizationDocument::BeginChange()
{
	UndoStack.Add({ Table, Revision });
	Revision = NextRevision++;
	if (UndoStack.Num() > 100) { UndoStack.RemoveAt(0); }
	RedoStack.Reset();
}

bool FTALocalizationDocument::Undo()
{
	if (UndoStack.IsEmpty()) { return false; }
	RedoStack.Add({ Table, Revision });
	FSnapshot Snapshot = UndoStack.Pop(); Table = MoveTemp(Snapshot.Table); Revision = Snapshot.Revision; return true;
}

bool FTALocalizationDocument::Redo()
{
	if (RedoStack.IsEmpty()) { return false; }
	UndoStack.Add({ Table, Revision });
	FSnapshot Snapshot = RedoStack.Pop(); Table = MoveTemp(Snapshot.Table); Revision = Snapshot.Revision; return true;
}

bool FTALocalizationDocument::IsDirty() const { return bLoaded && SavedRevision != Revision; }

FString FTALocalizationDocument::ExportDelimited(TCHAR Delimiter, const TSet<FGuid>* Selection) const
{
	FString Result = TEXT("Key");
	for (const FString& Language : Table.Languages) { Result += FString::Chr(Delimiter) + QuoteCell(Language, Delimiter); }
	Result += TEXT("\r\n");
	for (const FTALocalizationRow& Row : Table.Rows)
	{
		if (Selection && !Selection->Contains(Row.Id)) { continue; }
		Result += QuoteCell(Row.Key, Delimiter);
		for (const FString& Language : Table.Languages) { Result += FString::Chr(Delimiter) + QuoteCell(Row.Texts.FindRef(Language), Delimiter); }
		Result += TEXT("\r\n");
	}
	return Result;
}

bool FTALocalizationDocument::ImportDelimited(const FString& Source, TCHAR Delimiter, FString& OutError)
{
	TArray<TArray<FString>> Records;
	if (!ParseTable(Source, Delimiter, Records, OutError)) { return false; }
	if (Records.IsEmpty() || Records[0].Num() < 2 || Records[0][0] != TEXT("Key"))
	{
		OutError = TEXT("第一行必须为 Key 和语言代码，例如 Key,zh-CN,en。"); return false;
	}
	TSet<FString> SeenLanguages;
	for (int32 Column = 1; Column < Records[0].Num(); ++Column)
	{
		const FString& Language = Records[0][Column];
		if (!IsValidLanguage(Language) || SeenLanguages.Contains(Language.ToLower())) { OutError = TEXT("无效或重复的语言列：") + Language; return false; }
		for (const FString& Existing : Table.Languages)
		{
			if (Existing.Equals(Language, ESearchCase::IgnoreCase) && Existing != Language) { OutError = TEXT("语言代码大小写与已有列不同：") + Language; return false; }
		}
		SeenLanguages.Add(Language.ToLower());
	}
	TSet<FString> SeenKeys;
	for (int32 Index = 1; Index < Records.Num(); ++Index)
	{
		if (Records[Index].Num() != Records[0].Num()) { OutError = FString::Printf(TEXT("第 %d 行列数与表头不一致。"), Index + 1); return false; }
		const FString& Key = Records[Index][0];
		if (Key.TrimStartAndEnd().IsEmpty() || SeenKeys.Contains(Key)) { OutError = TEXT("导入表格中存在空 Key 或重复 Key：") + Key; return false; }
		SeenKeys.Add(Key);
	}
	if (!Validate(OutError)) { return false; }
	BeginChange();
	for (int32 Column = 1; Column < Records[0].Num(); ++Column) { Table.Languages.AddUnique(Records[0][Column]); }
	for (int32 Index = 1; Index < Records.Num(); ++Index)
	{
		const TArray<FString>& Record = Records[Index];
		FTALocalizationRow* Row = Table.Rows.FindByPredicate([&Record](const FTALocalizationRow& Item) { return Item.Key == Record[0]; });
		if (!Row) { Row = &Table.Rows.AddDefaulted_GetRef(); Row->Key = Record[0]; }
		for (int32 Column = 1; Column < Record.Num(); ++Column) { Row->Texts.Add(Records[0][Column], Record[Column]); }
	}
	return true;
}

bool FTALocalizationDocument::Save(FString& OutError)
{
	if (!bLoaded || !Validate(OutError)) { return false; }
	IFileManager& Files = IFileManager::Get();
	TArray<FString> DiskFiles;
	Files.FindFiles(DiskFiles, *(Directory / TEXT("*.json")), true, false);
	if (DiskFiles.Num() != OriginalFiles.Num()) { OutError = TEXT("语言文件已被外部增删，请先备份当前编辑内容，再重新载入。"); return false; }
	for (const auto& Pair : OriginalFiles)
	{
		if (Files.IsReadOnly(*(Directory / Pair.Key))) { OutError = TEXT("文件为只读，请先签出或解除只读：") + Pair.Key; return false; }
		FString Current;
		if (!FFileHelper::LoadFileToString(Current, *(Directory / Pair.Key)) || Current != Pair.Value)
		{
			OutError = TEXT("文件已被外部修改，请先导出当前编辑内容，再重新载入：") + Pair.Key; return false;
		}
	}
	if (!Files.MakeDirectory(*Directory, true)) { OutError = TEXT("无法创建本地化目录。"); return false; }
	TMap<FString, FString> Pending;
	TArray<FTALocalizationRow> Sorted = Table.Rows;
	Sorted.Sort([](const FTALocalizationRow& A, const FTALocalizationRow& B) { return A.Key < B.Key; });
	for (const FString& Language : Table.Languages)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		Writer->WriteObjectStart();
		for (const FTALocalizationRow& Row : Sorted)
		{
			// Retain every Key even if no language has a translation yet.
			Writer->WriteValue(Row.Key, Row.Texts.FindRef(Language));
		}
		Writer->WriteObjectEnd(); Writer->Close();
		Pending.Add(Language + TEXT(".json"), Json);
	}
	// Stage all files first. Keep a recovery copy of the original document before replacing files.
	const FString Backup = FPaths::ProjectSavedDir() / TEXT("LocalizationBackups") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
	if (!Files.MakeDirectory(*Backup, true)) { OutError = TEXT("无法创建本地化备份目录。"); return false; }
	for (const auto& Pair : OriginalFiles)
	{
		if (!FFileHelper::SaveStringToFile(Pair.Value, *(Backup / Pair.Key), FFileHelper::EEncodingOptions::ForceUTF8))
		{
			OutError = TEXT("无法写入本地化备份，未保存。"); return false;
		}
	}
	const FString StageSuffix = TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
	TArray<FString> Staged;
	for (const auto& Pair : Pending)
	{
		const FString Temp = Directory / (Pair.Key + StageSuffix);
		Staged.Add(Temp);
		if (!FFileHelper::SaveStringToFile(Pair.Value, *Temp, FFileHelper::EEncodingOptions::ForceUTF8))
		{
			for (const FString& Path : Staged) { Files.Delete(*Path); }
			OutError = TEXT("无法写入临时文件：") + Temp; return false;
		}
	}
	TArray<FString> Replaced;
	for (const auto& Pair : Pending)
	{
		if (!Files.Move(*(Directory / Pair.Key), *(Directory / (Pair.Key + StageSuffix)), true, false))
		{
			bool bRestored = true;
			// A failed move may already have removed the destination; restore it as well.
			Replaced.Add(Pair.Key);
			for (const FString& Name : Replaced)
			{
				if (OriginalFiles.Contains(Name)) { bRestored &= Files.Copy(*(Directory / Name), *(Backup / Name), true, false) == COPY_OK; }
				else { bRestored &= Files.Delete(*(Directory / Name)); }
			}
			for (const FString& Path : Staged) { Files.Delete(*Path); }
			OutError = TEXT("保存失败：") + Pair.Key + (bRestored ? TEXT("。已恢复原文件。备份：") : TEXT("。部分文件恢复失败，请从备份恢复：")) + Backup;
			return false;
		}
		Replaced.Add(Pair.Key);
	}
	OriginalFiles = MoveTemp(Pending); SavedRevision = Revision;
	return true;
}
