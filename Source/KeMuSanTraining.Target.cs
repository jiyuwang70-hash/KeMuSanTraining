using UnrealBuildTool;
using System.Collections.Generic;

public class KeMuSanTrainingTarget : TargetRules
{
	public KeMuSanTrainingTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("KeMuSanTraining");
	}
}
