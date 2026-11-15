using UnrealBuildTool;

public class HyperionShip : ModuleRules
{
	public HyperionShip(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Niagara"
		});
	}
}
