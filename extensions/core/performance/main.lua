-- CSSX Performance: live engine settings for A/B testing.
-- Every control maps to a console variable. Changes apply at once; the values
-- the game had when the first change was made are kept for Restore.
local function request(value)
    local result, err = cssx.request(value)
    if err then error(err) end
    return result
end
local function try(value)
    local result, err = cssx.request(value)
    if err then return nil, err end
    return result
end

local GROUPS = {"shadow", "gi", "reflection", "view", "post", "effects", "foliage", "shading", "texture", "aa"}
local CVAR = {
    shadow = "sg.ShadowQuality", gi = "sg.GlobalIlluminationQuality", reflection = "sg.ReflectionQuality",
    view = "sg.ViewDistanceQuality", post = "sg.PostProcessQuality", effects = "sg.EffectsQuality",
    foliage = "sg.FoliageQuality", shading = "sg.ShadingQuality", texture = "sg.TextureQuality", aa = "sg.AntiAliasingQuality",
    screen_percentage = "r.ScreenPercentage", motion_blur = "r.MotionBlurQuality", depth_of_field = "r.DepthOfFieldQuality",
    volumetric_fog = "r.VolumetricFog", fps_cap = "t.MaxFPS",
}
local LUMEN_ASYNC = {"r.LumenScene.Lighting.AsyncCompute", "r.Lumen.DiffuseIndirect.AsyncCompute", "r.Lumen.Reflections.AsyncCompute"}

local state = request {op = "state.load"}
if type(state.remember) ~= "boolean" then state.remember = false end
if type(state.values) ~= "table" then state.values = {} end

local library = nil            -- KismetSystemLibrary default object handle
local current = {}             -- cvar name -> number, as last read
local original = {}            -- cvar name -> number, captured before the first change
local last = "Change a setting; it applies at once."
local reapplied = false
local read_time = 0

local function kismet()
    if library == nil then library = request {op = "find", path = "/Script/Engine.Default__KismetSystemLibrary"} end
    return library
end
local function read_int(name)
    local r = try {op = "call", target = kismet(), ["function"] = "GetConsoleVariableIntValue", args = {VariableName = name}}
    if r and type(r.ReturnValue) == "number" then return r.ReturnValue end
    return nil
end
local function read_float(name)
    local r = try {op = "call", target = kismet(), ["function"] = "GetConsoleVariableFloatValue", args = {VariableName = name}}
    if r and type(r.ReturnValue) == "number" then return r.ReturnValue end
    return nil
end
local function read_all()
    for _, id in ipairs(GROUPS) do current[CVAR[id]] = read_int(CVAR[id]) end
    current[CVAR.screen_percentage] = read_float(CVAR.screen_percentage)
    current[CVAR.motion_blur] = read_int(CVAR.motion_blur)
    current[CVAR.depth_of_field] = read_int(CVAR.depth_of_field)
    current[CVAR.volumetric_fog] = read_int(CVAR.volumetric_fog)
    current[CVAR.fps_cap] = read_float(CVAR.fps_cap)
    for _, name in ipairs(LUMEN_ASYNC) do current[name] = read_int(name) end
end
local function player()
    local p = try {op = "player"}
    if p and type(p.pawn) == "table" and type(p.controller) == "table" then return p end
    return nil
end
local function exec(command)
    local p = player()
    if not p then error("Enter the world first.") end
    request {op = "call", target = kismet(), ["function"] = "ExecuteConsoleCommand",
             args = {WorldContextObject = p.pawn, Command = command, SpecificPlayer = p.controller}}
end
local function set_cvar(name, value)
    if original[name] == nil and current[name] ~= nil then original[name] = current[name] end
    exec(name .. " " .. tostring(value))
    current[name] = value
    if state.remember then state.values[name] = value else state.values = {} end
    request {op = "state.save", value = state}
end
local function fmt(x) if x == nil then return "?" end return string.format("%.1f", x) end
local function frame_line()
    local f = try {op = "frame.brief"}
    if not f or (f.frames or 0) < 10 then return "Measuring frame rate..." end
    return string.format("%.0f fps, %.1f ms per frame, CSSX %.2f ms", f.hz or 0, f.median_ms or 0, (f.core_mean_us or 0) / 1000)
