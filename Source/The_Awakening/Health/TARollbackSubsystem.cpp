#include "Health/TARollbackSubsystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogTARollback, Log, All);

UTARollbackSubsystem* UTARollbackSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTARollbackSubsystem>() : nullptr;
}

FTARollbackScopeHandle UTARollbackSubsystem::BeginScope(UObject* Owner, FName Label, TFunction<void()> AbortAction)
{
	FTARollbackScopeHandle Handle;
	if (!IsValid(Owner))
	{
		UE_LOG(LogTARollback, Warning, TEXT("BeginScope '%s' needs a valid owner."), *Label.ToString());
		return Handle;
	}
	FScope& Scope = Scopes.AddDefaulted_GetRef();
	Scope.Id = ++NextScopeId;
	Scope.Owner = Owner;
	Scope.Label = Label;
	Scope.AbortAction = MoveTemp(AbortAction);
	Handle.Id = Scope.Id;
	return Handle;
}

void UTARollbackSubsystem::CommitScope(FTARollbackScopeHandle& Handle)
{
	const uint64 Id = Handle.Id;
	Handle = FTARollbackScopeHandle();
	if (Id)
	{
		Scopes.RemoveAll([Id](const FScope& Scope) { return Scope.Id == Id; });
	}
}

void UTARollbackSubsystem::PruneScopes() const
{
	Scopes.RemoveAll([](const FScope& Scope) { return !Scope.Owner.IsValid(); });
}

bool UTARollbackSubsystem::IsRecording() const
{
	if (bRollingBack)
	{
		return false;
	}
	PruneScopes();
	return Scopes.Num() > 0;
}

void UTARollbackSubsystem::RecordUndo(FName Label, TFunction<void()> Undo)
{
	if (!Undo || !IsRecording())
	{
		return;
	}
	Scopes.Last().Records.Add({Label, MoveTemp(Undo)});
}

void UTARollbackSubsystem::RecordUndoByEvent(FName Label, FTARollbackUndoDynamicDelegate Undo)
{
	if (Undo.IsBound())
	{
		RecordUndo(Label, [Undo] { Undo.ExecuteIfBound(); });
	}
}

void UTARollbackSubsystem::RollbackAll()
{
	if (bRollingBack)
	{
		return;
	}
	PruneScopes();
	bRollingBack = true;
	// 从内到外：先撤销最近开始的作用域
	while (Scopes.Num() > 0)
	{
		FScope Scope = Scopes.Pop();
		UE_LOG(LogTARollback, Log, TEXT("Rolling back '%s' (%d records)."), *Scope.Label.ToString(), Scope.Records.Num());
		for (int32 Index = Scope.Records.Num() - 1; Index >= 0; --Index)
		{
			Scope.Records[Index].Undo();
		}
		if (Scope.AbortAction)
		{
			Scope.AbortAction();
		}
	}
	bRollingBack = false;
}

int32 UTARollbackSubsystem::GetScopeCount() const
{
	PruneScopes();
	return Scopes.Num();
}

int32 UTARollbackSubsystem::GetRecordCount() const
{
	PruneScopes();
	int32 Count = 0;
	for (const FScope& Scope : Scopes) Count += Scope.Records.Num();
	return Count;
}

void UTARollbackSubsystem::Deinitialize()
{
	Scopes.Reset();
	Super::Deinitialize();
}
