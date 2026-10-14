using UnrealBuildTool;

public class CSSAuthoring : ModuleRules
{
    public CSSAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Json",
            "ClothingSystemEditor", "ClothingSystemRuntimeCommon", "ClothingSystemRuntimeInterface"
        });
    }
}