end

return {
    model = function()
        local live = player() ~= nil
        if live and read_time == 0 then read_all(); read_time = 1 end
        local values, enabled, disabled = {}, {}, {}
        for _, id in ipairs(GROUPS) do
            local v = current[CVAR[id]]
            values[id] = v ~= nil and tostring(math.max(0, math.min(4, v))) or "2"
        end
        local sp = current[CVAR.screen_percentage] or 0
        if sp <= 0 then sp = 100 end
        values.screen_percentage = math.max(50, math.min(100, math.floor(sp / 5 + 0.5) * 5))
        values.motion_blur = (current[CVAR.motion_blur] or 0) > 0
        values.depth_of_field = (current[CVAR.depth_of_field] or 0) > 0
        values.volumetric_fog = (current[CVAR.volumetric_fog] or 0) ~= 0
        values.lumen_async = (current[LUMEN_ASYNC[1]] or 0) ~= 0
        values.fps_cap = math.max(0, math.min(240, math.floor(current[CVAR.fps_cap] or 0)))
        values.remember = state.remember
        local why = live and "" or "Enter the world first"
        for _, id in ipairs({"screen_percentage", "motion_blur", "depth_of_field", "volumetric_fog", "lumen_async", "fps_cap", "refresh"}) do
            enabled[id] = live; if not live then disabled[id] = why end
        end
        for _, id in ipairs(GROUPS) do enabled[id] = live; if not live then disabled[id] = why end end
        local changed = next(original) ~= nil
        enabled.restore = live and changed
        if not enabled.restore then disabled.restore = live and "Nothing changed yet" or why end
        local status = frame_line() .. "  |  " .. last
        return {values = values, enabled = enabled, disabled = disabled, status = status}
    end,
    event = function(event)
        local id = event.id
        if id == "refresh" then read_all(); last = "Values re-read from the engine."; return end
        if id == "restore" then
            for name, value in pairs(original) do exec(name .. " " .. tostring(value)); current[name] = value end
            original = {}; state.values = {}; request {op = "state.save", value = state}
            last = "Game values restored."; return
        end
        if id == "remember" then
            state.remember = event.value and true or false
            if not state.remember then state.values = {} end
            request {op = "state.save", value = state}
            last = state.remember and "Changes will be re-applied at launch." or "Changes stay for this session only."
            return
        end
        if CVAR[id] and event.value ~= nil then
            if id == "motion_blur" or id == "depth_of_field" then set_cvar(CVAR[id], event.value and 4 or 0)
            elseif id == "volumetric_fog" then set_cvar(CVAR[id], event.value and 1 or 0)
            elseif id == "fps_cap" or id == "screen_percentage" then set_cvar(CVAR[id], math.floor(tonumber(event.value) or 0))
            else set_cvar(CVAR[id], tonumber(event.value) or 2) end
            last = CVAR[id] .. " = " .. tostring(current[CVAR[id]])
            return
        end
        if id == "lumen_async" then
            for _, name in ipairs(LUMEN_ASYNC) do set_cvar(name, event.value and 1 or 0) end
            last = "Lumen async compute " .. (event.value and "on" or "off"); return
        end
        error("Unknown control: " .. tostring(id))
    end,
    tick = function()
        -- Re-apply remembered values once the world exists.
        if reapplied or not state.remember or next(state.values) == nil then return end
        if not player() then return end
        for name, value in pairs(state.values) do
            local ok = try {op = "call", target = kismet(), ["function"] = "ExecuteConsoleCommand",
                            args = {WorldContextObject = player().pawn, Command = name .. " " .. tostring(value), SpecificPlayer = player().controller}}
            if ok ~= nil then current[name] = value end
        end
        reapplied = true; read_time = 0
        request {op = "log", level = "info", message = "Remembered performance settings re-applied", fields = state.values}
        request {op = "invalidate"}
    end,
    status = function()
        local n = 0; for _ in pairs(original) do n = n + 1 end
        return {summary = n > 0 and (n .. " setting" .. (n == 1 and "" or "s") .. " changed") or "Game defaults", active = n > 0}
    end,
}
