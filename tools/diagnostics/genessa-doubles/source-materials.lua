-- Read the effective player surfaces, including transient customization MIDs.
local function valid(value) return value ~= nil and value:IsValid() end
local function name(value) return valid(value) and value:GetFullName() or "none" end
local function unwrap(value)
    if value and value:type() == "RemoteUnrealParam" then return value:get() end
    return value
end
local function bounded(array)
    if #array > 128 then error("Material array exceeds diagnostic bound") end
    return #array
end

local function material(value, prefix, emit)
    emit(prefix .. "material=" .. name(value))
    for depth = 1, 12 do
        if not valid(value) or not value:GetFullName():match("^MaterialInstance") then return end
        local scalars = value.ScalarParameterValues
        for index = 1, bounded(scalars) do
            local entry = unwrap(scalars[index])
            emit(prefix .. "scalar=" .. entry.ParameterInfo.Name:ToString() .. ":" .. tostring(entry.ParameterValue))
        end
        local vectors = value.VectorParameterValues
        for index = 1, bounded(vectors) do
            local entry = unwrap(vectors[index])
            local color = entry.ParameterValue
            emit(prefix .. "vector=" .. entry.ParameterInfo.Name:ToString() .. ":"
                .. color.R .. "," .. color.G .. "," .. color.B .. "," .. color.A)
        end
        local textures = value.TextureParameterValues
        for index = 1, bounded(textures) do
            local entry = unwrap(textures[index])
            emit(prefix .. "texture=" .. entry.ParameterInfo.Name:ToString() .. ":" .. name(entry.ParameterValue))
        end
        value = value.Parent
        prefix = prefix .. "parent."
        emit(prefix .. "material=" .. name(value))
    end
    error("Material parent chain exceeds diagnostic bound")
end

return function(pawn, emit)
    emit("ASTRAL_BEGIN")
    local component = pawn.Mesh
    if not valid(component) then error("Player mesh unavailable") end
    local mesh = component:GetSkeletalMeshAsset()
    if not valid(mesh) then error("Player mesh asset unavailable") end
    emit("player.mesh=" .. name(mesh))
    emit("player.globalOverlay=" .. name(component.OverlayMaterial))
    local defaults = mesh.Materials
    local overrides = component.MaterialSlotsOverlayMaterial
    local count = component:GetNumMaterials()
    bounded(defaults); bounded(overrides)
    if count < 0 or count > 128 then error("Material slot count exceeds diagnostic bound") end
    for slot = 0, count - 1 do
        local prefix = "slot[" .. slot .. "]."
        material(component:GetMaterial(slot), prefix .. "base.", emit)
        local overlay
        if slot < #overrides then overlay = unwrap(overrides[slot + 1]) end
        if not valid(overlay) and slot < #defaults then
            overlay = unwrap(defaults[slot + 1]).OverlayMaterialInterface
        end
        if valid(overlay) then material(overlay, prefix .. "overlay.", emit) end
    end
    emit("ASTRAL_END")
end
