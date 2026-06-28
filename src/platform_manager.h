#pragma once

// std_fom
#include "rpr/rpr-physical_v2.0.xml.h"

// nlohmann
#include "nlohmann/json.hpp"

// data_definitions
#include "data_definitions.h"

class PlatformManager : public rpr::PlatformBase<>
{
public:
    PlatformManager(const std::string &name, const rpr::EntityTypeStruct &entityType,
        const rpr::EntityIdentifierStruct &entityIdentifier, const rpr::EntityTypeStruct &alternateEntityType);

    void updateSpatial(const SpatialData& data);

};


