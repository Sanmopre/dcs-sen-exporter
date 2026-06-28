local lfs = require("lfs")

local base = lfs.writedir() .. "/Logs/"
local out = nil
local probe = nil
local frame = 0

local function num(v)
    if v == nil then
        return 0
    end
    return v
end

local function jstr(s)
    s = tostring(s or "")
    s = s:gsub("\\", "\\\\")
         :gsub("\"", "\\\"")
         :gsub("\r", "\\r")
         :gsub("\n", "\\n")
    return "\"" .. s .. "\""
end

local function probe_write(msg)
    if probe then
        probe:write(msg .. "\n")
        probe:flush()
    end
end

function LuaExportStart()
    probe = io.open(base .. "export_probe.txt", "w")
    if probe then
        probe_write("LuaExportStart")
        probe_write("writedir=" .. lfs.writedir())
        if LoIsObjectExportAllowed then
            probe_write("object_export_allowed=" .. tostring(LoIsObjectExportAllowed()))
        end
    end

    out = io.open(base .. "world_frames.ndjson", "w")
    if out then
        out:flush()
        probe_write("opened_output=" .. base .. "world_frames.ndjson")
    else
        probe_write("FAILED_TO_OPEN world_frames.ndjson")
    end
end

function LuaExportAfterNextFrame()
    frame = frame + 1

    local t = 0
    if LoGetModelTime then
        t = num(LoGetModelTime())
    end

    if probe and (frame == 1 or frame % 120 == 0) then
        probe_write(string.format("frame=%d t=%.6f", frame, t))
    end

    if not out then
        return
    end

    if LoIsObjectExportAllowed and not LoIsObjectExportAllowed() then
        return
    end

    local objects = nil
    if LoGetWorldObjects then
        objects = LoGetWorldObjects("units")
    end

    if not objects then
        return
    end

    out:write(string.format("{\"frame\":%d,\"t\":%.6f,\"aircraft\":[", frame, t))

    local first = true

    for id, obj in pairs(objects) do
        if obj and obj.Type and obj.LatLongAlt and obj.Type.level1 == 1 then
            if not first then
                out:write(",")
            end
            first = false

            out:write(string.format(
                "{\"id\":%d,\"name\":%s,\"lat\":%.8f,\"lon\":%.8f,\"alt\":%.2f,\"yaw\":%.6f,\"pitch\":%.6f,\"roll\":%.6f}",
                id,
                jstr(obj.Name),
                num(obj.LatLongAlt.Lat),
                num(obj.LatLongAlt.Long),
                num(obj.LatLongAlt.Alt),
                num(obj.Heading),
                num(obj.Pitch),
                num(obj.Bank)
            ))
        end
    end

    out:write("]}\n")
    out:flush()
end

function LuaExportStop()
    if out then
        out:close()
        out = nil
    end

    if probe then
        probe_write("LuaExportStop")
        probe:close()
        probe = nil
    end
end