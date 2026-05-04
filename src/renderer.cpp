#include "renderer.h"
#include "utils.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

// Deduplicate a vector while preserving order.
static std::vector<std::string> dedup_urls(const std::vector<std::string>& v) {
    std::vector<std::string> result;
    for (const auto& s : v) {
        if (std::find(result.begin(), result.end(), s) == result.end())
            result.push_back(s);
    }
    return result;
}

std::string render_markdown(const Config& cfg, const std::vector<Todo>& todos) {
    std::ostringstream out;

    // ── Header ──────────────────────────────────────────────────────────────
    out << "# Project: " << cfg.name << "\n";
    out << "\n";
    out << "- RPM: " << cfg.rpm << "\n";
    out << "- iTrack: " << (cfg.itrack.empty() ? "N/A" : cfg.itrack) << "\n";
    out << "- App ID: " << (cfg.app_id.empty() ? "N/A" : cfg.app_id) << "\n";
    out << "- Created: " << (cfg.created.empty() ? "N/A" : cfg.created) << "\n";
    out << "- Last Updated: " << date_today() << "\n";
    out << "\n";

    // ── Links section ────────────────────────────────────────────────────────
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

    // ── Managed sections comment ─────────────────────────────────────────────
    bool has_managed = !cfg.github.empty() || !cfg.swagger.empty() || !cfg.blizzard.empty() ||
                       !cfg.azure_subscription.empty() || !cfg.azure_key_vault.empty() ||
                       !cfg.azure_resource_group.empty() || !cfg.azure_aks.empty() ||
                       !cfg.azure_log_analytics.empty() || !cfg.azure_storage.empty() ||
                       !cfg.azure_private_dns.empty();
    if (has_managed) {
        out << "<!-- projot-managed: do not hand-edit sections below; "
               "use add-github/add-swagger/add-blizzard/add-azure -->\n";
        out << "\n";
    }

    // ── GitHub ───────────────────────────────────────────────────────────────
    const auto github = dedup_urls(cfg.github);
    if (!github.empty()) {
        out << "## GitHub\n";
        for (const auto& url : github) out << "- " << url << "\n";
        out << "\n";
    }

    // ── Swagger ──────────────────────────────────────────────────────────────
    const auto swagger = dedup_urls(cfg.swagger);
    if (!swagger.empty()) {
        out << "## Swagger\n";
        for (const auto& url : swagger) out << "- " << url << "\n";
        out << "\n";
    }

    // ── Blizzard ─────────────────────────────────────────────────────────────
    const auto blizzard = dedup_urls(cfg.blizzard);
    if (!blizzard.empty()) {
        out << "## Blizzard\n";
        for (const auto& url : blizzard) out << "- " << url << "\n";
        out << "\n";
    }

    // ── Azure ─────────────────────────────────────────────────────────────────
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
            const auto deduped = dedup_urls(sec.entries);
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

    // ── Todos ─────────────────────────────────────────────────────────────────
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
