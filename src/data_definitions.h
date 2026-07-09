#pragma once

// sen
#include <sen/core/base/numbers.h>

// std
#include <string>
#include <vector>

// std_fom
#include "rpr/rpr-base_v2.0.xml.h"

// nlohmann
#include "nlohmann/json.hpp"

// sen utils
#include <sen/util/dr/algorithms.h>

struct SpatialData {
    f64 latitude;
    f64 longitude;
    f64 altitude;
    f64 yaw;
    f64 pitch;
    f64 roll;
    sen::util::Location location;
};

enum UnitType : u8 {
    AIRCRAFT = 1,
    GROUND_VEHICLE = 2,
    SURFACE_VESSEL = 3,
    MUNITION = 4,
    EXPENDABLE = 5,
    UNKNOWN = 6
};

struct PlatformData {
    UnitType type;
    std::string name;
    u64 id;
    SpatialData spatial;
};

[[nodiscard]] inline UnitType getUnitType(const nlohmann::json &json) {
    const auto level1 = json.value("level1", 1);
    const auto level2 = json.value("level2", 1);

    switch (level1) {
    case 1:
        if (level2 == 3) {
            return UnitType::EXPENDABLE;
        }
        return UnitType::AIRCRAFT;
    case 2:
        return UnitType::GROUND_VEHICLE;
    case 3:
        return UnitType::SURFACE_VESSEL;
    case 4:
        return UnitType::MUNITION;
    default:
        return UnitType::UNKNOWN;
    }
}

struct FrameData {
    u64 frameNumber;
    f64 time;
    std::vector<PlatformData> platforms;
};

using Mappings = std::unordered_map<std::string, rpr::EntityTypeStruct>;

inline void fillMappings(const nlohmann::json &json, Mappings &mappings) {
    for (const auto &[name, value] : json.items()) {
        rpr::EntityTypeStruct entity;
        entity.entityKind = value.value("kind", 0);
        entity.domain = value.value("domain", 0);
        entity.countryCode = value.value("country", 0);
        entity.category = value.value("category", 0);
        entity.subcategory = value.value("subcategory", 0);
        entity.specific = value.value("specific", 0);
        entity.extra = value.value("extra", 0);

        mappings.emplace(name, entity);
    }
}

using Recording = std::vector<FrameData>;

constexpr auto FRAME_KEY = "frame";
constexpr auto TIMESTAMP_KEY = "t";
constexpr auto UNITS_KEY = "units";
constexpr auto NAME_KEY = "name";
constexpr auto ID_KEY = "id";
constexpr auto LATITUDE_KEY = "lat";
constexpr auto LONGITUDE_KEY = "lon";
constexpr auto ALTITUDE_KEY = "alt";
constexpr auto YAW_KEY = "yaw";
constexpr auto PITCH_KEY = "pitch";
constexpr auto ROLL_KEY = "roll";
constexpr auto X_KEY = "x";
constexpr auto Y_KEY = "y";
constexpr auto Z_KEY = "z";

[[nodiscard]] inline PlatformData getPlatformData(const nlohmann::json &json) {
    PlatformData platformData;
    platformData.type = getUnitType(json);
    platformData.name = json[NAME_KEY];
    platformData.id = json[ID_KEY];
    platformData.spatial.latitude = json[LATITUDE_KEY];
    platformData.spatial.longitude = json[LONGITUDE_KEY];
    platformData.spatial.altitude = json[ALTITUDE_KEY];
    platformData.spatial.yaw = json[YAW_KEY];
    platformData.spatial.pitch = json[PITCH_KEY];
    platformData.spatial.roll = json[ROLL_KEY];
    platformData.spatial.location.x = json[X_KEY];
    platformData.spatial.location.y = json[Y_KEY];
    platformData.spatial.location.z = json[Z_KEY];

    return platformData;
}

[[nodiscard]] inline FrameData getFrame(const nlohmann::json &json) {
    FrameData data;

    data.frameNumber = json[FRAME_KEY];
    data.time = json[TIMESTAMP_KEY];
    data.platforms.reserve(json[UNITS_KEY].size());

    for (const auto &unit : json[UNITS_KEY]) {
        data.platforms.emplace_back(getPlatformData(unit));
    }

    return data;
}