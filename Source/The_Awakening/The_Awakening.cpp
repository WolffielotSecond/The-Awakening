// Copyright Epic Games, Inc. All Rights Reserved.

#include "The_Awakening.h"
#include "Modules/ModuleManager.h"
#include "Core/TAVersionLibrary.h"

class FTheAwakeningModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		UE_LOG(LogThe_Awakening, Display, TEXT("The Awakening %s (save format %d)"),
			*UTAVersionLibrary::GetFullVersion(), UTAVersionLibrary::GetSaveFormatVersion());
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FTheAwakeningModule, The_Awakening, "The_Awakening" );

DEFINE_LOG_CATEGORY(LogThe_Awakening)
