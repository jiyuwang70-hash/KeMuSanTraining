using UnrealBuildTool;

public class KeMuSanTraining : ModuleRules
{
	public KeMuSanTraining(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"AudioExtensions"
		});
	}
}
