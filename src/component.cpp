#include "component.h"

DcsComponent::DcsComponent(const sen::Duration &tickDuration, spdlog::logger* logger)
    : logger_(logger), tickDuration_(tickDuration)
{
}

sen::kernel::FuncResult DcsComponent::load(sen::kernel::LoadApi &&load_api) {
    return Component::load(std::move(load_api));
}

sen::kernel::PassResult DcsComponent::init(sen::kernel::InitApi &&api) {
    return Component::init(std::move(api));
}

sen::kernel::FuncResult DcsComponent::run(sen::kernel::RunApi &api)
{
    return api.execLoop(tickDuration_, [this, &api]() {
        logger_->info("Running Dcs component");
    });
}

sen::kernel::FuncResult DcsComponent::unload(sen::kernel::UnloadApi &&api) {
    return Component::unload(std::move(api));
}

void DcsComponent::newFrame(const FrameData &frame)
{
    logger_->info("New frame: {}", frame.time);
}
