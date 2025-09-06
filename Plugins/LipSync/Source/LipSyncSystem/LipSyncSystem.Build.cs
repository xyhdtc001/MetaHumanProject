// Copyright 2022 Stendhal Syndrome Studio. All Rights Reserved.

using UnrealBuildTool;

public class LipSyncSystem : ModuleRules
{
	public LipSyncSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"InputCore",
			}
			);
		
		const string thirdPartyLibsDir = "$(PluginDir)/Source/ThirdParty/LipSyncSystemLibrary/Win64";
		const string libName           = "KkLipSync";
		const string source            = $"{thirdPartyLibsDir}/{libName}.dll";
		RuntimeDependencies.Add(source);
	}
}
