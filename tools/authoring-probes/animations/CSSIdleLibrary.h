#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSIdleLibrary.generated.h"

/** Editor-only prototypes for optional full-pose idle playback. */
UCLASS()
class UCSSIdleLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Copy a post-process graph and insert a switch before its existing rig chain. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static class UAnimBlueprint* CreateIdleLayer(class UAnimBlueprint* Source,
        const FString& OutputPackage);
};
