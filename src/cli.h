#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

// Parsed command-line arguments.
struct Args {
    std::string subcommand;
    // Multi-value map: flags["github"] may hold several URLs if --github is repeated.
    std::map<std::string, std::vector<std::string>> flags;
    std::vector<std::string> positional;    // non-flag tokens after the subcommand
    std::vector<std::string> unknown_flags; // unrecognised tokens (single-dash, etc.)
    bool help_requested    = false;
    bool version_requested = false;

    bool has(const std::string& key) const {
        auto it = flags.find(key);
        return it != flags.end() && !it->second.empty();
    }

    // Returns the first value for a flag, or `def` if absent.
    std::string get(const std::string& key, const std::string& def = "") const {
        auto it = flags.find(key);
        if (it == flags.end() || it->second.empty()) return def;
        return it->second[0];
    }

    // Returns all values for a repeatable flag (e.g. --github).
    std::vector<std::string> get_all(const std::string& key) const {
        auto it = flags.find(key);
        if (it == flags.end()) return {};
        return it->second;
    }
};

// Known boolean flags that take no value argument.
inline const std::set<std::string>& boolean_flags() {
    static const std::set<std::string> s{
        "force", "no-hook", "open", "closed", "all", "no-mcp", "no-vscode"
    };
    return s;
}

// Parse argc/argv into an Args struct.
Args parse_args(int argc, char* argv[]);
