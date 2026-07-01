local lfs = require("lfs")

local base = lfs.writedir() .. "/Logs/"
local out = nil
local probe = nil
local frame = 0

local function sanitize_filename(s)
    s = tostring(s or "unknown_mission")
    s = s:gsub("\\", "/")
    s = s:match("([^/]+)$") or s
    s = s:gsub("%.miz$", "")
    s = s:gsub('[<>:"/\\|?*]', "_")
    s = s:gsub("%s+", "_")
    return s
end

local function make_recording_name()
    local mission = "unknown_mission"

    if LoGetMissionFilename then
        mission = LoGetMissionFilename() or mission
    end

    local stamp = os.date("%Y-%m-%d_%H-%M-%S")

    return sanitize_filename(mission) .. "_" .. stamp .. ".ndjson"
end

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
    local filename = make_recording_name()
    local fullpath = base .. filename

    out = io.open(fullpath, "w")

    if out then
        out:flush()
        probe_write("opened_output=" .. fullpath)
    else
        probe_write("FAILED_TO_OPEN " .. fullpath)
    end
end

function LuaExportAfterNextFrame()
    frame = frame + 1

    local t = 0
    if LoGetModelTime then
        t = num(LoGetModelTime())
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

    out:write(string.format(
        "{\"frame\":%d,\"t\":%.6f,\"units\":[",
        frame,
        t))

    local first = true

    for id, obj in pairs(objects) do
        if obj and obj.Type and obj.LatLongAlt then

            if not first then
                out:write(",")
            end
            first = false

            out:write(string.format(
                "{\"id\":%d," ..
                "\"name\":%s," ..
                "\"level1\":%d," ..
                "\"level2\":%d," ..
                "\"country\":%d," ..
                "\"coalition\":%d," ..
                "\"lat\":%.8f," ..
                "\"lon\":%.8f," ..
                "\"alt\":%.2f," ..
                "\"yaw\":%.6f," ..
                "\"pitch\":%.6f," ..
                "\"roll\":%.6f}",

                id,

                jstr(obj.Name),

                num(obj.Type.level1),
                num(obj.Type.level2),

                num(obj.Country),
                num(obj.CoalitionID),

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