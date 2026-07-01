// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/base_sink.h"

// component
#include "component.h"

// nlohmann
#include "nlohmann/json.hpp"

// cli11
#include "CLI/CLI.hpp"

// std
#include <fstream>
#include <filesystem>
#include <string>

// platform manager
#include "physical_entity_manager.h"

// sen
#include "sen/kernel/test_kernel.h"
#include "sen/kernel/bootloader.h"

// fxtui
#include <ftxui/ftxui.hpp>


struct UiState
{
    std::mutex mutex;
    std::deque<std::string> logs;
    f64 progress = 0.f;
    std::string status;

    void AddLog(std::string s)
    {
        std::lock_guard lock(mutex);

        logs.push_back(std::move(s));

        constexpr size_t MaxLogs = 1000;
        while (logs.size() > MaxLogs)
        {
            logs.pop_front();
        }
    }
};

class FtxuiSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    FtxuiSink(UiState& state,
              ftxui::ScreenInteractive& screen)
        : state_(state),
          screen_(screen)
    {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        state_.AddLog(fmt::to_string(formatted));
        screen_.PostEvent(ftxui::Event::Custom);
    }

    void flush_() override {}

private:
    UiState& state_;
    ftxui::ScreenInteractive& screen_;
};


int main(int argc, char** argv)
{
    CLI::App app{"DCS sen exporter"};

    std::filesystem::path inputFile;

    app.add_option(
        "input,-i,--input",
        inputFile,
        "Path to the DCS .ndjson file")
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    UiState uiState;
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto sink = std::make_shared<FtxuiSink>(uiState, screen);

    auto logger =
        std::make_shared<spdlog::logger>(
            "dcs",
            sink);

    spdlog::set_default_logger(logger);


    auto renderer = ftxui::Renderer([&] {

    std::vector<ftxui::Element> log_elements;

    {
        std::lock_guard lock(uiState.mutex);

        for (auto const& line : uiState.logs)
            log_elements.push_back(ftxui::text(line));
    }

    return ftxui::vbox({
        ftxui::text("DCS Exporter") | ftxui::bold,
        ftxui::separator(),
        ftxui::gauge(uiState.progress),
        ftxui::separator(),
        vbox(std::move(log_elements))
            | ftxui::frame
            | ftxui::vscroll_indicator
            | ftxui::border
            | ftxui::flex,
        });
    });

    std::thread worker([&] {
    logger->info("Opening {}", inputFile.string());
    std::ifstream file(inputFile);

    if (!file)
    {
        logger->error("Failed to open {}", inputFile.string());
        return 1;
    }

    std::string line;
    Recording recording;
    // Ideally try to reserve the memory given the number of lines in the file

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        FrameData data;

        auto frame = nlohmann::json::parse(line);

        data.frameNumber = frame["frame"];
        data.time = frame["t"];
        data.platforms.reserve(frame["units"].size());

        for (const auto& aircraft : frame["units"])
        {
            PlatformData platformData;
            platformData.type = aircraft["level1"];
            platformData.name = aircraft["name"];
            platformData.id = aircraft["id"];
            platformData.spatial.latitude = aircraft["lat"];
            platformData.spatial.longitude = aircraft["lon"];
            platformData.spatial.altitude = aircraft["alt"];
            data.platforms.push_back(std::move(platformData));
        }

        recording.push_back(std::move(data));
    }


    std::shared_ptr<DcsComponent> component_;
    std::unique_ptr<sen::kernel::TestKernel> kernel_;

    component_ = std::make_shared<DcsComponent>(std::chrono::milliseconds(1), logger.get());

    const auto bootLoader = sen::kernel::Bootloader::fromYamlString(R"(load:
  - name: recorder
    group: 2
    recordings:
      - name: DCS_recording
        folder: .
        indexKeyframes: true
        indexObjects: true
        keyframePeriod: 2 s
        autoStart: true
        selections:
          - SELECT * FROM dcs.mission  # record all objects in local.kernel)", false);

    sen::kernel::ComponentContext component;
    component.instance = component_.get();
    component.info.name = "DCS-sen-exporter";
    component.info.description = "DCS sen exporter";
    component.info.buildInfo = {};
    component.config.cpuAffinity = 0;
    component.config.group = 2;
    component.config.priority = sen::kernel::Priority::nominalMin;
    component.config.stackSize = 0;
    component.config.inQueue.evictionPolicy = sen::kernel::QueueEvictionPolicy::dropOldest;
    component.config.inQueue.maxSize = 0;
    component.config.outQueue.evictionPolicy = sen::kernel::QueueEvictionPolicy::dropOldest;
    component.config.outQueue.maxSize = 0;
    component.config.sleepPolicy = sen::kernel::SystemSleep{};

    sen::kernel::ComponentConfig config;
    config.cpuAffinity = 0;
    config.group = 2;
    config.priority = sen::kernel::Priority::nominalMin;
    config.stackSize = 0;
    config.inQueue.evictionPolicy = sen::kernel::QueueEvictionPolicy::dropOldest;
    config.inQueue.maxSize = 0;
    config.outQueue.evictionPolicy = sen::kernel::QueueEvictionPolicy::dropOldest;
    config.outQueue.maxSize = 0;
    config.sleepPolicy = sen::kernel::SystemSleep{};

    const sen::kernel::KernelConfig::ComponentToLoad componentConfig
    {
        std::move(component),
        config,
        {} // empty params
    };

    // kernel creation
    bootLoader->getConfig().addToLoad(componentConfig);
    kernel_ = std::make_unique<sen::kernel::TestKernel>(bootLoader->getConfig());

    f64 currentTime = 0;
    u64 recordingFrame = 0;
    const f64 finalTime = recording.back().time;
    bool recordingFinished = false;

    // Kernel loop
    while (!recordingFinished)
    {
        if (recording[recordingFrame].time <= currentTime)
        {
            component_->newFrame(recording[recordingFrame]);
            recordingFrame++;
        }

        constexpr f64 stepTime = 0.001;
        kernel_->step();
        currentTime += stepTime;

        if (finalTime <= currentTime)
        {
            recordingFinished = true;
        }

        uiState.progress =
    currentTime / finalTime;

    screen.PostEvent(ftxui::Event::Custom);
    }

    logger->info("Finished conversion");
        return 0;
    });

    screen.Loop(renderer);
    worker.join();

    return 0;
}
