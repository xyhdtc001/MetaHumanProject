// Copyright 2023 Stendhal Syndrome Studio. All Rights Reserved.

using UnrealBuildTool;

public class LipSyncSystemEditor : ModuleRules
{
    public LipSyncSystemEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore", 
                "LipSyncSystem"
            }
        );
    }
}