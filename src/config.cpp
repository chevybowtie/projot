#include "config.h"
#include "utils.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

// String utilities

std::string trim(const std::string& s) {
    const auto front = s.find_first_not_of(" \t\r\n");
    if (front == std::string::npos) return "";
    const auto back = s.find_last_not_of(" \t\r\n");
    return s.substr(front, back - front + 1);
}

// List items are separated by ',' and may contain "\," (a literal comma, e.g. in a
// URL) or "\\" (a literal backslash). A backslash before anything else is kept as is,
// so values written before escaping existed read back unchanged.
std::vector<std::string> split_list(const std::string& value) {
    std::vector<std::string> result;
    std::string token;
    auto flush = [&] {
        auto t = trim(token);
        if (!t.empty()) result.push_back(t);
        token.clear();
    };
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '\\' && i + 1 < value.size() && (value[i + 1] == ',' || value[i + 1] == '\\')) {
            token += value[++i];
        } else if (c == ',') {
            flush();
        } else {
            token += c;
        }
    }
    flush();
    return result;
}

std::string join_list(const std::vector<std::string>& items) {
    std::string result;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) result += ", ";
        for (char c : items[i]) {
            if (c == ',' || c == '\\') result += '\\';
            result += c;
        }
    }
    return result;
}

// Parsing

// Azure entry helpers

AzureEntry parse_azure_entry(const std::string& s) {
    const auto pipe = s.find('|');
    if (pipe == std::string::npos) return {"", trim(s)};
    return {trim(s.substr(0, pipe)), trim(s.substr(pipe + 1))};
}

std::string format_azure_entry(const AzureEntry& e) {
    // A bare URL containing '|' would parse back with its head as the name, so mark
    // the name as explicitly empty.
    if (e.name.empty()) return e.url.find('|') == std::string::npos ? e.url : "|" + e.url;
    return e.name + "|" + e.url;
}

// List keys whose values are comma-separated lists.
static bool is_list_key(const std::string& key) {
    return key == "github" || key == "swagger" || key == "blizzard" || key == "links" ||
           key == "azure_subscription" || key == "azure_key_vault" ||
           key == "azure_resource_group" || key == "azure_aks" ||
           key == "azure_log_analytics" || key == "azure_storage" ||
           key == "azure_private_dns";
}

ParseResult parse_config(const std::string& path, Config& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {false, "Cannot open config file: " + path};
    }

    out = Config{};
    std::string line;
    while (std::getline(file, line)) {
        // Strip CRLF
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Skip comments and blank lines
        if (line.empty() || line[0] == '#') continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            // Malformed line; log warning for debugging but continue parsing
            std::cerr << "warning: ignoring malformed config line (missing '='): " << line << "\n";
            continue;
        }

        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (key.empty()) continue;

        if (key == "config_version") {
            // Treating an unreadable version as 0 would let a newer, incompatible
            // config (e.g. an overflowing number) slip past the version check.
            if (value.empty() || value.size() > 9 ||
                value.find_first_not_of("0123456789") != std::string::npos) {
                return {false, "Invalid config_version '" + value + "' in " + path};
            }
            out.config_version = std::stoi(value);
        } else if (key == "app_id") {
            out.app_id = value;
        } else if (key == "rpm") {
            out.rpm = value;
        } else if (key == "name") {
            out.name = value;
        } else if (key == "jira" || key == "itrack") {
            out.jira = value;
        } else if (key == "rpm_base_url") {
            out.rpm_base_url = value;
        } else if (key == "jira_base_url" || key == "itrack_base_url") {
            out.jira_base_url = value;
        } else if (key == "date_format") {
            out.date_format = value;
        } else if (key == "created") {
            out.created = value;
        } else if (key == "teams_sync_url" || key == "teams_webhook") {
            out.teams_sync_url = value;
        } else if (is_list_key(key)) {
            auto items = split_list(value);
            if (key == "github")                out.github                = items;
            else if (key == "swagger")          out.swagger               = items;
            else if (key == "blizzard")         out.blizzard              = items;
            else if (key == "links")            out.links                 = items;
            else if (key == "azure_subscription")   out.azure_subscription   = items;
            else if (key == "azure_key_vault")      out.azure_key_vault      = items;
            else if (key == "azure_resource_group") out.azure_resource_group = items;
            else if (key == "azure_aks")            out.azure_aks            = items;
            else if (key == "azure_log_analytics")  out.azure_log_analytics  = items;
            else if (key == "azure_storage")        out.azure_storage        = items;
            else if (key == "azure_private_dns")    out.azure_private_dns    = items;
        } else if (key.rfind("label.", 0) == 0) {
            out.labels[key.substr(6)] = value;
        } else if (key.rfind("link.", 0) == 0) {
            out.link_urls[key.substr(5)] = value;
        }
        // Unknown keys are silently ignored (future-proofing).
    }
    // getline() also stops on a read error; a partial config must not be mistaken
    // for the whole file, or the next write would persist the truncation.
    if (file.bad()) return {false, "Error reading config file: " + path};

    // Configs written before the iTrack -> Jira rename used "itrack" as the link key.
    for (auto& k : out.links) if (k == "itrack") k = "jira";
    for (auto* m : {&out.labels, &out.link_urls}) {
        auto it = m->find("itrack");
        if (it == m->end()) continue;
        m->emplace("jira", it->second); // keeps an explicit jira entry if both exist
        m->erase("itrack");
    }

    return {true, ""};
}

