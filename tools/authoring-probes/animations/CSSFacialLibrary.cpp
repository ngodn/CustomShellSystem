#include "CSSFacialLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
#include "AssetCompilingManager.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SkeletalMeshAttributes.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"

FString UCSSFacialLibrary::AddExpressionShapes(USkeletalMesh* Mesh, const FString& InputFile)
{
    if (!IsRunningCommandlet() || !Mesh ||
        !Mesh->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/SK_Face_")))
        return TEXT("ERROR: requires an isolated commandlet mesh");
    if (Mesh->GetLODNum() != 1 || !Mesh->GetMeshDescription(0))
        return TEXT("ERROR: expected one source-description LOD");
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text, *InputFile) || Text.Len() > 64*1024*1024 ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
        return TEXT("ERROR: invalid facial interchange");
    const TArray<TSharedPtr<FJsonValue>> *Points=nullptr, *Shapes=nullptr;
    int32 Schema=0;
    if (!Root->TryGetNumberField(TEXT("schema"), Schema) || Schema!=1 ||
        !Root->TryGetArrayField(TEXT("body_points"), Points) || Points->IsEmpty() || Points->Num()>100000 ||
        !Root->TryGetArrayField(TEXT("morph_targets"), Shapes) || Shapes->IsEmpty() || Shapes->Num()>128)
        return TEXT("ERROR: invalid facial counts or schema");
    // Build a detached description first. Validation cannot partly alter the candidate.
    FMeshDescription Description=*Mesh->GetMeshDescription(0);
    FSkeletalMeshAttributes Attributes(Description);
    const auto Positions=Attributes.GetVertexPositions();
    auto Numbers=[](const TSharedPtr<FJsonValue>& Value, int32 Count, TArray<double>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if (!Value->TryGetArray(Values) || Values->Num()!=Count) return false;
        Out.Reset();
        for (const auto& Entry:*Values) {
            double Number=0;
            if (!Entry->TryGetNumber(Number) || !FMath::IsFinite(Number)) return false;
            Out.Add(Number);
        }
        return true;
    };
    TArray<double> Values;
    for (int32 Index=0; Index<Points->Num(); ++Index) {
        if (!Description.Vertices().IsValid(FVertexID(Index)) || !Numbers((*Points)[Index],3,Values) ||
            FVector3f::Distance(Positions[FVertexID(Index)],FVector3f(Values[0],Values[1],Values[2]))>.0002f)
            return FString::Printf(TEXT("ERROR: body point order/fit differs at %d"),Index);
    }
    const int32 PreviousCount=Mesh->GetMorphTargets().Num();
    TSet<FName> Names;
    for (const auto& Value:*Shapes) {
        const TSharedPtr<FJsonObject>* Shape=nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Deltas=nullptr;
        FString Name;
        if (!Value->TryGetObject(Shape) || !(*Shape)->TryGetStringField(TEXT("name"),Name) ||
            Name.IsEmpty() || Name.Len()>64 || Names.Contains(FName(*Name)) || Mesh->FindMorphTarget(FName(*Name)) ||
            !(*Shape)->TryGetArrayField(TEXT("deltas"),Deltas) || Deltas->IsEmpty() || Deltas->Num()>Points->Num())
            return TEXT("ERROR: invalid or existing expression shape");
        for (TCHAR Character:Name)
            if (!FChar::IsAlnum(Character) && Character!=TEXT('_')) return TEXT("ERROR: invalid shape name");
        const FName MorphName(*Name);
        Names.Add(MorphName);
        if (!Attributes.RegisterMorphTargetAttribute(MorphName,false)) return TEXT("ERROR: cannot register morph attribute");
        auto MorphPositions=Attributes.GetVertexMorphPositionDelta(MorphName);
        TSet<int32> Seen;
        for (const auto& Delta:*Deltas) {
            if (!Numbers(Delta,4,Values) || Values[0]<0 || Values[0]>=Points->Num() ||
                Values[0]!=FMath::FloorToDouble(Values[0])) return TEXT("ERROR: invalid morph index");
            const int32 Index=static_cast<int32>(Values[0]);
            const FVector3f Position(Values[1],Values[2],Values[3]);
            if (Seen.Contains(Index) || Position.Size()>20) return TEXT("ERROR: duplicate or oversized face delta");
            Seen.Add(Index);
            MorphPositions[FVertexID(Index)]=Position;
        }
    }
    Mesh->CreateMeshDescription(0,MoveTemp(Description));
    Mesh->CommitMeshDescription(0);
    Mesh->Build();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (Mesh->GetMorphTargets().Num()!=PreviousCount+Names.Num())
        return TEXT("ERROR: built morph count differs");
    for (FName Name:Names) {
        UMorphTarget* Morph=Mesh->FindMorphTarget(Name);
        if (!Morph || !Morph->HasDataForLOD(0)) return TEXT("ERROR: empty built facial shape");
    }
    return FString::Printf(TEXT("PASS: retained %d morphs, added %d expression shapes"),PreviousCount,Names.Num());
}

