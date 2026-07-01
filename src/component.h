#pragma once

// data definitions
#include "data_definitions.h"

// platform manager
#include "physical_entity_manager.h"

// sen
#include "sen/kernel/component.h"

// spdlog
#include "spdlog/spdlog.h"


class DcsComponent : public sen::kernel::Component
{
public:
    DcsComponent(const sen::Duration& tickDuration, spdlog::logger* logger);
    ~DcsComponent() override = default;

    sen::kernel::FuncResult load(sen::kernel::LoadApi&&) override;
    sen::kernel::PassResult init(sen::kernel::InitApi&& api) override;
    sen::kernel::FuncResult run(sen::kernel::RunApi& api) override;
    sen::kernel::FuncResult unload(sen::kernel::UnloadApi&& api) override;

public:
    void newFrame(const FrameData& frame);

private:
    [[nodiscard]] static std::string sanitizeName(const std::string& name);

private:
    spdlog::logger* logger_;
    const sen::Duration& tickDuration_;

private:
    std::unordered_map<u64, std::shared_ptr<PhysicalEntityManager>> managers_;
    std::shared_ptr<sen::ObjectSource> source_;
};
