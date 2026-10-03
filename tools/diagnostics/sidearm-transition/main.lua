-- UE4SS Lua 5.4. On-demand read-only capture, excluded from releases.
local function valid(o) return o ~= nil and o:IsValid() end
local function name(o) return valid(o) and o:GetFullName() or "none" end
local function vector(v) return string.format("%.4f,%.4f,%.4f", v.X, v.Y, v.Z) end
local function rotation(r) return string.format("%.4f,%.4f,%.4f", r.Pitch, r.Yaw, r.Roll) end
return function(pawn, emit)
    local mesh = pawn.Mesh
    if not valid(mesh) then return nil, "No character mesh" end
    local rows = {}
    local function read(label, fn)
        local ok, value = pcall(fn)
        rows[#rows + 1] = label .. "=" .. (ok and tostring(value) or "ERROR: " .. tostring(value))
    end
    read("controlRotation", function() return rotation(pawn.Controller:GetControlRotation()) end)
    read("pawn", function() return name(pawn) end)
    read("mesh", function() return name(mesh:GetSkeletalMeshAsset()) end)
    read("skeleton", function() return name(mesh:GetSkeletalMeshAsset().Skeleton) end)
    read("meshAnim", function() return name(mesh:GetAnimInstance()) end)
    read("pawnAnim", function() return name(pawn:GetAnimInstance()) end)
    read("aimNode", function()
        local anim = mesh:GetAnimInstance()
        local node = anim.AnimGraphNode_LinkedAnimLayer_4
        return "class=" .. name(node.InstanceClass) .. " target=" .. name(node.TargetInstance)
    end)
    read("defaultAim", function()
        local class = StaticFindObject("/Game/Sparta/Core/Animations/Layers/ABPL_Aim_Default.ABPL_Aim_Default_C")
        return valid(class) and name(mesh:GetLinkedAnimLayerInstanceByClass(class)) or "class unavailable"
    end)
    read("postProcess", function() return name(mesh:GetPostProcessInstance()) end)
    read("montage", function()
        local anim = mesh:GetAnimInstance()
        return valid(anim) and name(anim:GetCurrentActiveMontage()) or "none"
    end)
    for _, bone in ipairs({"hand_r", "hand_l", "ik_hand_gun", "ik_hand_r", "ik_hand_l", "prop_r", "prop_l", "Socket_Prop_R_MachineGun"}) do
        read("bone." .. bone, function()
            local key = FName(bone)
            if not mesh:DoesSocketExist(key) then return "missing" end
            return vector(mesh:GetSocketLocation(key)) .. " rot=" .. rotation(mesh:GetSocketRotation(key))
        end)
    end
    local weapons = pawn.WeaponsComponent
    if valid(weapons) then
        local weapon = weapons:GetWeaponInSlot({TagName = FName("Weapon.Slot.Sidearm")})
        read("sidearm", function() return name(weapon) .. " class=" .. name(weapon:GetClass()) end)
        if valid(weapon) then
            local root = weapon:K2_GetRootComponent()
            read("attachment", function()
                if not valid(root) then return "none" end
                return name(root:GetAttachParent()) .. " socket=" .. root:GetAttachSocketName():ToString()
                    .. " relativeRot=" .. rotation(root.RelativeRotation)
                    .. " relativePos=" .. vector(root.RelativeLocation)
                    .. " worldRot=" .. rotation(root:K2_GetComponentRotation())
            end)
            read("weaponPose", function()
                local wm = weapon.WeaponMesh
                if not valid(wm) then return "none" end
                local result = {name(wm:GetSkeletalMeshAsset()), "anim=" .. name(wm:GetAnimInstance()),
                    "relativeRot=" .. rotation(wm.RelativeRotation)}
                for i = 0, math.min(wm:GetNumBones(), 32) - 1 do
                    local bone = wm:GetBoneName(i)
                    result[#result + 1] = bone:ToString() .. ":" .. rotation(wm:GetSocketRotation(bone))
                end
                return table.concat(result, " | ")
            end)
            read("aimLayers", function()
                local result = {}
                local layers = weapon:GetSidearmLocomotionAnimLayers()
                for i = 1, math.min(#layers, 16) do
                    local class = layers[i]
                    if class:type() == "RemoteUnrealParam" then class = class:get() end
                    result[#result + 1] = name(class) .. " -> " .. name(mesh:GetLinkedAnimLayerInstanceByClass(class))
                end
                return table.concat(result, " | ")
            end)
        end
    end
    emit(table.concat(rows, "\n"))
end

