#pragma once

// component
#include "component.h"

// spdlog
#include "spdlog/logger.h"

// ftxui
#include <ftxui/component/event.hpp>

// sen
#include <sen/kernel/bootloader.h>
#include <sen/kernel/test_kernel.h>

// std
#include <fstream>

struct LogEntry {
    std::string timestamp;
    std::string loggerName;
    spdlog::level::level_enum level;
    std::string message;
};

struct UiState {
    std::mutex mutex;
    std::deque<LogEntry> logs;
    f64 convertingProgress = 0.0;
    f64 readingProgress = 0.0;
    std::string status;

    void AddLog(LogEntry entry) {
        std::lock_guard lock(mutex);

        logs.push_back(std::move(entry));

        constexpr size_t MaxLogs = 1000;
        while (logs.size() > MaxLogs) {
            logs.pop_front();
        }
    }
};

[[nodiscard]] inline int processRecording(spdlog::logger* logger, const std::filesystem::path& inputFile, UiState& uiState, ftxui::ScreenInteractive &screen)
{
    // Start timer
    auto start = std::chrono::steady_clock::now();

    // Opening input file
    logger->info("Opening input file {}", inputFile.string());
    std::ifstream inputFileStream(inputFile);
    if (!inputFileStream) {
        logger->error("Failed to open input {}", inputFile.string());
        return 1;
    }
    const auto inputJsonFile = nlohmann::json::parse(inputFileStream);

    std::filesystem::path recordingFilePath;

    // Check if the recording key exists
    if (const auto it = inputJsonFile.find("recording"); it != inputJsonFile.end()) {
        recordingFilePath = inputJsonFile.at("recording").get<std::string>();
    } else {
        logger->error("Failed to find recording entry in {}", inputFile.string());
        return 1;
    }

    // Opening recording file
    logger->info("Opening recording {}", recordingFilePath.string());
    std::ifstream recordingFileStream(recordingFilePath);
    if (!recordingFileStream) {
        logger->error("Failed to open recording {}", recordingFilePath.string());
        return 1;
    }

    // Get the number of lines in the file
    std::string line;

    u64 lineCount = 0;
    while (std::getline(recordingFileStream, line)) {
        if (!line.empty())
            ++lineCount;
    }

    // Reset file
    recordingFileStream.clear();
    recordingFileStream.seekg(0);

    logger->info("Reading recording with {} entries", lineCount);

    Recording recording;
    recording.reserve(lineCount);

    u64 recordingCount = 0;
    while (std::getline(recordingFileStream, line)) {
        if (line.empty())
            continue;

        FrameData data;

        auto frame = nlohmann::json::parse(line);

        data.frameNumber = frame["frame"];
        data.time = frame["t"];
        data.platforms.reserve(frame["units"].size());

        for (const auto &aircraft : frame["units"]) {
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
        recordingCount++;
        {
            std::lock_guard lock(uiState.mutex);
            uiState.readingProgress =
                static_cast<f64>(recordingCount) / static_cast<f64>(lineCount);
        }
        screen.PostEvent(ftxui::Event::Custom);
    }

    logger->info("Finished reading the content of the recording");
    logger->info("-------------------------------------------------------------");
    logger->info("Number of frames            : {} frames", recording.size());
    logger->info("Recording duration of frames: {} seconds", recording.back().time);
    logger->info("-------------------------------------------------------------");

    std::shared_ptr<DcsComponent> component_;
    std::unique_ptr<sen::kernel::TestKernel> kernel_;

    // Check if mappings exist
    if (const auto it = inputJsonFile.find("mappings"); it == inputJsonFile.end()) {
        logger->warn("Failed to find mappings entry in {}", inputFile.string());
    }

    // Get mappings
    Mappings mappings;
    fillMappings(inputJsonFile["mappings"], mappings);

    component_ = std::make_shared<DcsComponent>(std::chrono::milliseconds(1), inputJsonFile.value("bus", "dcs.mission"),
                                                mappings, logger);

    const std::string yaml =
        fmt::format(R"(load:
  - name: recorder
    group: 1
    recordings:
      - name: '{}'
        folder: '{}'
        indexKeyframes: true
        indexObjects: true
        keyframePeriod: 2 s
        autoStart: true
        selections:
          - SELECT * FROM {})",
                    inputJsonFile.value("outputRecording", "DCS_Recording"),
                    inputJsonFile.value("outputFolder", "."),
                    inputJsonFile.value("bus", "dcs.mission"));

    const auto bootLoader = sen::kernel::Bootloader::fromYamlString(yaml, false);

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

    const sen::kernel::KernelConfig::ComponentToLoad componentConfig{
        std::move(component), config, {} // empty params
    };

    // kernel creation
    bootLoader->getConfig().addToLoad(componentConfig);
    kernel_ = std::make_unique<sen::kernel::TestKernel>(bootLoader->getConfig());

    f64 currentTime = 0;
    u64 recordingFrame = 0;
    const f64 finalTime = recording.back().time;
    bool recordingFinished = false;

    logger->info("Converting to sen recording");

    // Kernel loop
    while (!recordingFinished) {
        if (recording[recordingFrame].time <= currentTime) {
            component_->newFrame(recording[recordingFrame]);
            recordingFrame++;
        }

        constexpr f64 stepTime = 0.001;
        kernel_->step();
        currentTime += stepTime;

        if (finalTime <= currentTime) {
            recordingFinished = true;
        }

        {
            std::lock_guard lock(uiState.mutex);
            uiState.convertingProgress = currentTime / finalTime;
        }
        screen.PostEvent(ftxui::Event::Custom);
    }

    // Calculate elapsed time
    auto elapsed = std::chrono::duration<f64>(std::chrono::steady_clock::now() - start);

    logger->info("Finished conversion in {:.3f} s", elapsed.count());

    // Sleep for half a second
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    screen.ExitLoopClosure()();
    return 0;
}