// Writing

std::string invalid_config_value(const Config& cfg) {
    const std::pair<const char*, const std::string*> scalars[] = {
        {"app_id", &cfg.app_id}, {"rpm", &cfg.rpm}, {"name", &cfg.name},
        {"jira", &cfg.jira}, {"created", &cfg.created}, {"date_format", &cfg.date_format},
        {"teams_sync_url", &cfg.teams_sync_url},
        {"rpm_base_url", &cfg.rpm_base_url}, {"jira_base_url", &cfg.jira_base_url},
    };
    for (const auto& [key, value] : scalars)
        if (has_line_break(*value)) return std::string(key) + " contains a line break";

    const std::pair<const char*, const std::vector<std::string>*> lists[] = {
        {"github", &cfg.github}, {"swagger", &cfg.swagger}, {"blizzard", &cfg.blizzard},
        {"azure_subscription", &cfg.azure_subscription},
        {"azure_key_vault", &cfg.azure_key_vault},
        {"azure_resource_group", &cfg.azure_resource_group},
        {"azure_aks", &cfg.azure_aks},
        {"azure_log_analytics", &cfg.azure_log_analytics},
        {"azure_storage", &cfg.azure_storage},
        {"azure_private_dns", &cfg.azure_private_dns},
    };
    for (const auto& [key, items] : lists)
        for (const auto& item : *items)
            if (has_line_break(item)) return std::string(key) + " entry contains a line break";

    // Link keys are also items of the comma-separated `links` list.
    for (const auto& key : cfg.links)
        if (key.find_first_of("=,\r\n") != std::string::npos)
            return "link key '" + key + "' contains '=', ',' or a line break";

    for (const auto* m : {&cfg.labels, &cfg.link_urls}) {
        for (const auto& [key, value] : *m) {
            if (key.find_first_of("=\r\n") != std::string::npos)
                return "link key '" + key + "' contains '=' or a line break";
            if (has_line_break(value))
                return "value for link key '" + key + "' contains a line break";
        }
    }
    return "";
}

