#include "config.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

// ── String utilities ─────────────────────────────────────────────────────────

std::string trim(const std::string& s) {
    const auto front = s.find_first_not_of(" \t\r\n");
    if (front == std::string::npos) return "";
    const auto back = s.find_last_not_of(" \t\r\n");
    return s.substr(front, back - front + 1);
}

std::vector<std::string> split_list(const std::string& value) {
    std::vector<std::string> result;
    std::istringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        auto t = trim(token);
        if (!t.empty()) result.push_back(t);
    }
    return result;
}

std::string join_list(const std::vector<std::string>& items) {
    std::string result;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) result += ", ";
        result += items[i];
    }
    return result;
}

// Deduplicate a vector while preserving order.
static std::vector<std::string> dedup(const std::vector<std::string>& v) {
    std::vector<std::string> result;
    for (const auto& s : v) {
        if (std::find(result.begin(), result.end(), s) == result.end())
            result.push_back(s);
    }
    return result;
}

// ── Parsing ──────────────────────────────────────────────────────────────────

// ── Azure entry helpers ──────────────────────────────────────────────────────

AzureEntry parse_azure_entry(const std::string& s) {
    const auto pipe = s.find('|');
    if (pipe == std::string::npos) return {"", trim(s)};
    return {trim(s.substr(0, pipe)), trim(s.substr(pipe + 1))};
}

std::string format_azure_entry(const AzureEntry& e) {
    if (e.name.empty()) return e.url;
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
        if (eq == std::string::npos) continue; // malformed line, ignore

        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (key.empty()) continue;

        if (key == "config_version") {
            try { out.config_version = std::stoi(value); }
            catch (...) { out.config_version = 0; }
        } else if (key == "app_id") {
            out.app_id = value;
        } else if (key == "ranp") {
            out.ranp = value;
        } else if (key == "name") {
            out.name = value;
        } else if (key == "itrack") {
            out.itrack = value;
        } else if (key == "date_format") {
            out.date_format = value;
        } else if (key == "created") {
            out.created = value;
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

    return {true, ""};
}

// ── Writing ──────────────────────────────────────────────────────────────────

ParseResult write_config(const std::string& path, const Config& cfg) {
    // Ensure parent directory exists
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) return {false, "Cannot create directory: " + p.parent_path().string()};
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return {false, "Cannot write config file: " + path};
    }

    file << "# projot config\n";
    file << "config_version = " << PROJOT_CONFIG_SCHEMA_VERSION << "\n";
    file << "\n";

    file << "# --- Repo-level fields (set by `init`) ---\n";
    file << "\n";
    file << "app_id = " << cfg.app_id << "\n";

    auto write_list = [&](const std::string& key, const std::vector<std::string>& items) {
        file << key << " = " << join_list(dedup(items)) << "\n";
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
    file << "ranp = " << cfg.ranp << "\n";
    file << "name = " << cfg.name << "\n";
    file << "itrack = " << cfg.itrack << "\n";

    if (!cfg.created.empty()) {
        file << "created = " << cfg.created << "\n";
    }
    if (!cfg.date_format.empty()) {
        file << "date_format = " << cfg.date_format << "\n";
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

    return {true, ""};
}
