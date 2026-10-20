#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Document/TACharacterActionComponent.h"
#include "TACharacterActionTestListener.generated.h"

/** 仅供自动化测试：模拟动画蓝图认领动作，并记录动作组件的事件。 */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class THE_AWAKENING_API UTACharacterActionTestListener : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UTACharacterActionComponent> Action;
	bool bAcceptRequests = false;

	TArray<FGameplayTag> Requested;
	TArray<FName> Keyframes;
	TArray<bool> Finished;

	void Bind(UTACharacterActionComponent* InAction)
	{
		Action = InAction;
		InAction->OnActionRequested.AddDynamic(this, &UTACharacterActionTestListener::HandleRequested);
		InAction->OnActionKeyframe.AddDynamic(this, &UTACharacterActionTestListener::HandleKeyframe);
		InAction->OnActionFinished.AddDynamic(this, &UTACharacterActionTestListener::HandleFinished);
	}

	UFUNCTION()
	void HandleRequested(FGameplayTag ActionTag)
	{
		Requested.Add(ActionTag);
		if (bAcceptRequests && Action.IsValid())
		{
			Action->AcceptAction(ActionTag);
		}
	}

	UFUNCTION()
	void HandleKeyframe(FGameplayTag ActionTag, FName KeyName) { Keyframes.Add(KeyName); }

	UFUNCTION()
	void HandleFinished(FGameplayTag ActionTag, bool bInterrupted) { Finished.Add(bInterrupted); }
};
