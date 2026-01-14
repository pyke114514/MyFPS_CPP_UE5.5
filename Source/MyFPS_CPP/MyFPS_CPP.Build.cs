// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyFPS_CPP : ModuleRules
{
	public MyFPS_CPP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Niagara",
			"Sockets",
			"AIModule",
			"GameplayTasks",
			"NavigationSystem",
			}
		);
	}
}
