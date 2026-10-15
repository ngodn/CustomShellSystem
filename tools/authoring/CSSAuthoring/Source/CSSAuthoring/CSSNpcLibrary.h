#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSNpcLibrary.generated.h"

UCLASS()
class UCSSNpcLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static bool ClearBossPreview(class USkeleton* Target);

    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static int32 ConfigureAnimationReference(class USkeleton* Target, class USkeleton* Human);
};
