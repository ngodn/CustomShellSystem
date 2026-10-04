using UnrealBuildTool;

public class CSSSharedEditorTarget : TargetRules
{
    public CSSSharedEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("CSSSharedAuthoring");
    }
}
