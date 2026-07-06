// spdlog
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

// nlohmann
#include "nlohmann/json.hpp"

// cli11
#include "CLI/CLI.hpp"

// std
#include <filesystem>
#include <string>

// sen
#include "sen/kernel/bootloader.h"

// fxtui
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

// recording processing
#include "recording_processing.h"

class FtxuiSink : public spdlog::sinks::base_sink<std::mutex> {
  public:
    FtxuiSink(UiState &state, ftxui::ScreenInteractive &screen) : state_(state), screen_(screen) {}

  protected:
    void sink_it_(const spdlog::details::log_msg &msg) override {
        auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif

        std::ostringstream ts;
        ts << std::put_time(&tm, "%H:%M:%S");

        std::string message(msg.payload.begin(), msg.payload.end());

        while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }

        state_.AddLog(LogEntry{ts.str(),
                               std::string(msg.logger_name.begin(), msg.logger_name.end()),
                               msg.level, std::move(message)});

        screen_.PostEvent(ftxui::Event::Custom);
    }

    void flush_() override {}

  private:
    UiState &state_;
    ftxui::ScreenInteractive &screen_;
};

int main(int argc, char **argv) {
    CLI::App app{"DCS sen exporter"};

    std::filesystem::path inputFile;

    app.add_option("input,-i,--input", inputFile,
                   "Path to input.json file with the conversion information")
        ->required()
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    UiState uiState;
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    // Logger initialization
    auto sink = std::make_shared<FtxuiSink>(uiState, screen);
    const auto logger = std::make_shared<spdlog::logger>("dcs", sink);

    spdlog::apply_all([&](const std::shared_ptr<spdlog::logger>& logger)
    {
        logger->sinks().clear();
        logger->sinks().push_back(sink);
    });


    spdlog::set_default_logger(logger);

    // FTXUI Renderer application
    auto LevelColor = [](spdlog::level::level_enum level) {
        using namespace ftxui;

        switch (level) {
        case spdlog::level::trace:
            return ftxui::Color::GrayDark;
        case spdlog::level::debug:
            return ftxui::Color::Blue;
        case spdlog::level::info:
            return ftxui::Color::Green;
        case spdlog::level::warn:
            return ftxui::Color::Yellow;
        case spdlog::level::err:
            return ftxui::Color::Red;
        case spdlog::level::critical:
            return ftxui::Color::Red;
        default:
            return ftxui::Color::White;
        }
    };

    // FTXUI Renderer application
    auto renderer = ftxui::Renderer([&] {
        std::vector<ftxui::Element> log_elements;
        f64 readingProgress = 0.0;
        f64 convertingProgress = 0.0;

        {
            std::lock_guard lock(uiState.mutex);

            readingProgress = uiState.readingProgress;
            convertingProgress = uiState.convertingProgress;

            for (const auto &[timestamp, loggerName, level, message] : uiState.logs) {
                auto levelText = std::string(spdlog::level::to_string_view(level).data(),
                                             spdlog::level::to_string_view(level).size());

                log_elements.push_back(ftxui::hbox({
                    ftxui::text("[" + timestamp + "] "),
                    ftxui::text("[" + loggerName + "] "),
                    ftxui::text("[" + levelText + "] ") | ftxui::color(LevelColor(level)),
                    ftxui::text(message),
                }));
            }
        }

        return ftxui::vbox({
            ftxui::separator(),
            ftxui::paragraph(R"(
 ______      ______   ______       _              _       ______   ________  ____  _____
|_   _ `.  .' ___  |.' ____ \     / /            \ \    .' ____ \ |_   __  ||_   \|_   _|
  | | `. \/ .'   \_|| (___ \_|   / /______  ______\ \   | (___ \_|  | |_ \_|  |   \ | |
  | |  | || |        _.____`.   < <|______||______|> >   _.____`.   |  _| _   | |\ \| |
 _| |_.' /\ `.___.'\| \____) |   \ \              / /   | \____) | _| |__/ | _| |_\   |_
|______.'  `.____ .' \______.'    \_\            /_/     \______.'|________||_____|\____|
)") | ftxui::bold | ftxui::center| ftxui::border,
            ftxui::separator(),
            ftxui::text("Reading"),
            ftxui::gauge(static_cast<f32>(readingProgress)) | ftxui::color(ftxui::Color::Blue),
            ftxui::separator(),
            ftxui::text("Converting"),
            ftxui::gauge(static_cast<f32>(convertingProgress)) | ftxui::color(ftxui::Color::Green),
            ftxui::separator(),
            ftxui::vbox(std::move(log_elements)) | ftxui::frame | ftxui::vscroll_indicator |
                ftxui::border | ftxui::flex,
        });
    });

    std::thread worker(
        processRecording,
        logger.get(),
        std::cref(inputFile),
        std::ref(uiState),
        std::ref(screen)
    );

    screen.Loop(renderer);
    worker.join();

    spdlog::shutdown();
    return 0;
}
