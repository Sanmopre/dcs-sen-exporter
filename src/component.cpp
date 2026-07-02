#include "component.h"

DcsComponent::DcsComponent(const sen::Duration &tickDuration, const std::string& publishingBus, const Mappings& mappings, spdlog::logger* logger)
    : logger_(logger), tickDuration_(tickDuration), publishingBus_(publishingBus), mappings_(mappings)
{
}

sen::kernel::FuncResult DcsComponent::load(sen::kernel::LoadApi &&load_api)
{
    return Component::load(std::move(load_api));
}

sen::kernel::PassResult DcsComponent::init(sen::kernel::InitApi &&api)
{
    source_ = api.getSource(publishingBus_);
    return Component::init(std::move(api));
}

sen::kernel::FuncResult DcsComponent::run(sen::kernel::RunApi &api)
{
    return api.execLoop(tickDuration_, [this, &api]() {});
}

sen::kernel::FuncResult DcsComponent::unload(sen::kernel::UnloadApi &&api)
{
    return Component::unload(std::move(api));
}

void DcsComponent::newFrame(const FrameData &frame)
{
    std::vector<u64> seenEntities;
    seenEntities.reserve(frame.platforms.size());

    for (const auto& platform : frame.platforms)
    {
        // Add the entity to the seen entities this frame
        seenEntities.emplace_back(platform.id);

        if (const auto it = managers_.find(platform.id); it != managers_.end())
        {
            // Update entity
            managers_.at(platform.id)->updateSpatial(platform.spatial);
        }
        else
        {
            // Create entity
            const auto name = std::string("dcs_").append(sanitizeName(platform.name)).append("_").append(std::to_string(platform.id));
            const auto entityType = getEntityType(platform.name);

            switch (platform.type)
            {
                case PlatformType::AIRCRAFT:
                    managers_.try_emplace(platform.id,std::make_shared<rpr::AircraftBase<rpr::PlatformBase<PhysicalEntityManager>>>(name, entityType, rpr::EntityIdentifierStruct{}, entityType));
                    break;
                case PlatformType::GROUND_VEHICLE:
                   managers_.try_emplace(platform.id,std::make_shared<rpr::GroundVehicleBase<rpr::PlatformBase<PhysicalEntityManager>>>(name, entityType, rpr::EntityIdentifierStruct{}, entityType));
                    break;
                case PlatformType::SURFACE_VESSEL:
                    managers_.try_emplace(platform.id,std::make_shared<rpr::SurfaceVesselBase<rpr::PlatformBase<PhysicalEntityManager>>>(name, entityType, rpr::EntityIdentifierStruct{}, entityType));
                    break;
                case PlatformType::MUNITION:
                    managers_.try_emplace(platform.id,std::make_shared<rpr::MunitionBase<PhysicalEntityManager>>(name, entityType, rpr::EntityIdentifierStruct{}, entityType));
                    break;
                default:
                    managers_.try_emplace(platform.id,std::make_shared<PhysicalEntityManager>(name, entityType, rpr::EntityIdentifierStruct{}, entityType));
            }

            managers_.at(platform.id)->updateSpatial(platform.spatial);
            source_->add(managers_.at(platform.id));
        }
    }

    // Delete entities that
    std::vector<u64> toDelete;
    toDelete.reserve(managers_.size());
    for (const auto& [key, manager] : managers_)
    {
        if (const auto it = std::find(seenEntities.begin(), seenEntities.end(), key); it== seenEntities.end())
        {
            toDelete.emplace_back(key);
        }
    }

    for (const auto& key : toDelete)
    {
        managers_.erase(key);
    }
}

std::string DcsComponent::sanitizeName(const std::string& name)
{
    std::string result;
    result.reserve(name.size());

    for (unsigned char c : name)
    {
        if (std::isalnum(c) || c == '-' || c == '_')
        {
            result += static_cast<char>(c);
        }
        else if (std::isspace(c))
        {
            result += '_';
        }
    }

    return result;
}

rpr::EntityTypeStruct DcsComponent::getEntityType(const std::string &name) const noexcept
{
    if (const auto it = mappings_.find(name); it != mappings_.end()) {
        return it->second;
    }

    logger_->warn("Failed to find mapping entry for {}", name);
    return rpr::EntityTypeStruct{};
}
