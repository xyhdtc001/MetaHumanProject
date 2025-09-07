// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MetaHumanProject : ModuleRules
{
	public MetaHumanProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput","Slate",
			"SlateCore", 
			"AudioMixer", 
			"AudioCaptureCore", 
			"AudioCapture", 
			"DeveloperSettings", 
			"ImageWrapper",
			"Json",  // 添加JSON模块
			"HTTP"   // 添加HTTP模块
            });

		PrivateDependencyModuleNames.AddRange(new string[] { "CommandSystem","LipSyncSystem","RuntimeAudioImporter" });
		
		// iFlytek SDK Integration
		string IFlytekPath = Path.Combine(ModuleDirectory, "ThirdParty", "iFlytek");
		string IFlytekIncludePath = Path.Combine(IFlytekPath, "include");
		string IFlytekLibPath = Path.Combine(IFlytekPath, "lib");
		string IFlytekBinPath = Path.Combine(IFlytekPath, "bin");

		PublicIncludePaths.Add(IFlytekIncludePath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(IFlytekLibPath, "msc_x64.lib"));
			RuntimeDependencies.Add("$(BinaryOutputDir)/msc_x64.dll", Path.Combine(IFlytekBinPath, "msc_x64.dll"));
			
			// 确保DLL被复制到输出目录
			PublicDelayLoadDLLs.Add("msc_x64.dll");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
