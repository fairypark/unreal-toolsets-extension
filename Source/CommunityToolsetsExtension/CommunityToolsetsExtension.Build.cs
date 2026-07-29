using UnrealBuildTool;

public class CommunityToolsetsExtension : ModuleRules
{
	public CommunityToolsetsExtension(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Landscape",
				"ToolsetRegistry"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Foliage",
				"LandscapeEditor",
				"UnrealEd"
			});
	}
}
