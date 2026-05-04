#pragma once

#include "config.h"

#include <string>
#include <map>

// Returns true when a config value string represents a truthy boolean
// ("true", "1", or "yes"). Used by both parse_user_config and cmd_config.
inline bool parse_bool_value(const std::string& v) {
    return v == "true" || v == "1" || v == "yes";
}

// Per-user (global) configuration for projot.
// Stored at a platform-standard location:
//   Linux/macOS: $XDG_CONFIG_HOME/projot/config  (default: ~/.config/projot/config)
//   Windows:     %APPDATA%\projot\config
struct UserConfig {
    // Base URLs for common link types.
    // The project number or ID is appended to form the full link URL.
    // Keys: "rpm", "itrack", "github", "appdb"
    // Example: base_urls["itrack"] = "https://itrack.com/browse/"
    //          -> full URL for itrack 12345 becomes "https://itrack.com/browse/12345"
    std::map<std::string, std::string> base_urls;

    // When false, suppresses any MCP server use by projot even if a repo has it configured.
    bool use_mcp = true;

    // When false, skips git pre-commit hook installation on `projot new`.
    // Per-invocation --no-hook flag still takes precedence.
    bool install_hooks = true;
};

// Returns the platform-standard user config file path.
// Returns an empty string if the path cannot be determined (e.g. HOME is unset).
std::string get_user_config_path();

// Parse a user config file from disk into `out`.
// A missing file is treated as success (caller gets defaults).
// Returns ok=false only on I/O errors other than file-not-found.
ParseResult parse_user_config(const std::string& path, UserConfig& out);

// Write a UserConfig to disk in canonical format.
// Creates parent directories as needed.
ParseResult write_user_config(const std::string& path, const UserConfig& cfg);

// Load user config from the default platform path.
// Never fails: returns defaults if the file is missing, unreadable, or the path
// cannot be determined.
UserConfig load_user_config();
