#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

// Current config schema version written to .projot/config on `init`.
// Increment only when the schema changes in a breaking way.
static constexpr int PROJOT_CONFIG_SCHEMA_VERSION = 1;

struct Config {
    // Schema version
    int config_version = 0;

    // Repo-level fields (set by `init`)
    std::string app_id;
    std::vector<std::string> github;
    std::vector<std::string> swagger;
    std::vector<std::string> blizzard;

    // Project-level fields (set by `new`)
    std::string ranp;
    std::string name;
    std::string itrack;
    std::string date_format;

    // Links configuration
    std::vector<std::string> links;              // ordered link keys
    std::map<std::string, std::string> labels;   // label.<key> -> display label
    std::map<std::string, std::string> link_urls; // link.<key> -> url
};

// Result type for operations that can fail with a message
struct ParseResult {
    bool ok = true;
    std::string error;
};

// Parse a .projot/config file from disk.
// Returns ok=false with an error message on failure.
ParseResult parse_config(const std::string& path, Config& out);

// Write a Config to disk in canonical format.
// Returns ok=false with an error message on failure.
ParseResult write_config(const std::string& path, const Config& cfg);

// Split a comma-separated value string into trimmed tokens.
std::vector<std::string> split_list(const std::string& value);

// Join a list of strings into a comma-separated value string.
std::string join_list(const std::vector<std::string>& items);

// Trim leading/trailing whitespace from a string.
std::string trim(const std::string& s);
