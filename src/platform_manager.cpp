#include "platform_manager.h"

// std
constexpr f64 PI = 3.141592653589793238462643383279502884;

namespace {
    [[nodiscard]] f64 degToRad(const f64 deg) {
        return deg * (PI / 180.0);
    }
}

PlatformManager::PlatformManager(const std::string &name, const rpr::EntityTypeStruct &entityType,
const rpr::EntityIdentifierStruct &entityIdentifier, const rpr::EntityTypeStruct &alternateEntityType)
: PlatformBase<>(name, entityType, entityIdentifier, alternateEntityType)
, dr_(*this, {})
{
}

void PlatformManager::updateSpatial(const SpatialData &data)
{
    sen::util::GeodeticSituation situation;
    situation.worldLocation.latitude = data.latitude;
    situation.worldLocation.longitude = data.longitude;
    situation.worldLocation.altitude = data.altitude;

    // TODO: Check values
    situation.orientation.phi = degToRad(data.pitch);
    situation.orientation.theta = degToRad(data.roll);
    situation.orientation.psi = degToRad(data.yaw);

    dr_.setSpatial(situation);
}
