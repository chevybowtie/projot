#include "repo.h"
#include <cstdlib>
#include <memory>

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
    // Windows, use APPDATA or USERPROFILE
    auto get_env = [](const char* name) -> std::optional<std::string> {
        char* value = nullptr;
        size_t len = 0;
        if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) {
            std::string result(value);
            free(value);
            if (!result.empty()) return result;
        }
        return std::nullopt;
    };

    if (auto appdata = get_env("APPDATA"))
        return std::filesystem::path(*appdata) / "projot" / "config";

    if (auto userprofile = get_env("USERPROFILE"))
        return std::filesystem::path(*userprofile) / ".config" / "projot" / "config";
#else
    // Linux and macOS, follow XDG Base Directory Specification
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        if (*xdg) return std::filesystem::path(xdg) / "projot" / "config";

    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / "projot" / "config";
#endif

    return std::nullopt;
}
