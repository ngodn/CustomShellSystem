using UnrealBuildTool;

public class CSSRigAudit : ModuleRules
{
    public CSSRigAudit(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "Json", "MeshDescription", "StaticMeshDescription", "SkeletalMeshDescription", "AnimationCore" });
    }
}