ParseResult write_config(const std::string& path, const Config& cfg) {
    const std::string invalid = invalid_config_value(cfg);
    if (!invalid.empty()) return {false, "Refusing to write config file " + path + ": " + invalid};

    // Ensure parent directory exists
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) return {false, "Cannot create directory: " + p.parent_path().string()};
    }

    std::ostringstream file;

    file << "# projot config\n";
    file << "config_version = " << PROJOT_CONFIG_SCHEMA_VERSION << "\n";
    file << "\n";

    file << "# --- Repo-level fields (set by `init`) ---\n";
    file << "\n";
    file << "app_id = " << cfg.app_id << "\n";
    // Repo-level overrides of the global base URLs (read by the MCP server).
    if (!cfg.rpm_base_url.empty())
        file << "rpm_base_url = " << cfg.rpm_base_url << "\n";
    if (!cfg.jira_base_url.empty())
        file << "jira_base_url = " << cfg.jira_base_url << "\n";

    auto write_list = [&](const std::string& key, const std::vector<std::string>& items) {
        file << key << " = " << join_list(deduplicate(items)) << "\n";
    };

    file << "\n";
    write_list("github", cfg.github);
    file << "\n";
    write_list("swagger", cfg.swagger);
    file << "\n";
    write_list("blizzard", cfg.blizzard);
    file << "\n";

    file << "# --- Project-level fields (set by `new`) ---\n";
    file << "\n";
    file << "rpm = " << cfg.rpm << "\n";
    file << "name = " << cfg.name << "\n";
    file << "jira = " << cfg.jira << "\n";

    if (!cfg.created.empty()) {
        file << "created = " << cfg.created << "\n";
    }
    if (!cfg.date_format.empty()) {
        file << "date_format = " << cfg.date_format << "\n";
    }
    if (!cfg.teams_sync_url.empty()) {
        file << "teams_sync_url = " << cfg.teams_sync_url << "\n";
    }

    file << "\n";
    file << "links = " << join_list(cfg.links) << "\n";
    file << "\n";

    // Write label.<key> entries in links order, then any extras
    auto written_labels = std::vector<std::string>{};
    for (const auto& key : cfg.links) {
        auto it = cfg.labels.find(key);
        if (it != cfg.labels.end()) {
            file << "label." << key << " = " << it->second << "\n";
            written_labels.push_back(key);
        }
    }
    for (const auto& [k, v] : cfg.labels) {
        if (std::find(written_labels.begin(), written_labels.end(), k) == written_labels.end()) {
            file << "label." << k << " = " << v << "\n";
        }
    }

    file << "\n";
    // Write link.<key> entries
    auto written_links = std::vector<std::string>{};
    for (const auto& key : cfg.links) {
        auto it = cfg.link_urls.find(key);
        if (it != cfg.link_urls.end()) {
            file << "link." << key << " = " << it->second << "\n";
            written_links.push_back(key);
        }
    }
    for (const auto& [k, v] : cfg.link_urls) {
        if (std::find(written_links.begin(), written_links.end(), k) == written_links.end()) {
            file << "link." << k << " = " << v << "\n";
        }
    }

    // Azure resource fields (project-level)
    bool has_azure = !cfg.azure_subscription.empty() || !cfg.azure_key_vault.empty() ||
                     !cfg.azure_resource_group.empty() || !cfg.azure_aks.empty() ||
                     !cfg.azure_log_analytics.empty() || !cfg.azure_storage.empty() ||
                     !cfg.azure_private_dns.empty();
    if (has_azure) {
        file << "\n";
        file << "# --- Azure resources ---\n";
        file << "\n";
        if (!cfg.azure_subscription.empty())
            write_list("azure_subscription", cfg.azure_subscription);
        if (!cfg.azure_key_vault.empty())
            write_list("azure_key_vault", cfg.azure_key_vault);
        if (!cfg.azure_resource_group.empty())
            write_list("azure_resource_group", cfg.azure_resource_group);
        if (!cfg.azure_aks.empty())
            write_list("azure_aks", cfg.azure_aks);
        if (!cfg.azure_log_analytics.empty())
            write_list("azure_log_analytics", cfg.azure_log_analytics);
        if (!cfg.azure_storage.empty())
            write_list("azure_storage", cfg.azure_storage);
        if (!cfg.azure_private_dns.empty())
            write_list("azure_private_dns", cfg.azure_private_dns);
    }

    std::string error;
    if (!atomic_write_file(p, file.str(), error)) {
        return {false, "Cannot write config file: " + path + " (" + error + ")"};
    }
    return {true, ""};
}

ParseResult write_global_config(const std::string& path, const Config& cfg) {
    if (has_line_break(cfg.rpm_base_url) || has_line_break(cfg.jira_base_url))
        return {false, "Refusing to write global config " + path + ": base URL contains a line break"};

    // Ensure parent directory exists
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) return {false, "Cannot create directory: " + p.parent_path().string()};
    }

    std::ostringstream file;
    file << "# projot global config\n";
    if (!cfg.rpm_base_url.empty())
        file << "rpm_base_url = " << cfg.rpm_base_url << "\n";
    if (!cfg.jira_base_url.empty())
        file << "jira_base_url = " << cfg.jira_base_url << "\n";

    std::string error;
    if (!atomic_write_file(p, file.str(), error)) {
        return {false, "Cannot write global config: " + path + " (" + error + ")"};
    }
    return {true, ""};
}
