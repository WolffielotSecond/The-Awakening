// Source/The_AwakeningEditor/The_AwakeningEditor.h
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IAssetTypeActions;

class FThe_AwakeningEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<IAssetTypeActions> StoryAssetActions;
};
