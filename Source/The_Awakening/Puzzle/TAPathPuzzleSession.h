#pragma once

#include "CoreMinimal.h"
#include "Puzzle/TAPathPuzzleTypes.h"
#include "TAPathPuzzleSession.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTAOnPuzzleChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTAOnPuzzleSettled, const FTAPuzzleResult&, Result);

/** One entry into a puzzle. Re-entry creates a new session; retries never refill budgets or time. */
UCLASS(BlueprintType)
class THE_AWAKENING_API UTAPathPuzzleSession : public UObject
{
	GENERATED_BODY()
public:
	bool Initialize(const FTAPuzzleDefinition& InDefinition, const FTAPuzzleSettings& InSettings, FString& Error);

	UFUNCTION(BlueprintCallable, Category="Puzzle") bool SelectNode(int32 NodeIndex);
	UFUNCTION(BlueprintCallable, Category="Puzzle") bool Undo();
	UFUNCTION(BlueprintCallable, Category="Puzzle") bool Retry();
	UFUNCTION(BlueprintCallable, Category="Puzzle") void Abort();
	/** Called by the owning widget, not by both widget and a world timer. */
	void AdvanceTime(float DeltaSeconds);

	/** Updates total allowances, preserving already-spent uses. Also enables/disables each action. */
	UFUNCTION(BlueprintCallable, Category="Puzzle|Assistance")
	void SetUndoAllowance(bool bEnabled, int32 TotalAllowance);
	UFUNCTION(BlueprintCallable, Category="Puzzle|Assistance")
	void SetRetryAllowance(bool bEnabled, int32 TotalAllowance);
	UFUNCTION(BlueprintPure, Category="Puzzle|Assistance") int32 GetUndoRemaining() const;
	UFUNCTION(BlueprintPure, Category="Puzzle|Assistance") int32 GetRetryRemaining() const;
	UFUNCTION(BlueprintPure, Category="Puzzle|Assistance") bool CanUndo() const;
	UFUNCTION(BlueprintPure, Category="Puzzle|Assistance") bool CanRetry() const;
	UFUNCTION(BlueprintPure, Category="Puzzle") bool IsTerminal() const;

	UPROPERTY(BlueprintReadOnly, Category="Puzzle") FTAPuzzleDefinition Definition;
	UPROPERTY(BlueprintReadOnly, Category="Puzzle") FTAPuzzleSettings Settings;
	UPROPERTY(BlueprintReadOnly, Category="Puzzle") FTAPuzzleProgress Progress;
	UPROPERTY(BlueprintReadOnly, Category="Puzzle") ETAPuzzleState State = ETAPuzzleState::Ready;
	UPROPERTY(BlueprintReadOnly, Category="Puzzle") float TimeRemaining = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Puzzle") FText LastMessage;
	UPROPERTY(BlueprintReadOnly, Category="Puzzle") FTAPuzzleResult Result;
	UPROPERTY(BlueprintAssignable, Category="Puzzle") FTAOnPuzzleChanged OnChanged;
	UPROPERTY(BlueprintAssignable, Category="Puzzle") FTAOnPuzzleSettled OnSettled;

private:
	void ResetProgress();
	void Settle(ETAPuzzleFailure Failure);
	UPROPERTY() TArray<FTAPuzzleProgress> History;
	int32 UndoUsed = 0;
	int32 RetryUsed = 0;
	bool bInitialized = false;
};
