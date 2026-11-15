// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClaudeAITest : ModuleRules
{
	public ClaudeAITest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		// HUD de l'Hyperion : widgets construits en C++ (Slate/UMG) et lecture des moteurs du vaisseau.
		// Propulseurs de l'Hyperion : poussee reelle via la mecanique orbitale (SolarMechanics).
		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "HyperionShip", "SolarMechanics" });

		PublicIncludePaths.AddRange(new string[] {
			"ClaudeAITest",
			"ClaudeAITest/Variant_Platforming",
			"ClaudeAITest/Variant_Platforming/Animation",
			"ClaudeAITest/Variant_Combat",
			"ClaudeAITest/Variant_Combat/AI",
			"ClaudeAITest/Variant_Combat/Animation",
			"ClaudeAITest/Variant_Combat/Gameplay",
			"ClaudeAITest/Variant_Combat/Interfaces",
			"ClaudeAITest/Variant_Combat/UI",
			"ClaudeAITest/Variant_SideScrolling",
			"ClaudeAITest/Variant_SideScrolling/AI",
			"ClaudeAITest/Variant_SideScrolling/Gameplay",
			"ClaudeAITest/Variant_SideScrolling/Interfaces",
			"ClaudeAITest/Variant_SideScrolling/UI",
			"ClaudeAITest/HyperionUI",
			"ClaudeAITest/HyperionFlight"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
