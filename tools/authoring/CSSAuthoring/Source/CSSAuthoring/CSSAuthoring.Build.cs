using UnrealBuildTool;
using System.IO;

public class CSSAuthoring : ModuleRules
{
    public CSSAuthoring(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "Json",
            "AssetRegistry", "MeshDescription", "SkeletalMeshDescription",
            "TargetPlatform", "DesktopPlatform", "AudioPlatformConfiguration", "RHI", "RenderCore",
            "ControlRig", "ControlRigDeveloper", "RigVM", "RigVMDeveloper",
            "ClothingSystemEditor", "ClothingSystemEditorInterface", "ClothingSystemRuntimeCommon", "ChaosCloth"
        });
        PrivateIncludePathModuleNames.Add("TextureCompressor");
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory,
            "Source/Developer/Windows/WindowsTargetPlatformSettings/Public"));
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory,
            "Source/Developer/Windows/WindowsTargetPlatfomControls/Public"));
    }
}
