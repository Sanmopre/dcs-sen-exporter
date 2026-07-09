#include "physical_entity_manager.h"

PhysicalEntityManager::PhysicalEntityManager(const std::string &name,
                                             const rpr::EntityTypeStruct &entityType,
                                             const rpr::EntityIdentifierStruct &entityIdentifier,
                                             const rpr::EntityTypeStruct &alternateEntityType)
    : PhysicalEntityBase<>(name, entityType, entityIdentifier, alternateEntityType),
      dr_(*this, {}) {}

void PhysicalEntityManager::updateSpatial(const SpatialData &data, f64 timeStamp) {
    sen::util::Velocity velocity;
    if (!previousLocation.has_value()) {
        previousLocation = data.location;
        previousTimeStampSeconds = timeStamp;
    } else {

        if (const f64 dt = timeStamp - previousTimeStampSeconds; dt > 0.0) {
            // Convert displacement to meters/second (NED coordinates)
            velocity = sen::util::Velocity{(previousLocation->z - data.location.z) / dt,
                                           (previousLocation->x - data.location.x) / dt,
                                           -(previousLocation->y - data.location.y) / dt};
        }

        previousLocation = data.location;
        previousTimeStampSeconds = timeStamp;
    }

    sen::util::GeodeticSituation situation;
    situation.worldLocation.latitude = data.latitude;
    situation.worldLocation.longitude = data.longitude;
    situation.worldLocation.altitude = data.altitude;
    situation.orientation.phi = data.roll;
    situation.orientation.theta = data.pitch;
    situation.orientation.psi = data.yaw;
    situation.velocityVector = velocity;

    dr_.setSpatial(situation);
}
