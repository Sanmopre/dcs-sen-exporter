#include "physical_entity_manager.h"

PhysicalEntityManager::PhysicalEntityManager(const std::string &name,
                                             const rpr::EntityTypeStruct &entityType,
                                             const rpr::EntityIdentifierStruct &entityIdentifier,
                                             const rpr::EntityTypeStruct &alternateEntityType)
    : PhysicalEntityBase<>(name, entityType, entityIdentifier, alternateEntityType),
      dr_(*this, {}) {}

void PhysicalEntityManager::updateSpatial(const SpatialData &data)
{
    sen::util::Velocity velocity;
    if (!previousLocation.has_value())
    {
        previousLocation = data.location;
    }
    else
    {
        // Adjusted for NED
         velocity = sen::util::Velocity{
            previousLocation.value().z - data.location.z,
            previousLocation.value().x - data.location.x,
            - (previousLocation.value().y - data.location.y)
        };
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
