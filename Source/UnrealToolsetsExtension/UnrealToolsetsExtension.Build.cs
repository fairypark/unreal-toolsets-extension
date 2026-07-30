using UnrealBuildTool;

public class UnrealToolsetsExtension : ModuleRules
{
	public UnrealToolsetsExtension(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Landscape",
				"PCG",
				"ToolsetRegistry"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Foliage",
				"Fab",
				"Json",
				"LandscapeEditor",
				"Projects",
				"UnrealEd"
			});
	}
}
