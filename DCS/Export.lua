local lfs = require("lfs")

local base = lfs.writedir() .. "/Logs/"
local out = nil
local probe = nil
local frame = 0

local FRAME_KEY      = "frame"
local TIMESTAMP_KEY  = "t"
local UNITS_KEY      = "units"
local ID_KEY         = "id"
local NAME_KEY       = "name"
local TYPE_KEY       = "type"
local LEVEL1_KEY     = "level1"
local LEVEL2_KEY     = "level2"
local COUNTRY_KEY    = "country"
local COALITION_KEY  = "coalition"
local LATITUDE_KEY   = "lat"
local LONGITUDE_KEY  = "lon"
local ALTITUDE_KEY   = "alt"
local X_KEY          = "x"
local Y_KEY          = "y"
local Z_KEY          = "z"
local YAW_KEY        = "yaw"
local PITCH_KEY      = "pitch"
local ROLL_KEY       = "roll"

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
        "{\"%s\":%d,\"%s\":%.6f,\"%s\":[",
        FRAME_KEY,
        frame,
        TIMESTAMP_KEY,
        t,
        UNITS_KEY
    ))

    local first = true

    for id, obj in pairs(objects) do
        if obj and obj.Type and obj.LatLongAlt and obj.Position then

            if not first then
                out:write(",")
            end
            first = false

            out:write(string.format(
                "{\"%s\":%d," ..
                "\"%s\":%s," ..
                "\"%s\":%d," ..
                "\"%s\":%d," ..
                "\"%s\":%d," ..
                "\"%s\":%d," ..
                "\"%s\":%.8f," ..
                "\"%s\":%.8f," ..
                "\"%s\":%.2f," ..
                "\"%s\":%.3f," ..
                "\"%s\":%.3f," ..
                "\"%s\":%.3f," ..
                "\"%s\":%.6f," ..
                "\"%s\":%.6f," ..
                "\"%s\":%.6f}",

                ID_KEY, id,

                NAME_KEY, jstr(obj.Name),

                LEVEL1_KEY, num(obj.Type.level1),
                LEVEL2_KEY, num(obj.Type.level2),

                COUNTRY_KEY, num(obj.Country),
                COALITION_KEY, num(obj.CoalitionID),

                LATITUDE_KEY, num(obj.LatLongAlt.Lat),
                LONGITUDE_KEY, num(obj.LatLongAlt.Long),
                ALTITUDE_KEY, num(obj.LatLongAlt.Alt),

                X_KEY, num(obj.Position.x),
                Y_KEY, num(obj.Position.y),
                Z_KEY, num(obj.Position.z),

                YAW_KEY, num(obj.Heading),
                PITCH_KEY, num(obj.Pitch),
                ROLL_KEY, num(obj.Bank)
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