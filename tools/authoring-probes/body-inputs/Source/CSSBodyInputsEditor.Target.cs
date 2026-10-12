using UnrealBuildTool;
public class CSSBodyInputsEditorTarget : TargetRules
{
    public CSSBodyInputsEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("CSSBodyInputs");
    }
}
