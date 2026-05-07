#include "repo.h"
#include <cstdlib>

std::optional<std::filesystem::path> find_repo_root(const std::filesystem::path& start) {
    namespace fs = std::filesystem;

    fs::path current = start.empty() ? fs::current_path() : fs::absolute(start);

    while (true) {
        // Accept both a .git directory and a .git file (worktrees)
        if (fs::exists(current / ".git")) {
            return current;
        }

        const auto parent = current.parent_path();
        if (parent == current) {
            // Reached filesystem root without finding .git
            return std::nullopt;
        }
        current = parent;
    }
}

std::optional<std::filesystem::path> global_config_path() {
#ifdef _WIN32
    // Windows: use APPDATA (preferred) or USERPROFILE fallback
    if (const char* appdata = std::getenv("APPDATA"))
        return std::filesystem::path(appdata) / "projot" / "config";
    if (const char* userprofile = std::getenv("USERPROFILE"))
        return std::filesystem::path(userprofile) / ".config" / "projot" / "config";
#else
    // Unix/Linux/macOS: use XDG_CONFIG_HOME or ~/.config
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        if (*xdg) return std::filesystem::path(xdg) / "projot" / "config";
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / "projot" / "config";
#endif
    return std::nullopt;
}
