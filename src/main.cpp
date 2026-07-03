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


struct LogEntry
{
    std::string timestamp;
    std::string loggerName;
    spdlog::level::level_enum level;
    std::string message;
};

struct UiState
{
    std::mutex mutex;
    std::deque<LogEntry> logs;
    f64 convertingProgress = 0.0;
    f64 readingProgress = 0.0;
    std::string status;

    void AddLog(LogEntry entry)
    {
        std::lock_guard lock(mutex);

        logs.push_back(std::move(entry));

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
        auto time = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());

        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif

        std::ostringstream ts;
        ts << std::put_time(&tm, "%H:%M:%S");

        std::string message(msg.payload.begin(), msg.payload.end());

        while (!message.empty() &&
               (message.back() == '\n' || message.back() == '\r'))
        {
            message.pop_back();
        }

        state_.AddLog(LogEntry{
            ts.str(),
            std::string(msg.logger_name.begin(), msg.logger_name.end()),
            msg.level,
            std::move(message)
        });

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
        "Path to input.json file with the conversion information")
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    UiState uiState;
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    // Logger initialization
    auto sink = std::make_shared<FtxuiSink>(uiState, screen);
    const auto logger =
        std::make_shared<spdlog::logger>(
            "dcs",
            sink);

    spdlog::set_default_logger(logger);


    // FTXUI Renderer application
    auto LevelColor = [](spdlog::level::level_enum level)
    {
        using namespace ftxui;

        switch (level)
        {
            case spdlog::level::trace:    return ftxui::Color::GrayDark;
            case spdlog::level::debug:    return ftxui::Color::Blue;
            case spdlog::level::info:     return ftxui::Color::Green;
            case spdlog::level::warn:     return ftxui::Color::Yellow;
            case spdlog::level::err:      return ftxui::Color::Red;
            case spdlog::level::critical: return ftxui::Color::Red;
            default:                      return ftxui::Color::White;
        }
    };

    // FTXUI Renderer application
    auto renderer = ftxui::Renderer([&]
    {
        std::vector<ftxui::Element> log_elements;
        f64 readingProgress = 0.0;
        f64 convertingProgress = 0.0;

        {
            std::lock_guard lock(uiState.mutex);

            readingProgress = uiState.readingProgress;
            convertingProgress = uiState.convertingProgress;

            for (const auto&[timestamp, loggerName, level, message] : uiState.logs)
            {
                auto levelText =
                    std::string(spdlog::level::to_string_view(level).data(),
                                spdlog::level::to_string_view(level).size());

                log_elements.push_back(
                    ftxui::hbox({
                        ftxui::text("[" + timestamp + "] "),
                        ftxui::text("[" + loggerName + "] "),
                        ftxui::text("[" + levelText + "] ")
                            | ftxui::color(LevelColor(level)),
                        ftxui::text(message),
                    })
                );
            }
        }

        return ftxui::vbox({
            ftxui::text("Digital Combat Simulator <=> Sen") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            ftxui::text("Reading"),
            ftxui::gauge(readingProgress) | ftxui::color(ftxui::Color::Blue),
            ftxui::separator(),
            ftxui::text("Converting"),
            ftxui::gauge(convertingProgress) | ftxui::color(ftxui::Color::Green),
            ftxui::separator(),
            ftxui::vbox(std::move(log_elements))
                | ftxui::frame
                | ftxui::vscroll_indicator
                | ftxui::border
                | ftxui::flex,
        });
    });

    std::thread worker([&]
    {
    // Start timer
    auto start = std::chrono::steady_clock::now();

    // Opening input file
    logger->info("Opening input file {}", inputFile.string());
    std::ifstream inputFileStream(inputFile);
    if (!inputFileStream)
    {
        logger->error("Failed to open input {}", inputFile.string());
        return 1;
    }
    const auto inputJsonFile = nlohmann::json::parse(inputFileStream);

    std::filesystem::path recordingFilePath;

     // Check if the recording key exists
     if (const auto it = inputJsonFile.find("recording"); it != inputJsonFile.end())
     {
         recordingFilePath = inputJsonFile.at("recording").get<std::string>();
     }
     else
     {
         logger->error("Failed to find recording entry in {}", inputFile.string());
         return 1;
     }

    // Opening recording file
    logger->info("Opening recording {}", recordingFilePath.string());
    std::ifstream recordingFileStream(recordingFilePath);
    if (!recordingFileStream)
    {
        logger->error("Failed to open recording {}", recordingFilePath.string());
        return 1;
    }


    // Get the number of lines in the file
    std::string line;

    u64 lineCount = 0;
    while (std::getline(recordingFileStream, line))
    {
        if (!line.empty())
            ++lineCount;
    }

    // Reset file
    recordingFileStream.clear();
    recordingFileStream.seekg(0);

    logger->info("Reading recording with {} entries", lineCount);

    Recording recording;
    //recording.reserve(lineCount);

    u64 recordingCount = 0;
    while (std::getline(recordingFileStream, line))
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
        recordingCount++;
        {
            std::lock_guard lock(uiState.mutex);
            uiState.readingProgress =
                static_cast<f64>(recordingCount) / static_cast<f64>(lineCount);
        }
        screen.PostEvent(ftxui::Event::Custom);
    }


    std::shared_ptr<DcsComponent> component_;
    std::unique_ptr<sen::kernel::TestKernel> kernel_;

    // Check if mappings exist
    if (const auto it = inputJsonFile.find("mappings"); it == inputJsonFile.end())
    {
        logger->warn("Failed to find mappings entry in {}", inputFile.string());
    }

    // Get mappings
    Mappings mappings;
    fillMappings(inputJsonFile["mappings"], mappings);
    logger->info("Mappings size {}", mappings.size());

    component_ = std::make_shared<DcsComponent>(std::chrono::milliseconds(1),"dcs.mission", mappings, logger.get());

        const std::string yaml = fmt::format(R"(load:
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

        {
            std::lock_guard lock(uiState.mutex);
            uiState.convertingProgress = currentTime / finalTime;
        }
        screen.PostEvent(ftxui::Event::Custom);
    }


        // Calculate elapsed time
        auto elapsed =
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start);

        logger->info("Finished conversion in {:.3f} s", elapsed.count());
        return 0;
    });

    screen.Loop(renderer);
    worker.join();

    return 0;
}
