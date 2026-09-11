#pragma once

#include <string>
#include <vector>

namespace miqudesk {

struct DesktopShortcut {
    std::string id;
    std::string name;
    std::string exec;
    std::string icon;
    bool terminal = false;
};

class DesktopScanner {
public:
    static std::string get_desktop_dir();
    static std::vector<DesktopShortcut> get_shortcuts();
    static std::string clean_exec(const std::string& raw);
};

} // namespace miqudesk
