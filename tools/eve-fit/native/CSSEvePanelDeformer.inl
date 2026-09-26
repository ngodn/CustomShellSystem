#include "Utils/ClothingMeshUtils.h"
#include "PointWeightMap.h"

static bool GeneratePanelDeformer(UE::Chaos::ClothAsset::FCollectionClothFacade& Cloth,bool Repair)
{
    using namespace UE::Chaos::ClothAsset;
    Cloth.DefineSchema(EClothCollectionExtendedSchemas::RenderDeformer);
    TArray<uint32> SimIndices,RenderIndices;
    for (const FIntVector3& F:Cloth.GetSimIndices3D()) for (int32 I=0;I<3;++I) SimIndices.Add(F[I]);
    for (const FIntVector3& F:Cloth.GetRenderIndices()) for (int32 I=0;I<3;++I) RenderIndices.Add(F[I]);
    const ClothingMeshUtils::ClothMeshDesc Source(Cloth.GetSimPosition3D(),SimIndices);
    const ClothingMeshUtils::ClothMeshDesc Target(Cloth.GetRenderPosition(),Cloth.GetRenderNormal(),Cloth.GetRenderTangentU(),RenderIndices);
    const FPointWeightMap Distances(Cloth.GetWeightMap(TEXT("MaxDistance")));
    TArray<FMeshToMeshVertData> Mapping;
    ClothingMeshUtils::GenerateMeshToMeshVertData(Mapping,Target,Source,&Distances,true,true,30.f);
    const int32 Vertices=Cloth.GetNumRenderVertices();
    if (!Vertices || Mapping.Num()%Vertices) return false;
    const int32 Influences=Mapping.Num()/Vertices;
    if (Influences<=1) return false;
    int32 RepairedVertices=0,AlternativeVertices=0;
    for (int32 V=0;V<Vertices;++V)
    {
        float Blend=0.f,BlendWeight=0.f,Total=0.f;
        bool FullySkinned=true;
        for (int32 I=0;I<Influences;++I)
        {
            const auto& M=Mapping[V*Influences+I];
            Blend+=M.Weight*float(M.SourceMeshVertIndices[3])/65535.f;
            BlendWeight+=M.Weight;
            if (M.Weight>0.f && M.SourceMeshVertIndices[3]!=65535) FullySkinned=false;
        }
        if (!FMath::IsNearlyEqual(BlendWeight,1.f,.001f)) return false;
        // Preserve the exact kinematic endpoint before the builder quantizes to uint16.
        Blend=FullySkinned?1.f:FMath::Clamp(Blend/BlendWeight,0.f,1.f);
        if (Repair && Blend<1.f)
        {
            const FVector3f Position=Cloth.GetRenderPosition()[V];
            auto Reconstruct=[&](const FMeshToMeshVertData& M,const FVector4f& Bary)
            {
                FVector3f P=FVector3f::ZeroVector;
                for (int32 I=0;I<3;++I)
                {
                    const int32 Index=M.SourceMeshVertIndices[I];
                    P+=Bary[I]*(Source.GetPositions()[Index]-Source.GetNormals()[Index]*Bary.W);
                }
                return P;
            };
            auto Accurate=[&](const FMeshToMeshVertData& M)
            {
                if (M.Weight<=0.f) return false;
                for (int32 I=0;I<3;++I)
                    if (M.SourceMeshVertIndices[I]>=Source.GetPositions().Num()) return false;
                const FVector4f& B=M.PositionBaryCoordsAndDist;
                const float Extrapolation=FMath::Abs(B.X)+FMath::Abs(B.Y)+FMath::Abs(B.Z);
                return Extrapolation<=8.f &&
                    FVector3f::Distance(Reconstruct(M,B),Position)<=.01f &&
                    FVector3f::Distance(Reconstruct(M,M.NormalBaryCoordsAndDist),Position+Cloth.GetRenderNormal()[V])<=.01f &&
                    FVector3f::Distance(Reconstruct(M,M.TangentBaryCoordsAndDist),Position+Cloth.GetRenderTangentU()[V])<=.01f;
            };
            TArray<FMeshToMeshVertData> Candidates,Good;
            for (int32 I=0;I<Influences;++I) Candidates.Add(Mapping[V*Influences+I]);
            float Nearest=FLT_MAX;
            for (const auto& M:Candidates)
            {
                if (M.Weight<=0.f) continue;
                const FVector A(Source.GetPositions()[M.SourceMeshVertIndices[0]]);
                const FVector B(Source.GetPositions()[M.SourceMeshVertIndices[1]]);
                const FVector C(Source.GetPositions()[M.SourceMeshVertIndices[2]]);
                Nearest=FMath::Min(Nearest,float(FVector::Distance(FVector(Position),FMath::ClosestPointOnTriangleToPoint(FVector(Position),A,B,C))));
                if (Accurate(M)) Good.Add(M);
            }
            ClothingMeshUtils::FMeshToMeshFilterSet Filter;
            bool UsedAlternative=false;
            if (Good.IsEmpty())
            {
                Filter.TargetVertices.Add(0);
                for (int32 I=0;I<SimIndices.Num()/3;++I) Filter.SourceTriangles.Add(I);
            }
            for (int32 Attempt=0;Good.IsEmpty() && Attempt<8;++Attempt)
            {
                for (const auto& M:Candidates)
                    for (int32 I=0;I<SimIndices.Num()/3;++I)
                        if (SimIndices[I*3]==M.SourceMeshVertIndices[0] && SimIndices[I*3+1]==M.SourceMeshVertIndices[1] && SimIndices[I*3+2]==M.SourceMeshVertIndices[2])
                            Filter.SourceTriangles.Remove(I);
                TArray<FVector3f> Point={Position},Normal={Cloth.GetRenderNormal()[V]},Tangent={Cloth.GetRenderTangentU()[V]};
                TArray<uint32> NoFaces;
                const ClothingMeshUtils::ClothMeshDesc Single(Point,Normal,Tangent,NoFaces);
                TArray<ClothingMeshUtils::FMeshToMeshFilterSet> Filters={Filter};
                ClothingMeshUtils::GenerateMeshToMeshVertData(Candidates,Single,Source,&Distances,true,true,30.f,Filters);
                for (const auto& M:Candidates)
                {
                    if (!Accurate(M)) continue;
                    const FVector A(Source.GetPositions()[M.SourceMeshVertIndices[0]]);
                    const FVector B(Source.GetPositions()[M.SourceMeshVertIndices[1]]);
                    const FVector C(Source.GetPositions()[M.SourceMeshVertIndices[2]]);
                    if (FVector::Distance(FVector(Position),FMath::ClosestPointOnTriangleToPoint(FVector(Position),A,B,C))<=Nearest+2.f)
                        Good.Add(M);
                }
                if (!Good.IsEmpty())
                {
                    ++AlternativeVertices;
                    UsedAlternative=true;
                }
            }
            if (Good.IsEmpty())
            {
                UE_LOG(LogCSSEvePanel,Error,TEXT("No accurate local cloth mapping at render vertex %d; skin blend %.9g, nearest %.9g cm"),V,Blend,Nearest);
                for (int32 Pass=0;Pass<2;++Pass)
                {
                    const int32 Count=Pass?Candidates.Num():Influences;
                    for (int32 I=0;I<Count;++I)
                    {
                        const auto& M=Pass?Candidates[I]:Mapping[V*Influences+I];
                        const auto& B=M.PositionBaryCoordsAndDist;
                        UE_LOG(LogCSSEvePanel,Display,TEXT("Mapping pass %d slot %d weight %.9g baryL1 %.9g errors P/N/T %.9g %.9g %.9g"),
                            Pass,I,M.Weight,FMath::Abs(B.X)+FMath::Abs(B.Y)+FMath::Abs(B.Z),
                            FVector3f::Distance(Reconstruct(M,B),Position),
                            FVector3f::Distance(Reconstruct(M,M.NormalBaryCoordsAndDist),Position+Cloth.GetRenderNormal()[V]),
                            FVector3f::Distance(Reconstruct(M,M.TangentBaryCoordsAndDist),Position+Cloth.GetRenderTangentU()[V]));
                    }
                }
                return false;
            }
            float GoodWeight=0.f;
            for (const auto& M:Good) GoodWeight+=M.Weight;
            if (Good.Num()!=Influences || UsedAlternative) ++RepairedVertices;
            for (int32 I=0;I<Influences;++I)
            {
                Mapping[V*Influences+I]=Good[I<Good.Num()?I:0];
                Mapping[V*Influences+I].Weight=I<Good.Num()?Good[I].Weight/GoodWeight:0.f;
            }
        }
        auto& P=Cloth.GetRenderDeformerPositionBaryCoordsAndDist()[V];
        auto& N=Cloth.GetRenderDeformerNormalBaryCoordsAndDist()[V];
        auto& T=Cloth.GetRenderDeformerTangentBaryCoordsAndDist()[V];
        auto& Indices=Cloth.GetRenderDeformerSimIndices3D()[V];
        auto& Weights=Cloth.GetRenderDeformerWeight()[V];
        P.SetNum(Influences); N.SetNum(Influences); T.SetNum(Influences);
        Indices.SetNum(Influences); Weights.SetNum(Influences);
        for (int32 I=0;I<Influences;++I)
        {
            const auto& M=Mapping[V*Influences+I];
            P[I]=M.PositionBaryCoordsAndDist; N[I]=M.NormalBaryCoordsAndDist; T[I]=M.TangentBaryCoordsAndDist;
            Indices[I]=FIntVector3(M.SourceMeshVertIndices[0],M.SourceMeshVertIndices[1],M.SourceMeshVertIndices[2]);
            Weights[I]=M.Weight;
            Total+=M.Weight;
        }
        if (!FMath::IsNearlyEqual(Total,1.f,.001f) || !FMath::IsFinite(Blend)) return false;
        Cloth.GetRenderDeformerSkinningBlend()[V]=Blend;
    }
    for (int32 I=0;I<Cloth.GetNumRenderPatterns();++I)
        Cloth.GetRenderPattern(I).SetRenderDeformerNumInfluences(Influences);
    UE_LOG(LogCSSEvePanel,Display,TEXT("Explicit cloth mapping: %d vertices, %d influences each; %d repaired, %d alternate supports"),Vertices,Influences,RepairedVertices,AlternativeVertices);
    return true;
}
