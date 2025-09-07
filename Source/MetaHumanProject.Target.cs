// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MetaHumanProjectTarget : TargetRules
{
	public MetaHumanProjectTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		ExtraModuleNames.Add("MetaHumanProject");
		
		// 在非编辑器构建中禁用有问题的插件
		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			DisablePlugins.Add("MetaHuman");
			DisablePlugins.Add("DNACalibModule");
		}
	}
}
