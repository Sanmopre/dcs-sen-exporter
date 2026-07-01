#include "physical_entity_manager.h"

// std
constexpr f64 PI = 3.141592653589793238462643383279502884;

namespace {
    [[nodiscard]] f64 degToRad(const f64 deg) {
        return deg * (PI / 180.0);
    }
}

PhysicalEntityManager::PhysicalEntityManager(const std::string &name, const rpr::EntityTypeStruct &entityType,
const rpr::EntityIdentifierStruct &entityIdentifier, const rpr::EntityTypeStruct &alternateEntityType)
: PhysicalEntityBase<>(name, entityType, entityIdentifier, alternateEntityType)
, dr_(*this, {})
{
}

void PhysicalEntityManager::updateSpatial(const SpatialData &data)
{
    sen::util::GeodeticSituation situation;
    situation.worldLocation.latitude = data.latitude;
    situation.worldLocation.longitude = data.longitude;
    situation.worldLocation.altitude = data.altitude;
    situation.orientation.phi = data.roll;
    situation.orientation.theta = data.pitch;
    situation.orientation.psi = data.yaw;

    dr_.setSpatial(situation);
}
