#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSPerformanceLibrary.generated.h"

UCLASS()
class UCSSPerformanceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Duplicate an audited post-process graph and add explicit-time playback before its physics. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static class UAnimBlueprint* CreatePerformanceLayer(class UAnimBlueprint* Source,
        const FString& OutputPackage);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString VerifyPerformanceLayer(class UAnimBlueprint* Source, class UAnimBlueprint* Layer);

    /** Compare local poses through original and explicit-time post-process graphs. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString EvaluatePerformanceLayer(class USkeletalMesh* Mesh, class UAnimBlueprint* Source,
        class UAnimBlueprint* Layer, class UAnimSequence* BaseClip, class UAnimSequence* PerformanceClip);
};
