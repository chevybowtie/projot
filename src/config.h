#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>

// Current config schema version written to .projot/config on `init`.
// Increment only when the schema changes in a breaking way.
// The value is set by CMakeLists.txt as PROJOT_CONFIG_VERSION.
#ifndef PROJOT_CONFIG_VERSION
#define PROJOT_CONFIG_VERSION 1
#endif
static constexpr int PROJOT_CONFIG_SCHEMA_VERSION = PROJOT_CONFIG_VERSION;

// A single Azure resource entry with an optional name and a URL.
// Stored in config as "name|url" (with name) or just "url" (without name).
struct AzureEntry {
    std::string name;  // may be empty for general/feature links
    std::string url;
};

// Parse "name|url" or just "url" into an AzureEntry.
AzureEntry parse_azure_entry(const std::string& s);

// Format an AzureEntry as "name|url" when name is non-empty, else just "url".
std::string format_azure_entry(const AzureEntry& e);

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
    std::string created;       // ISO date when project was created (YYYY-MM-DD)

    // Links configuration
    std::vector<std::string> links;              // ordered link keys
    std::map<std::string, std::string> labels;   // label.<key> -> display label
    std::map<std::string, std::string> link_urls; // link.<key> -> url

    // Azure resources (project-level). Each entry is "name|url" or just "url".
    std::vector<std::string> azure_subscription;
    std::vector<std::string> azure_key_vault;
    std::vector<std::string> azure_resource_group;
    std::vector<std::string> azure_aks;
    std::vector<std::string> azure_log_analytics;
    std::vector<std::string> azure_storage;
    std::vector<std::string> azure_private_dns;
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
