#include "desktop_scanner.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>

namespace miqudesk {

namespace fs = std::filesystem;

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

std::string DesktopScanner::clean_exec(const std::string& raw) {
    std::string result;
    result.reserve(raw.size());

    size_t i = 0;
    while (i < raw.size()) {
        if (raw[i] == '%' && i + 1 < raw.size() && raw[i + 1] == '%') {
            result += '%';
            i += 2;
            continue;
        }

        if (raw[i] == '%' && i + 1 < raw.size()) {
            char code = raw[i + 1];
            if (code == 'f' || code == 'F' || code == 'u' || code == 'U' ||
                code == 'd' || code == 'D' || code == 'n' || code == 'N' ||
                code == 'i' || code == 'c' || code == 'k' || code == 'v' ||
                code == 'm') {
                if (!result.empty() && result.back() == ' ') {
                    if (i + 2 == raw.size() || raw[i + 2] == ' ' || raw[i + 2] == '"' || raw[i + 2] == '\'') {
                        result.pop_back();
                    }
                }
                if (result.size() >= 1 && (result.back() == '"' || result.back() == '\'')) {
                    char quote = result.back();
                    if (i + 2 < raw.size() && raw[i + 2] == quote) {
                        result.pop_back();
                        if (!result.empty() && result.back() == ' ') {
                            result.pop_back();
                        }
                        i += 3;
                        continue;
                    }
                }
                i += 2;
                continue;
            }
        }
        result += raw[i++];
    }

    while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }
    return result;
}

static bool parse_desktop_file(const std::string& file_path, DesktopShortcut& out) {
    std::ifstream f(file_path);
    if (!f.is_open()) return false;

    std::string line;
    bool in_entry = false;
    std::string name, exec, icon;
    bool terminal = false;
    bool nodisplay = false;

    while (std::getline(f, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#') continue;

        if (line == "[Desktop Entry]") {
            in_entry = true;
            continue;
        } else if (!line.empty() && line[0] == '[' && line != "[Desktop Entry]") {
            in_entry = false;
        }

        if (!in_entry) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq));
        std::string val = trim_str(line.substr(eq + 1));

        if (key == "Name" && name.empty()) {
            name = val;
        } else if (key == "Exec" && exec.empty()) {
            exec = val;
        } else if (key == "Icon" && icon.empty()) {
            icon = val;
        } else if (key == "Terminal") {
            terminal = (val == "true" || val == "1");
        } else if (key == "NoDisplay") {
            nodisplay = (val == "true" || val == "1");
        }
    }

    if (nodisplay || name.empty() || exec.empty()) {
        return false;
    }

    fs::path p(file_path);
    std::string stem = p.stem().string();

    out.id = stem;
    out.name = name;
    out.exec = DesktopScanner::clean_exec(exec);
    out.icon = icon.empty() ? stem : icon;
    out.terminal = terminal;
    return true;
}

std::string DesktopScanner::get_desktop_dir() {
    const char* xdg_desktop = getenv("XDG_DESKTOP_DIR");
    if (xdg_desktop && *xdg_desktop && fs::exists(xdg_desktop)) {
        return xdg_desktop;
    }
    const char* home = getenv("HOME");
    if (home && *home) {
        std::string d = std::string(home) + "/Desktop";
        if (fs::exists(d)) return d;
    }
    return "";
}

std::vector<DesktopShortcut> DesktopScanner::get_shortcuts() {
    std::vector<DesktopShortcut> shortcuts;

    std::string desktop_dir = get_desktop_dir();

    if (!desktop_dir.empty() && fs::exists(desktop_dir) && fs::is_directory(desktop_dir)) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(desktop_dir, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec) || entry.is_symlink(ec)) {
                if (entry.path().extension() == ".desktop") {
                    DesktopShortcut s;
                    if (parse_desktop_file(entry.path().string(), s)) {
                        shortcuts.push_back(std::move(s));
                    }
                }
            }
        }
    }

    if (shortcuts.empty()) {
        shortcuts = get_default_shortcuts();
    }

    return shortcuts;
}

std::vector<DesktopShortcut> DesktopScanner::get_default_shortcuts() {
    std::vector<DesktopShortcut> defaults;

    // List of candidate apps to look for on system
    const std::vector<std::string> candidates = {
        "/usr/share/applications/kitty.desktop",
        "/usr/share/applications/firefox.desktop",
        "/usr/share/applications/org.gnome.Nautilus.desktop",
        "/usr/share/applications/thunar.desktop",
        "/usr/share/applications/code.desktop",
        "/usr/share/applications/yazi.desktop"
    };

    for (const auto& c : candidates) {
        if (fs::exists(c)) {
            DesktopShortcut s;
            if (parse_desktop_file(c, s)) {
                defaults.push_back(std::move(s));
            }
        }
    }

    // If still empty (e.g. minimal setup), add fallback commands
    if (defaults.empty()) {
        defaults.push_back({"kitty", "Terminal", "kitty", "utilities-terminal", false});
        defaults.push_back({"firefox", "Browser", "firefox", "firefox", false});
        defaults.push_back({"files", "Files", "kitty -e yazi", "system-file-manager", false});
    }

    return defaults;
}

} // namespace miqudesk
