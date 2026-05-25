#include "renderer.h"
#include "utils.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

std::string render_markdown(const Config& cfg, const std::vector<Todo>& todos) {
    std::ostringstream out;

    // Header
    out << "# Project: " << cfg.name << "\n";
    out << "\n";
    out << "- RPM: " << cfg.rpm << "\n";
    out << "- iTrack: " << (cfg.itrack.empty() ? "N/A" : cfg.itrack) << "\n";
    out << "- App ID: " << (cfg.app_id.empty() ? "N/A" : cfg.app_id) << "\n";
    out << "- Created: " << (cfg.created.empty() ? "N/A" : cfg.created) << "\n";
    out << "- Last Updated: " << date_today() << "\n";
    out << "\n";

    // Links section────
    out << "## Links\n";
    for (const auto& key : cfg.links) {
        // Look up human-friendly label
        std::string label = key;
        auto lab_it = cfg.labels.find(key);
        if (lab_it != cfg.labels.end()) label = lab_it->second;

        // Look up URL
        std::string url = "N/A";
        auto url_it = cfg.link_urls.find(key);
        if (url_it != cfg.link_urls.end() && !url_it->second.empty()) url = url_it->second;

        out << "- " << label << ": " << url << "\n";
    }
    out << "\n";

    // GitHub
    const auto github = deduplicate(cfg.github);
    if (!github.empty()) {
        out << "## GitHub\n";
        for (const auto& url : github) out << "- " << url << "\n";
        out << "\n";
    }

    // Swagger
    const auto swagger = deduplicate(cfg.swagger);
    if (!swagger.empty()) {
        out << "## Swagger\n";
        for (const auto& url : swagger) out << "- " << url << "\n";
        out << "\n";
    }

    // Blizzard
    const auto blizzard = deduplicate(cfg.blizzard);
    if (!blizzard.empty()) {
        out << "## Blizzard\n";
        for (const auto& url : blizzard) out << "- " << url << "\n";
        out << "\n";
    }

    // Azure
    struct AzureSection { const char* heading; const std::vector<std::string>& entries; };
    const AzureSection azure_sections[] = {
        {"Subscriptions",      cfg.azure_subscription},
        {"Key Vaults",         cfg.azure_key_vault},
        {"Resource Groups",    cfg.azure_resource_group},
        {"AKS Clusters",       cfg.azure_aks},
        {"Log Analytics",      cfg.azure_log_analytics},
        {"Storage Containers", cfg.azure_storage},
        {"Private DNS Zones",  cfg.azure_private_dns},
    };

    bool any_azure = false;
    for (const auto& sec : azure_sections) {
        if (!sec.entries.empty()) { any_azure = true; break; }
    }

    if (any_azure) {
        out << "## Azure\n";
        out << "\n";
        for (const auto& sec : azure_sections) {
            const auto deduped = deduplicate(sec.entries);
            if (deduped.empty()) continue;
            out << "### " << sec.heading << "\n";
            for (const auto& raw : deduped) {
                AzureEntry e = parse_azure_entry(raw);
                if (!e.name.empty() && !e.url.empty())
                    out << "- [" << e.name << "](" << e.url << ")\n";
                else if (!e.url.empty())
                    out << "- " << e.url << "\n";
                else if (!e.name.empty())
                    out << "- " << e.name << "\n";
            }
            out << "\n";
        }
    }

    // Todos
    // Build a managed comment that hints which projot commands can edit the
    // content shown above (helps users and tests discover relevant commands).
    std::vector<std::string> managed_cmds;
    if (!github.empty()) managed_cmds.push_back("add-github");
    if (!swagger.empty()) managed_cmds.push_back("add-swagger");
    if (!blizzard.empty()) managed_cmds.push_back("add-blizzard");
    // Any Azure section present -> mention add-azure
    bool has_azure = any_azure;
    if (has_azure) managed_cmds.push_back("add-azure");

    out << "<!-- projot-managed: content above is generated from .projot/config";
    if (!managed_cmds.empty()) {
        out << " — edit using projot commands: ";
        for (size_t i = 0; i < managed_cmds.size(); ++i) {
            if (i) out << ", ";
            out << managed_cmds[i];
        }
    } else {
        out << " — edit using projot commands";
    }
    out << "; edits here will be overwritten on next render -->\n";
    out << "## Todos\n";
    out << "\n";

    for (const auto& todo : todos) {
        const char* box = todo.completed ? "[x]" : "[ ]";
        out << todo.id << ". " << box << " " << todo.text << "\n";
        out << "   - Created: " << todo.created_date << "\n";
        if (todo.completed && !todo.completed_date.empty()) {
            out << "   - Completed: " << todo.completed_date << "\n";
        }
        out << "   - Notes:\n";
        for (const auto& note : todo.notes) {
            out << "     - " << note << "\n";
        }
        out << "\n";
    }

    return out.str();
}

RenderResult render_to_file(const std::string& path, const Config& cfg, const std::vector<Todo>& todos) {
    // Ensure parent directory exists
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) return {false, "Cannot create directory: " + p.parent_path().string()};
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return {false, "Cannot write notes file: " + path};
    }

    file << render_markdown(cfg, todos);
    return {true, ""};
}
