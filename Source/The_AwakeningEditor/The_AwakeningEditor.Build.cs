// Source/The_AwakeningEditor/The_AwakeningEditor.Build.cs
using UnrealBuildTool;

public class The_AwakeningEditor : ModuleRules
{
	public The_AwakeningEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Slate",
			"SlateCore",
			"UMG",
			"Blutility",
			"Json",
			"JsonUtilities",
			"AssetRegistry",
			"InputCore",
			"The_Awakening"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
