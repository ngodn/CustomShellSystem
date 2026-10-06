#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSFacialLibrary.generated.h"

UCLASS()
class UCSSFacialLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Add measured expression shapes only to an isolated editor candidate. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString AddExpressionShapes(class USkeletalMesh* Mesh, const FString& InputFile);

    /** Read back saved candidate geometry and morphs against its source and interchange. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString VerifyExpressionShapes(class USkeletalMesh* Source, class USkeletalMesh* Candidate,
        const FString& InputFile);

    /** Compare non-expression mesh data and persistent cloth properties after a facial rebuild. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString VerifyExpressionPreservation(class USkeletalMesh* Source, class USkeletalMesh* Candidate);

    /** Export render-to-source indices for independent cooked-delta comparison. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString ExpressionVertexMap(class USkeletalMesh* Candidate);
};
