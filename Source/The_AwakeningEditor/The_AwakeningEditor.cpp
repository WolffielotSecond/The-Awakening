// Source/The_AwakeningEditor/The_AwakeningEditor.cpp
#include "The_AwakeningEditor.h"
#include "Modules/ModuleManager.h"
#include "TADialogueAssetEditorToolkit.h"
#include "Story/TAStoryAsset.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetTypeActions_Base.h"
#include "TADialogueGraphNodeCustomization.h"
#include "TAPortraitEntryCustomization.h"
#include "TAStoryArrayEntryCustomization.h"
#include "TAStoryConditionCustomization.h"
#include "TADialogueAssetGraph.h"
#include "PropertyEditorModule.h"
#include "Story/TADialogueTypes.h"
#include "TALocalizationEditor.h"

namespace
{
	class FTAStoryAssetTypeActions final : public FAssetTypeActions_Base
	{
	public:
		virtual FText GetName() const override { return NSLOCTEXT("TAStoryAsset", "AssetTypeName", "Story"); }
		virtual FColor GetTypeColor() const override { return FColor(60, 125, 190); }
		virtual UClass* GetSupportedClass() const override { return UTAStoryAsset::StaticClass(); }
		virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }

		virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override
		{
			for (UObject* Object : InObjects)
			{
				if (UTAStoryAsset* Asset = Cast<UTAStoryAsset>(Object))
				{
					MakeShared<FTADialogueAssetEditorToolkit>()->InitEditor(EditWithinLevelEditor, Asset);
				}
			}
		}
	};
}

void FThe_AwakeningEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditor.RegisterCustomClassLayout(
		UTAStoryGraphNode::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FTAStoryGraphNodeDetails::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		FTAPortraitEntry::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTAStoryPortraitEntryCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		FTAStoryEvent::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTAStoryArrayEntryCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		FTAStoryChoice::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTAStoryArrayEntryCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(
		FTAStoryCondition::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTAStoryConditionCustomization::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	StoryAssetActions = MakeShared<FTAStoryAssetTypeActions>();
	AssetTools.RegisterAssetTypeActions(StoryAssetActions.ToSharedRef());
	TALocalizationEditor::Register();
}

void FThe_AwakeningEditorModule::ShutdownModule()
{
	TALocalizationEditor::Unregister();
	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditor.UnregisterCustomClassLayout(UTAStoryGraphNode::StaticClass()->GetFName());
		PropertyEditor.UnregisterCustomPropertyTypeLayout(FTAPortraitEntry::StaticStruct()->GetFName());
		PropertyEditor.UnregisterCustomPropertyTypeLayout(FTAStoryEvent::StaticStruct()->GetFName());
		PropertyEditor.UnregisterCustomPropertyTypeLayout(FTAStoryChoice::StaticStruct()->GetFName());
		PropertyEditor.UnregisterCustomPropertyTypeLayout(FTAStoryCondition::StaticStruct()->GetFName());
		PropertyEditor.NotifyCustomizationModuleChanged();
	}
	if (StoryAssetActions.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
	{
		FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().UnregisterAssetTypeActions(StoryAssetActions.ToSharedRef());
	}
	StoryAssetActions.Reset();
}

IMPLEMENT_MODULE(FThe_AwakeningEditorModule, The_AwakeningEditor);
