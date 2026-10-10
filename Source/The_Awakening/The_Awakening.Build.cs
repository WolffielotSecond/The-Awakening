// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class The_Awakening : ModuleRules
{
	public The_Awakening(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
            "GameplayAbilities",
            "GameplayTags",
			"GameplayTasks",
            "AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
            "SlateCore",
            "Json",
			"JsonUtilities",
			"Niagara",
			"Paper2D",
			"PaperZD",
			"AssetRegistry"
        });

		PrivateDependencyModuleNames.AddRange(new string[] { "ApplicationCore" });

		PublicIncludePaths.AddRange(new string[] {
			"The_Awakening"
		});

		// 剧情编辑器模块（The_AwakeningEditor）需要引用本模块头文件（Story/...、Core/... 等）
		PublicIncludePaths.Add(ModuleDirectory);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
