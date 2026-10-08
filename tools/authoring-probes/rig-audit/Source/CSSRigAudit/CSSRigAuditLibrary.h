#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSRigAuditLibrary.generated.h"

UCLASS()
class UCSSRigAuditLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString SnapshotHandPostProcess(class USkeletalMesh* Mesh, class UAnimBlueprint* Blueprint,
                                          const FString& SnapshotJson, const TArray<FName>& Curves,
                                          bool bCompatibleInput);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString SnapshotAssetProperties(class UObject* Asset);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString SnapshotAnimation(class UAnimSequence* Sequence);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString SnapshotSkeleton(class USkeleton* Skeleton);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString SnapshotMesh(class USkeletalMesh* Mesh, bool bInfluenceCoverage = false);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString SnapshotMeshSections(class USkeletalMesh* Mesh, const TArray<FName>& MaterialSlots,
                                        bool bDigestArrays = false);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString SnapshotRenderSections(class USkeletalMesh* Mesh, const TArray<FName>& MaterialSlots,
                                          const TArray<FName>& MorphNames, bool bDigestArrays = false);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString CompareMeshCloth(class USkeletalMesh* Source, class USkeletalMesh* Candidate);

    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString SnapshotClothCustomData(class USkeletalMesh* Mesh);
};
