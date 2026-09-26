static TSharedPtr<FJsonObject> ExportPanelRenderMapping(const UChaosClothAsset* Asset)
{
    const auto& Model=Asset->GetImportedModel()->LODModels[0];
    auto Result=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Sections;
    for (const auto& Section:Model.Sections)
    {
        auto Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("material"),Asset->GetMaterials()[Section.MaterialIndex].MaterialSlotName.ToString());
        TArray<TSharedPtr<FJsonValue>> Positions,Weights,Faces,Mappings;
        for (const auto& V:Section.SoftVertices)
        {
            TArray<TSharedPtr<FJsonValue>> P={MakeShared<FJsonValueNumber>(V.Position.X),MakeShared<FJsonValueNumber>(V.Position.Y),MakeShared<FJsonValueNumber>(V.Position.Z)};
            Positions.Add(MakeShared<FJsonValueArray>(P));
            TArray<TSharedPtr<FJsonValue>> Influences;
            for (int32 I=0;I<Section.MaxBoneInfluences;++I)
            {
                if (!V.InfluenceWeights[I]) continue;
                const FName Bone=Asset->GetRefSkeleton().GetBoneName(Section.BoneMap[V.InfluenceBones[I]]);
                TArray<TSharedPtr<FJsonValue>> Pair={MakeShared<FJsonValueString>(Bone.ToString()),MakeShared<FJsonValueNumber>(V.InfluenceWeights[I]/65535.f)};
                Influences.Add(MakeShared<FJsonValueArray>(Pair));
            }
            Weights.Add(MakeShared<FJsonValueArray>(Influences));
        }
        for (uint32 I=0;I<Section.NumTriangles*3;++I)
            Faces.Add(MakeShared<FJsonValueNumber>(int32(Model.IndexBuffer[Section.BaseIndex+I])-Section.BaseVertexIndex));
        if (!Section.ClothMappingDataLODs.IsEmpty())
        {
            for (const auto& M:Section.ClothMappingDataLODs[0])
            {
                TArray<TSharedPtr<FJsonValue>> Values;
                for (int32 I=0;I<4;++I) Values.Add(MakeShared<FJsonValueNumber>(M.SourceMeshVertIndices[I]));
                for (int32 I=0;I<4;++I) Values.Add(MakeShared<FJsonValueNumber>(M.PositionBaryCoordsAndDist[I]));
                Values.Add(MakeShared<FJsonValueNumber>(M.Weight));
                Mappings.Add(MakeShared<FJsonValueArray>(Values));
            }
        }
        Row->SetArrayField(TEXT("positions"),Positions);
        Row->SetArrayField(TEXT("weights"),Weights);
        Row->SetArrayField(TEXT("indices"),Faces);
        Row->SetArrayField(TEXT("mapping"),Mappings);
        Sections.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result->SetArrayField(TEXT("sections"),Sections);
    Result->SetStringField(TEXT("mapping_layout"),TEXT("sim_i0,i1,i2,skin_blend_u16,bary_x,y,z,normal_distance,weight"));
    return Result;
}
