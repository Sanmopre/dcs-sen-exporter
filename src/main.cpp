// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// nlohmann
#include "nlohmann/json.hpp"

// cli11
#include "CLI/CLI.hpp"

// std
#include <fstream>
#include <filesystem>
#include <string>

// platform manager
#include "platform_manager.h"


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

    const auto logger =
        spdlog::create<spdlog::sinks::stdout_color_sink_mt>(
            "dcs-sen-exporter");

    logger->info("Opening {}", inputFile.string());

    std::ifstream file(inputFile);

    if (!file)
    {
        logger->error("Failed to open {}", inputFile.string());
        return 1;
    }

    std::string line;
    Recording recording;
    // Try to reserve the memory given the number of lines in the file

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        FrameData data;

        auto frame = nlohmann::json::parse(line);

        data.frameNumber = frame["frame"];
        data.time = frame["t"];
        data.platforms.reserve(frame["aircraft"].size());

        for (const auto& aircraft : frame["aircraft"])
        {
            PlatformData platformData;
            platformData.name = aircraft["name"];
            platformData.id = aircraft["id"];
            platformData.spatial.latitude = aircraft["lat"];
            platformData.spatial.longitude = aircraft["lon"];
            platformData.spatial.altitude = aircraft["alt"];
        }
    }


    std::unordered_map<u64, std::shared_ptr<PlatformManager>> entities;

    return 0;
}
