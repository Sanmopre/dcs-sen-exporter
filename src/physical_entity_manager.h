#pragma once

// std_fom
#include "rpr/rpr-physical_v2.0.xml.h"

// nlohmann
#include "nlohmann/json.hpp"

// data_definitions
#include "data_definitions.h"

// sen_util
#include "sen/util/dr/settable_dead_reckoner.h"

class PhysicalEntityManager : public rpr::PhysicalEntityBase<> {
  public:
    PhysicalEntityManager(const std::string &name, const rpr::EntityTypeStruct &entityType,
                          const rpr::EntityIdentifierStruct &entityIdentifier,
                          const rpr::EntityTypeStruct &alternateEntityType);

    void updateSpatial(const SpatialData &data);

  private:
    sen::util::SettableDeadReckoner<rpr::PhysicalEntityBase<>> dr_;
};
