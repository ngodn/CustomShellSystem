-- Create unattached MIDs only. Never assign them to a component or edit a source.
local function valid(object) return object ~= nil and object:IsValid() end
local function name(object) return valid(object) and object:GetFullName() or "none" end
local function unwrap(value)
    if value and value:type() == "RemoteUnrealParam" then return value:get() end
    return value
end

return function(pawn, emit)
    emit("ASTRAL_BEGIN")
    local library = StaticFindObject("/Script/Engine.Default__KismetMaterialLibrary")
    if not valid(library) then error("Material library unavailable") end
    local component = pawn.Mesh
    if not valid(component) then error("Player mesh unavailable") end
    local count = component:GetNumMaterials()
    if count < 1 or count > 128 then error("Unexpected material count") end
    local tested = 0
    local families = {}
    for slot = 0, count - 1 do
        local source = component:GetMaterial(slot)
        if valid(source) and source:GetFullName():match("^MaterialInstance") then
            local scalars = source.ScalarParameterValues
            if #scalars > 128 then error("Unexpected scalar count") end
            local root = source
            for depth = 1, 12 do
                if not valid(root) or not root:GetFullName():match("^MaterialInstance") then break end
                root = root.Parent
                if depth == 12 then error("Material hierarchy exceeds bound") end
            end
            local family = name(root)
            if #scalars > 0 and tested < 8 and not families[family] then
                families[family] = true
                local parent = source.Parent
                if not valid(parent) then error("Source parent unavailable") end
                local copy = library:CreateDynamicMaterialInstance(pawn, parent, FName("None"), 0)
                if not valid(copy) or name(copy) == name(source) then error("Private MID creation failed") end
                copy:K2_CopyMaterialInstanceParameters(source, true)
                for i = 1, #scalars do
                    local entry = unwrap(scalars[i])
                    local key = entry.ParameterInfo.Name
                    local before = source:K2_GetScalarParameterValue(key)
                    local copied = copy:K2_GetScalarParameterValue(key)
                    if math.abs(copied - before) > 0.00001 then error("Scalar copy differs: " .. key:ToString()) end
                end
                local textures = source.TextureParameterValues
                if #textures > 128 then error("Unexpected texture count") end
                for i = 1, #textures do
                    local key = unwrap(textures[i]).ParameterInfo.Name
                    if name(source:K2_GetTextureParameterValue(key)) ~= name(copy:K2_GetTextureParameterValue(key)) then
                        error("Texture copy differs: " .. key:ToString())
                    end
                end
                local vectors = source.VectorParameterValues
                if #vectors > 128 then error("Unexpected vector count") end
                for i = 1, #vectors do
                    local key = unwrap(vectors[i]).ParameterInfo.Name
                    local a, b = source:K2_GetVectorParameterValue(key), copy:K2_GetVectorParameterValue(key)
                    for _, channel in ipairs({"R", "G", "B", "A"}) do
                        if math.abs(a[channel] - b[channel]) > 0.00001 then error("Vector copy differs") end
                    end
                end
                local inherited_textures = 0
                local current = parent
                for depth = 1, 12 do
                    if not valid(current) or not current:GetFullName():match("^MaterialInstance") then break end
                    local values = current.TextureParameterValues
                    if #values > 128 then error("Unexpected inherited texture count") end
                    for i = 1, #values do
                        local key = unwrap(values[i]).ParameterInfo.Name
                        if name(source:K2_GetTextureParameterValue(key)) ~= name(copy:K2_GetTextureParameterValue(key)) then
                            error("Inherited texture copy differs: " .. key:ToString())
                        end
                        inherited_textures = inherited_textures + 1
                    end
                    current = current.Parent
                end
                local key = unwrap(scalars[1]).ParameterInfo.Name
                local before = source:K2_GetScalarParameterValue(key)
                copy:SetScalarParameterValue(key, before + 1)
                if source:K2_GetScalarParameterValue(key) ~= before then error("Source was changed") end
                if math.abs(copy:K2_GetScalarParameterValue(key) - before - 1) > 0.00001 then error("Copy did not change") end
                if name(component:GetMaterial(slot)) ~= name(source) then error("Component material changed") end
                emit("copy.slot=" .. slot .. " source=" .. name(source) .. " private=" .. name(copy)
                    .. " scalars=" .. #scalars .. " textures=" .. #textures .. " vectors=" .. #vectors
                    .. " inherited_textures=" .. inherited_textures .. " copied_texture_overrides=" .. #copy.TextureParameterValues
                    .. " family=" .. family .. " source_unchanged=true")
                tested = tested + 1
            end
        end
    end
    if tested == 0 then error("No material overrides available to test") end
    emit("copy.tested=" .. tested)
    emit("ASTRAL_END")
end
