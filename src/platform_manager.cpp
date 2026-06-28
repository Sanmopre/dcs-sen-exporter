#include "platform_manager.h"

PlatformManager::PlatformManager(const std::string &name, const rpr::EntityTypeStruct &entityType,
const rpr::EntityIdentifierStruct &entityIdentifier, const rpr::EntityTypeStruct &alternateEntityType)
: PlatformBase<>(name, entityType, entityIdentifier, alternateEntityType)
{
}
