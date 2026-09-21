using UnrealBuildTool;
using System.IO;

public class CSSAuthoring : ModuleRules
{
    public CSSAuthoring(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        // Pinned UE 5.6.1 AssetHeaderPatcher uses a misplaced UE_INTERNAL attribute
        // in Clang. Its version-specific header migration is audited separately.
        bValidateInternalApi = false;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "AssetTools", "Engine", "UnrealEd", "Json",
            "AssetRegistry", "MeshDescription", "SkeletalMeshDescription",
            "TargetPlatform", "DesktopPlatform", "AudioPlatformConfiguration", "RHI", "RenderCore",
            "ControlRig", "ControlRigDeveloper", "RigVM", "RigVMDeveloper",
            "ClothingSystemEditor", "ClothingSystemEditorInterface", "ClothingSystemRuntimeCommon", "ChaosCloth"
        });
        PrivateIncludePathModuleNames.Add("TextureCompressor");
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Developer/AssetTools/Internal"));
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/CoreUObject/Internal"));
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory,
            "Source/Developer/Windows/WindowsTargetPlatformSettings/Public"));
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory,
            "Source/Developer/Windows/WindowsTargetPlatfomControls/Public"));
    }
}
