#include "user_config.h"

#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

// ── Platform path ─────────────────────────────────────────────────────────────

std::string get_user_config_path() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata || !*appdata) return "";
    return (fs::path(appdata) / "projot" / "config").string();
#else
    // Respect XDG_CONFIG_HOME; fall back to ~/.config
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        return (fs::path(xdg) / "projot" / "config").string();
    }
    const char* home = std::getenv("HOME");
    if (!home || !*home) return "";
    return (fs::path(home) / ".config" / "projot" / "config").string();
#endif
}

// ── Parsing ───────────────────────────────────────────────────────────────────

ParseResult parse_user_config(const std::string& path, UserConfig& out) {
    out = UserConfig{};
    std::ifstream file(path);
    if (!file.is_open()) {
        // Missing file is not an error; the caller gets defaults.
        return {true, ""};
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        const std::string key   = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (key.empty()) continue;

        if (key.rfind("base_url.", 0) == 0) {
            out.base_urls[key.substr(9)] = value;
        } else if (key == "use_mcp") {
            out.use_mcp = (value == "true" || value == "1" || value == "yes");
        } else if (key == "install_hooks") {
            out.install_hooks = (value == "true" || value == "1" || value == "yes");
        }
        // Unknown keys are silently ignored (future-proofing).
    }

    return {true, ""};
}

// ── Writing ───────────────────────────────────────────────────────────────────

ParseResult write_user_config(const std::string& path, const UserConfig& cfg) {
    fs::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
        if (ec) return {false, "Cannot create directory: " + p.parent_path().string()};
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return {false, "Cannot write user config file: " + path};
    }

    file << "# projot user config\n";
    file << "\n";

    file << "# Base URLs — the project number or ID is appended to form the full link URL.\n";
    file << "# Example: base_url.itrack = https://itrack.com/browse/\n";
    file << "\n";

    // Write known keys first (in canonical order), then any extras.
    const std::vector<std::string> known_keys = {"rpm", "itrack", "github", "appdb"};
    std::vector<std::string> written;
    for (const auto& k : known_keys) {
        auto it = cfg.base_urls.find(k);
        if (it != cfg.base_urls.end()) {
            file << "base_url." << k << " = " << it->second << "\n";
            written.push_back(k);
        }
    }
    for (const auto& [k, v] : cfg.base_urls) {
        if (std::find(written.begin(), written.end(), k) == written.end()) {
            file << "base_url." << k << " = " << v << "\n";
        }
    }

    file << "\n";
    file << "# Feature toggles\n";
    file << "use_mcp = " << (cfg.use_mcp ? "true" : "false") << "\n";
    file << "install_hooks = " << (cfg.install_hooks ? "true" : "false") << "\n";

    return {true, ""};
}

// ── Load helper ───────────────────────────────────────────────────────────────

UserConfig load_user_config() {
    UserConfig cfg;
    const std::string path = get_user_config_path();
    if (!path.empty()) {
        parse_user_config(path, cfg);  // errors silently ignored — caller gets defaults
    }
    return cfg;
}
