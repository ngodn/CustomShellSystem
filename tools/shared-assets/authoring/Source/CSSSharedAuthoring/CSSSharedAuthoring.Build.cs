using UnrealBuildTool;

public class CSSSharedAuthoring : ModuleRules
{
    public CSSSharedAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "UnrealEd", "AnimGraph", "AnimGraphRuntime", "BlueprintGraph",
            "Kismet", "KismetCompiler", "AssetTools", "Json"
        });
    }
}
