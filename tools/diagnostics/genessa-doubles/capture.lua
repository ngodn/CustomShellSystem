-- UE4SS Lua 5.4, on-demand reads only. No hooks, asset loads or retained actors.
local function valid(object) return object ~= nil and object:IsValid() end
local function name(object) return valid(object) and object:GetFullName() or "none" end
local function unwrap(value)
    if value and value:type() == "RemoteUnrealParam" then return value:get() end
    return value
end

return function(pawn, emit)
    emit("ASTRAL_BEGIN")
    emit("player=" .. name(pawn) .. " shell=" .. pawn.CharacterId.TagName:ToString())
    local mesh = pawn.Mesh
    emit("player.mesh=" .. (valid(mesh) and name(mesh:GetSkeletalMeshAsset()) or "none"))
    local spawnerClass = StaticFindObject("/Game/Sparta/Core/Components/BPC_AstralAISpawner.BPC_AstralAISpawner_C")
    local astralClass = StaticFindObject("/Game/Sparta/Core/AI/Components/BPC_AstralAI.BPC_AstralAI_C")
    if not valid(spawnerClass) then emit("spawner=class unavailable"); emit("ASTRAL_END"); return end
    local spawner = pawn:GetComponentByClass(spawnerClass)
    emit("spawner=" .. name(spawner))
    if not valid(spawner) then emit("ASTRAL_END"); return end
    local actors = spawner.AllCharacters
    emit("count=" .. #actors)
    if #actors > 32 then error("Spawner character count exceeds diagnostic bound") end
    for index = 1, #actors do
        local actor = unwrap(actors[index])
        if valid(actor) then
            local prefix = "clone[" .. index .. "]."
            emit(prefix .. "actor=" .. name(actor) .. " owner=" .. name(actor:GetOwner()))
            emit(prefix .. "hidden=" .. tostring(actor.bHidden))
            local astral = valid(astralClass) and actor:GetComponentByClass(astralClass) or nil
            emit(prefix .. "astral=" .. name(astral))
            if valid(astral) then
                emit(prefix .. "id=" .. astral.MyID:ToString() .. " cached=" .. tostring(astral.bIsCached)
                    .. " initialized=" .. tostring(astral.bInitialized) .. " enabled=" .. tostring(astral.bCharacterEnabled))
                local mid = astral.MID_Astral
                emit(prefix .. "ghost=" .. name(mid) .. " material=" .. name(astral.Material))
                if valid(mid) then emit(prefix .. "opacity=" .. mid:K2_GetScalarParameterValue(FName("GlobalOpacity"))) end
                local target = astral.MySkeletalMesh
                if valid(target) then
                    emit(prefix .. "mesh=" .. name(target:GetSkeletalMeshAsset())
                        .. " post=" .. name(target:GetPostProcessInstance()) .. " anim=" .. name(target:GetAnimInstance()))
                    emit(prefix .. "clothDisabled=" .. tostring(target.bDisableClothSimulation)
                        .. " rigidBodyDisabled=" .. tostring(target.bDisableRigidBodyAnimNode)
                        .. " physicsOverride=" .. name(target.PhysicsAssetOverride))
                    local count = target:GetNumMaterials()
                    if count > 128 then error("Mesh material count exceeds diagnostic bound") end
                    for slot = 0, count - 1 do emit(prefix .. "material[" .. slot .. "]=" .. name(target:GetMaterial(slot))) end
                end
            end
        end
    end
    emit("ASTRAL_END")
end
