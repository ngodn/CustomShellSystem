local values = {enabled = false, quality = "balanced", strength = 50,
    count = 4, theme = "gold", name = "My template", progress = 0, loading = false}
values.catalog = "item-001"
local running, elapsed, refresh = false, 0, 0
local status = "UI examples only. This extension does not change the game."
local function request(value)
    local result, err = cssx.request(value)
    if err then error(err) end
    return result
end
return {
    model = function()
        return {values = values, busy = {run = running}, status = status}
    end,
    event = function(event)
        if event.id == "run" then
            elapsed, refresh, running = 0, 0, true
            values.progress, values.loading = 0, true
            status = "Example operation running. You can keep browsing."
        elseif event.id == "confirm" then
            status = "Confirmed. No game data was changed."
        elseif values[event.id] ~= nil then
            values[event.id] = event.value
            status = "Updated " .. event.id .. "."
        end
    end,
    tick = function(delta)
        if not running then return end
        elapsed, refresh = elapsed + delta, refresh + delta
        values.progress = math.min(1, elapsed / 5)
        if elapsed >= 5 then
            running, values.loading = false, false
            status = "Example operation complete."
        end
        if refresh >= 0.25 or not running then
            refresh = 0
            request {op = "invalidate"}
        end
    end
}
