using UnrealBuildTool;

public class CSSLightingEditorTarget : TargetRules
{
    public CSSLightingEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("CSSLighting");
    }
}
