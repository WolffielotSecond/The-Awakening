#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "TAGameViewportClient.generated.h"

/** Game-surface policy: receivers own navigation; Slate must not move focus automatically. */
UCLASS()
class THE_AWAKENING_API UTAGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
public:
	virtual bool HandleNavigation(const uint32 UserIndex, TSharedPtr<SWidget> Destination) override;
};
