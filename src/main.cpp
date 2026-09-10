#include "core/desktop_canvas.hpp"
#include "core/desktop_scanner.hpp"
#include "core/desk_config.hpp"
#include "widgets/clock_widget.hpp"
#include "widgets/system_widget.hpp"
#include "widgets/app_shortcut_widget.hpp"
#include <iostream>
#include <csignal>
#include <string>

static std::shared_ptr<miqu::AppEngine> s_engine;
static miqudesk::DesktopCanvas* s_canvas = nullptr;

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (s_engine) {
            s_engine->quit();
        }
    } else if (sig == SIGUSR2 || sig == SIGHUP) {
        if (s_canvas) {
            s_canvas->request_reload();
        }
    }
}

int main(int argc, char* argv[]) {
    std::string config_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: miqudesk [OPTIONS]\n"
                      << "Interactive desktop canvas for Miquland\n\n"
                      << "Options:\n"
                      << "  -c, --config <path>   Use specific configuration file\n"
                      << "  -h, --help            Show this help message\n";
            return 0;
        }
    }

    // Load configuration
    miqudesk::DeskConfig::get().load(config_path);
    const auto& cfg = miqudesk::DeskConfig::get();

    std::cout << "[miqudesk] Starting interactive desktop canvas..." << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGUSR2, signal_handler);
    std::signal(SIGHUP, signal_handler);

    s_engine = miqu::AppEngine::create();
    if (!s_engine) {
        std::cerr << "[miqudesk] Failed to initialize AppEngine." << std::endl;
        return 1;
    }

    auto canvas = std::make_unique<miqudesk::DesktopCanvas>(s_engine.get());
    s_canvas = canvas.get();

    // Add default widgets if enabled
    if (cfg.clock_enabled) {
        auto clock_w = std::make_shared<miqudesk::ClockWidget>();
        canvas->add_widget(clock_w, cfg.clock_x, cfg.clock_y, cfg.clock_width, cfg.clock_height);
    }

    if (cfg.system_enabled) {
        auto system_w = std::make_shared<miqudesk::SystemWidget>();
        canvas->add_widget(system_w, cfg.system_x, cfg.system_y, cfg.system_width, cfg.system_height);
    }

    // Add desktop app shortcuts from ~/Desktop
    canvas->reload_shortcuts();

    // Load user customized positions if they exist
    canvas->load_config();

    if (!canvas->init()) {
        std::cerr << "[miqudesk] Failed to initialize DesktopCanvas window." << std::endl;
        return 1;
    }

    std::cout << "[miqudesk] Desktop canvas active on LayerBottom. Running event loop." << std::endl;
    return s_engine->enter_loop();
}
