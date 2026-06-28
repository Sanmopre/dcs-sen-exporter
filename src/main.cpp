
// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// nlohmann
#include "nlohmann/json.hpp"

// std
#include <fstream>
#include <string>

int main()
{
    auto logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("dcs-sen-exporter");

    logger->info("Opening dcs file");

    std::ifstream file("C:\\Users\\molin\\Saved Games\\DCS\\Logs\\world_frames.ndjson");
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        nlohmann::json frame = nlohmann::json::parse(line);

        int frameNumber = frame["frame"];
        double t = frame["t"];
        logger->info("FRAME {} TIME {}", frameNumber, t);

        for (const auto& aircraft : frame["aircraft"])
        {
            std::string name = aircraft["name"];
            double lat = aircraft["lat"];
            double lon = aircraft["lon"];
            double alt = aircraft["alt"];
            logger->info("AIRCRAFT {} lat {} lon {} alt {}", name, lat, lon, alt);
        }
    }

}
