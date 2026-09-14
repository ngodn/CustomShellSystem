using UnrealBuildTool;

public class CSSAuthoringEditorTarget : TargetRules
{
    public CSSAuthoringEditorTarget(TargetInfo target) : base(target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("CSSAuthoring");
        // Keep import independent of experimental cross-host cooking modules.
        // Epic's installed Linux editor does not precompile those modules.
    }
}
