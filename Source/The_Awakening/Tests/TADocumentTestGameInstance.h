#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "TADocumentTestGameInstance.generated.h"

/** 仅供自动化测试：让 GameInstance 子系统拿到测试世界（回滚子系统挂在世界上），并记录文本收集事件。 */
UCLASS(Transient, NotBlueprintable, HideDropdown)
class THE_AWAKENING_API UTADocumentTestGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** GetWorld 是 final，只能通过受保护的 WorldContext 指向测试世界 */
	void SetTestWorld(UWorld* World)
	{
		TestContext.SetCurrentWorld(World);
		WorldContext = &TestContext;
	}

	struct FCollectedEvent { FName DocumentId; int32 PageIndex; };
	TArray<FCollectedEvent> CollectedEvents;

	UFUNCTION()
	void HandleDocumentCollected(FName DocumentId, int32 PageIndex) { CollectedEvents.Add({DocumentId, PageIndex}); }

private:
	FWorldContext TestContext;
};
