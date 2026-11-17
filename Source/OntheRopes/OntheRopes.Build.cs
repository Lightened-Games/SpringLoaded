// Copyright On the Ropes. All Rights Reserved.

using UnrealBuildTool;

public class OntheRopes : ModuleRules
{
	public OntheRopes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"Niagara",
			"EngineCameras"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "AudioExtensions" });

		// Editor-only helpers used by the content scripts (volume brushes, Niagara renderers).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.Add("OntheRopes");
	}
}
