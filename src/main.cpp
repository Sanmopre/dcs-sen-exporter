
// spdlog
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main()
{
    auto logger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("dcs-sen-exporter");
    logger->info("Hello World!");
}
