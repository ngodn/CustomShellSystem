using UnrealBuildTool;

public class CSSRigAuditEditorTarget : TargetRules
{
    public CSSRigAuditEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("CSSRigAudit");
    }
}