FString UCSSFacialLibrary::VerifyExpressionShapes(USkeletalMesh* Source, USkeletalMesh* Candidate,
    const FString& InputFile)
{
    if (!IsRunningCommandlet() || !Source || !Candidate || Source==Candidate ||
        !Candidate->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/SK_Face_")))
        return TEXT("ERROR: invalid comparison assets");
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FMeshDescription *Before=Source->GetMeshDescription(0), *After=Candidate->GetMeshDescription(0);
    if (!Before || !After || Before->Vertices().Num()!=After->Vertices().Num() ||
        Before->Triangles().Num()!=After->Triangles().Num()) return TEXT("ERROR: topology count changed");
    FSkeletalMeshConstAttributes Old(*Before), Now(*After);
    const auto OldPositions=Old.GetVertexPositions(), NewPositions=Now.GetVertexPositions();
    for (FVertexID Vertex:Before->Vertices().GetElementIDs())
        if (!After->Vertices().IsValid(Vertex) || OldPositions[Vertex]!=NewPositions[Vertex])
            return TEXT("ERROR: base vertex positions changed");
    for (UMorphTarget* Morph:Source->GetMorphTargets()) {
        const auto A=Old.GetVertexMorphPositionDelta(Morph->GetFName());
        const auto B=Now.GetVertexMorphPositionDelta(Morph->GetFName());
        if (!A.IsValid() || !B.IsValid()) return TEXT("ERROR: original morph attribute missing");
        for (FVertexID Vertex:Before->Vertices().GetElementIDs())
            if (A[Vertex]!=B[Vertex]) return TEXT("ERROR: original morph deltas changed");
    }
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text,*InputFile) || Text.Len()>64*1024*1024 ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root)
        return TEXT("ERROR: invalid comparison interchange");
    const TArray<TSharedPtr<FJsonValue>>* Shapes=nullptr;
    if (!Root->TryGetArrayField(TEXT("morph_targets"),Shapes) || Shapes->IsEmpty() || Shapes->Num()>128)
        return TEXT("ERROR: invalid comparison shapes");
    const FSkeletalMeshModel* Model=Candidate->GetImportedModel();
    if (!Model || Model->LODModels.Num()!=1) return TEXT("ERROR: unexpected built LODs");
    const auto& LOD=Model->LODModels[0];
    double MaxError=0;
    int32 Compared=0;
    for (const auto& Value:*Shapes) {
        const TSharedPtr<FJsonObject>* Shape=nullptr;
        FString Name;
        const TArray<TSharedPtr<FJsonValue>>* Deltas=nullptr;
        if (!Value->TryGetObject(Shape) || !(*Shape)->TryGetStringField(TEXT("name"),Name) ||
            !(*Shape)->TryGetArrayField(TEXT("deltas"),Deltas)) return TEXT("ERROR: invalid comparison shape");
        const auto Attribute=Now.GetVertexMorphPositionDelta(FName(*Name));
        if (!Attribute.IsValid()) return TEXT("ERROR: facial attribute missing");
        TMap<int32,FVector3f> Expected;
        for (const auto& Delta:*Deltas) {
            const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
            if (!Delta->TryGetArray(Values) || Values->Num()!=4) return TEXT("ERROR: invalid comparison delta");
            double V[4];
            for (int32 I=0;I<4;++I)
                if (!(*Values)[I]->TryGetNumber(V[I]) || !FMath::IsFinite(V[I])) return TEXT("ERROR: invalid comparison number");
            if (V[0]<0 || V[0]>100000 || V[0]!=FMath::FloorToDouble(V[0])) return TEXT("ERROR: invalid comparison vertex");
            const int32 Index=static_cast<int32>(V[0]);
            if (!After->Vertices().IsValid(FVertexID(Index)) || Expected.Contains(Index)) return TEXT("ERROR: invalid comparison point");
            Expected.Add(Index,FVector3f(V[1],V[2],V[3]));
        }
        for (FVertexID Vertex:After->Vertices().GetElementIDs()) {
            const FVector3f Wanted=Expected.FindRef(Vertex.GetValue());
            MaxError=FMath::Max(MaxError,static_cast<double>(FVector3f::Distance(Attribute[Vertex],Wanted)));
        }
        UMorphTarget* Morph=Candidate->FindMorphTarget(FName(*Name));
        if (!Morph) return TEXT("ERROR: built morph missing");
        int32 Count=0;
        const FMorphTargetDelta* Built=Morph->GetMorphTargetDelta(0,Count);
        if (!Built || Count==0) return TEXT("ERROR: built morph empty");
        for (int32 I=0;I<Count;++I) {
            if (!LOD.MeshToImportVertexMap.IsValidIndex(Built[I].SourceIdx)) return TEXT("ERROR: built morph index invalid");
            const int32 Point=LOD.MeshToImportVertexMap[Built[I].SourceIdx];
            MaxError=FMath::Max(MaxError,static_cast<double>(FVector3f::Distance(Built[I].PositionDelta,Expected.FindRef(Point))));
            ++Compared;
        }
    }
    if (MaxError>.0002) return FString::Printf(TEXT("ERROR: facial delta error %.9f cm"),MaxError);
    return FString::Printf(TEXT("PASS: %d shapes, %d built deltas, maximum error %.9f cm"),Shapes->Num(),Compared,MaxError);
}
