local function request(value)
    local result, err = cssx.request(value)
    if err then error(err) end
    return result
end
local state = request {op = "state.load"}
state.counter = math.max(0, math.min(100, tonumber(state.counter) or 0))
return {
    model = function()
        return {values = {counter = state.counter}, status = "Settings persist across game sessions."}
    end,
    event = function(event)
        if event.id == "counter" then
            state.counter = math.max(0, math.min(100, tonumber(event.value) or 0))
            request {op = "state.save", value = state}
            request {op = "log", level = "info", message = "Counter changed", fields = state}
        elseif event.id == "export" then
            request {op = "output.write", file = "counter.txt", text = tostring(state.counter) .. "\n"}
        end
    end
}
