#pragma once

// sen
#include <sen/core/base/numbers.h>

// std
#include <string>
#include<vector>

// std_fom
#include "rpr/rpr-base_v2.0.xml.h"

// nlohmann
#include "nlohmann/json.hpp"

struct SpatialData
{
    f64 latitude;
    f64 longitude;
    f64 altitude;
    f64 yaw;
    f64 pitch;
    f64 roll;
};

enum PlatformType : u8
{
    AIRCRAFT = 1,
    GROUND_VEHICLE = 2,
    SURFACE_VESSEL = 3,
    MUNITION = 4
};

struct PlatformData
{
    PlatformType type;
    std::string name;
    u64 id;
    SpatialData spatial;
};

struct FrameData
{
    u64 frameNumber;
    f64 time;
    std::vector<PlatformData> platforms;
};

using Mappings = std::unordered_map<std::string, rpr::EntityTypeStruct>;

inline void fillMappings(
    const nlohmann::json& json,
    Mappings& mappings)
{
    for (const auto& [name, value] : json.items())
    {
        rpr::EntityTypeStruct entity;
        entity.entityKind  = value.value("kind", 0);
        entity.domain      = value.value("domain", 0);
        entity.countryCode = value.value("country", 0);
        entity.category    = value.value("category", 0);
        entity.subcategory = value.value("subcategory", 0);
        entity.specific    = value.value("specific", 0);
        entity.extra       = value.value("extra", 0);

        mappings.emplace(name, entity);
    }
}

using Recording = std::vector<FrameData>;