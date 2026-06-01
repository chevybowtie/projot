#include "commands.h"
#include "commands_internal.h"
#include "config.h"
#include "markdown.h"
#include "renderer.h"
#include "repo.h"
#include "utils.h"

#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
static const char* CARRYOVER_TODOS_FILE = "carryover_todos.md";

// init

int cmd_init(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot init --app-id <App ID> [options]\n\n"
            "Initialize projot for the current git repository.\n\n"
            "Required:\n"
            "  --app-id <App ID>   Application ID for this repository\n\n"
            "Optional:\n"
            "  --github <URL>      Add a GitHub URL (repeatable)\n"
            "  --swagger <URL>     Add a Swagger URL (repeatable)\n"
            "  --blizzard <URL>    Add a Blizzard URL (repeatable)\n\n"
            "Example:\n"
            "  projot init --app-id MyApp --github https://github.com/org/repo\n";
        return 0;
    }

    if (!args.has("app-id")) {
        std::cerr << "error: --app-id is required. Run 'projot init --help' for usage.\n";
        return 1;
    }

    auto root = find_repo_root();
    if (!root) {
        std::cerr << "error: not inside a git repository.\n";
        return 1;
    }

    auto projot_dir = *root / ".projot";
    if (fs::exists(projot_dir)) {
        std::cerr << "error: .projot/ already exists in " << root->string()
                  << ". Remove it to re-initialize.\n";
        return 1;
    }

    Config cfg;
    cfg.app_id   = args.get("app-id");
    cfg.github   = args.get_all("github");
    cfg.swagger  = args.get_all("swagger");
    cfg.blizzard = args.get_all("blizzard");

    auto result = write_config((projot_dir / "config").string(), cfg);
    if (!result.ok) { std::cerr << "error: " << result.error << "\n"; return 1; }

    std::cout << "Initialized projot for " << cfg.app_id
              << " in " << projot_dir.string() << "\n";

    return 0;
}

// new

int cmd_new(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot new --rpm <RPM> --name \"<Name>\" --itrack <iTrack> [options]\n\n"
            "Start a new RPM project in this repository.\n\n"
            "Required:\n"
            "  --rpm <RPM>               RPM project number\n"
            "  --name \"<Project Name>\"   Human-readable project name\n"
            "  --itrack <iTrack>         iTrack ticket number\n\n"
            "Optional:\n"
            "  --teams <URL>             Teams channel URL\n"
            "  --teams-webhook <URL>     Teams incoming webhook URL for Kanban sync\n"
            "  --rpm-url <URL>           RPM system link\n"
            "  --itrack-url <URL>        iTrack link\n"
            "  --other <URL>             Other URL\n"
            "  --no-hook                 Skip pre-commit hook installation\n\n"
            "Example:\n"
            "  projot new --rpm 12345 --name \"Widget Redesign\" --itrack 67890\n";
        return 0;
    }

    if (!args.has("rpm") || !args.has("name") || !args.has("itrack")) {
        std::cerr << "error: --rpm, --name, and --itrack are required. "
                     "Run 'projot new --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }

    if (!ctx.config.rpm.empty()) {
        std::cerr << "error: a project (RPM " << ctx.config.rpm
                  << ") is already configured.\n";
        return 1;
    }

    ctx.config.rpm     = args.get("rpm");
    ctx.config.name    = args.get("name");
    ctx.config.itrack  = args.get("itrack");
    ctx.config.created = date_today();

    // Build links list from provided optional flags
    struct LinkDef { std::string key, label, flag; };
    for (const auto& ld : std::vector<LinkDef>{
            {"teams",  "Teams",  "teams"},
            {"itrack", "iTrack", "itrack-url"},
            {"rpm",    "RPM",    "rpm-url"},
            {"other",  "Other",  "other"},
        }) {
        if (args.has(ld.flag)) {
            ctx.config.links.push_back(ld.key);
            ctx.config.labels[ld.key]    = ld.label;
            ctx.config.link_urls[ld.key] = args.get(ld.flag);
        }
    }

    if (args.has("teams-webhook"))
        ctx.config.teams_webhook = args.get("teams-webhook");

    auto save = write_config(projot_file_path(ctx, "config"), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    std::vector<Todo> carryover_todos;
    fs::path carryover_path = ctx.repo_root / ".projot" / CARRYOVER_TODOS_FILE;
    if (fs::exists(carryover_path)) {
        Project carryover_project;
        auto parse = parse_markdown(carryover_path.string(), carryover_project);
        if (!parse.ok) {
            std::cerr << "error: " << parse.error << "\n";
            return 1;
        }

        int next_id = 1;
        for (const auto* todo : filter_todos(carryover_project.todos, TodoFilter::Open)) {
            Todo copied = *todo;
            copied.id = next_id++;
            copied.completed_date.clear();
            carryover_todos.push_back(std::move(copied));
        }
    }

    auto render = render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, carryover_todos);
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    std::error_code ec;
    fs::remove(carryover_path, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
        std::cerr << "warning: failed to clear carryover todos file: " << ec.message() << "\n";
    }

    std::cout << "Created project " << ctx.config.name
              << " (RPM " << ctx.config.rpm << ")\n";

    if (!args.has("no-hook")) {
        bool appended = false;
        std::string hook_error;
        if (!install_hook_impl(ctx.repo_root, appended, hook_error)) {
            std::cerr << "warning: could not install pre-commit hook: " << hook_error << "\n";
        } else if (appended) {
            std::cout << "Note: appended projot render block to existing "
                         ".git/hooks/pre-commit\n";
        } else {
            std::cout << "Installed pre-commit hook.\n";
        }
    }

    return 0;
}

// set-app-id

int cmd_set_app_id(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot set-app-id --app-id <App ID> [--force]\n\n"
            "Set the application ID in config.\n\n"
            "Required:\n"
            "  --app-id <App ID>   New application ID\n\n"
            "Optional:\n"
            "  --force             Required if app_id is already set\n\n"
            "Example:\n"
            "  projot set-app-id --app-id NewApp --force\n";
        return 0;
    }

    if (!args.has("app-id")) {
        std::cerr << "error: --app-id is required. "
                     "Run 'projot set-app-id --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }

    const std::string new_app_id = args.get("app-id");
    const bool force = args.has("force");

    return execute_config_command(ctx, [&new_app_id, force](Context& c) {
        if (!c.config.app_id.empty() && !force) {
            return ParseResult{false, "app_id is already set to '" + c.config.app_id + "'. Use --force to overwrite."};
        }
        c.config.app_id = new_app_id;
        return ParseResult{true, ""};
    }, true, "Set app_id = " + new_app_id);
}

// add-github / add-swagger / add-blizzard

struct UrlListDef {
    const char* key;
    std::vector<std::string> Config::* field;
};

static const UrlListDef URL_LIST_TYPES[] = {
    {"github",   &Config::github},
    {"swagger",  &Config::swagger},
    {"blizzard", &Config::blizzard},
};

static const UrlListDef* find_url_list_type(const std::string& key) {
    for (const auto& t : URL_LIST_TYPES) {
        if (t.key == key) return &t;
    }
    return nullptr;
}

static int cmd_add_url(const Args& args, const std::string& kind) {
    if (args.help_requested) {
        std::cout << "Usage: projot add-" << kind << " --url <URL>\n\n"
                  << "Add a " << kind << " URL to the project config.\n\n"
                  << "Required:\n"
                  << "  --url <URL>   URL to add\n\n"
                  << "Example:\n"
                  << "  projot add-" << kind << " --url https://example.com\n";
        return 0;
    }

    if (!args.has("url")) {
        std::cerr << "error: --url is required. "
                  << "Run 'projot add-" << kind << " --help' for usage.\n";
        return 1;
    }

    const UrlListDef* tdef = find_url_list_type(kind);
    if (!tdef) {
        std::cerr << "error: unknown URL list kind '" << kind << "'\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    const std::string url = args.get("url");

    return execute_config_command(ctx, [tdef, &url, &kind](Context& c) {
        auto& list = c.config.*tdef->field;

        if (std::find(list.begin(), list.end(), url) != list.end()) {
            std::cout << "URL already present (skipped).\n";
            return ParseResult{false, "already_present"};
        }
        list.push_back(url);
        std::cout << "Added " << kind << " URL: " << url << "\n";
        return ParseResult{true, ""};
    });
}

int cmd_add_github(const Args& args)   { return cmd_add_url(args, "github");   }
int cmd_add_swagger(const Args& args)  { return cmd_add_url(args, "swagger");  }
int cmd_add_blizzard(const Args& args) { return cmd_add_url(args, "blizzard"); }

// add-azure

struct AzureTypeDef {
    const char* key;
    const char* label;
    std::vector<std::string> Config::* field;
};

static const AzureTypeDef AZURE_TYPES[] = {
    {"subscription",   "Subscriptions",      &Config::azure_subscription},
    {"key-vault",      "Key Vaults",         &Config::azure_key_vault},
    {"resource-group", "Resource Groups",    &Config::azure_resource_group},
    {"aks",            "AKS Clusters",       &Config::azure_aks},
    {"log-analytics",  "Log Analytics",      &Config::azure_log_analytics},
    {"storage",        "Storage Containers", &Config::azure_storage},
    {"private-dns",    "Private DNS Zones",  &Config::azure_private_dns},
};

static const AzureTypeDef* find_azure_type(const std::string& key) {
    for (const auto& t : AZURE_TYPES) {
        if (t.key == key) return &t;
    }
    return nullptr;
}

int cmd_add_azure(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot add-azure --type <type> --url <URL> [--name <name>]\n\n"
            "Add an Azure resource entry to the project config.\n"
            "Run the command once per resource to build up a list.\n\n"
            "Required:\n"
            "  --type <type>   Resource type. One of:\n"
            "                    subscription, key-vault, resource-group,\n"
            "                    aks, log-analytics, storage, private-dns\n"
            "  --url <URL>     URL to the resource or feature in Azure portal\n\n"
            "Optional:\n"
            "  --name <name>   Human-readable resource name\n\n"
            "Examples:\n"
            "  # Add two subscriptions (run once per subscription)\n"
            "  projot add-azure --type subscription --name \"PROD\" "
                "--url https://portal.azure.com/#@tenant/subscriptions/prod-id\n"
            "  projot add-azure --type subscription --name \"NPRD\" "
                "--url https://portal.azure.com/#@tenant/subscriptions/nprd-id\n\n"
            "  # Add a general management link (no name needed)\n"
            "  projot add-azure --type private-dns "
                "--url https://portal.azure.com/...\n";
        return 0;
    }

    if (!args.has("type") || !args.has("url")) {
        std::cerr << "error: --type and --url are required. "
                     "Run 'projot add-azure --help' for usage.\n";
        return 1;
    }

    const std::string type_key = args.get("type");
    const AzureTypeDef* tdef = find_azure_type(type_key);
    if (!tdef) {
        std::cerr << "error: unknown Azure resource type '" << type_key << "'. "
                     "Valid types: subscription, key-vault, resource-group, "
                     "aks, log-analytics, storage, private-dns\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    const AzureEntry entry{args.get("name"), args.get("url")};
    const std::string raw = format_azure_entry(entry);
    const std::string label = tdef->label;
    auto field_ptr = tdef->field;

    return execute_config_command(ctx, [&raw, field_ptr](Context& c) {
        auto& list = c.config.*field_ptr;
        if (std::find(list.begin(), list.end(), raw) != list.end()) {
            return ParseResult{false, "already_present"};
        }
        list.push_back(raw);
        return ParseResult{true, ""};
    }, true, "Added Azure " + label + ": " + raw);
}

// set-global

int cmd_set_global(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot set-global [options]\n\n"
            "Set global defaults for base URLs. These are used by projot across all projects.\n"
            "Values can be overridden at the repo level in .projot/config if needed.\n\n"
            "Options:\n"
            "  --rpm-base-url <url>    Base URL for RPM links (project number is appended)\n"
            "  --itrack-base-url <url> Base URL for iTrack links (ticket number is appended)\n\n"
            "At least one of the above options is required.\n\n"
            "Examples:\n"
            "  projot set-global --rpm-base-url https://rpm.example.com/\n"
            "  projot set-global --itrack-base-url https://itrack.example.com/record/\n";
        return 0;
    }

    if (!args.has("rpm-base-url") && !args.has("itrack-base-url")) {
        std::cerr << "error: --rpm-base-url or --itrack-base-url required.\n";
        return 1;
    }

    auto path = global_config_path();
    if (!path) {
        std::cerr << "error: cannot determine global config path ($HOME not set).\n";
        return 1;
    }

    Config cfg;
    parse_config(path->string(), cfg); // silently ok if file missing

    if (args.has("rpm-base-url"))
        cfg.rpm_base_url = args.get("rpm-base-url");
    if (args.has("itrack-base-url"))
        cfg.itrack_base_url = args.get("itrack-base-url");

    auto result = write_global_config(path->string(), cfg);
    if (!result.ok) {
        std::cerr << "error: " << result.error << "\n";
        return 1;
    }

    std::cout << "Updated global config: " << path->string() << "\n";
    return 0;
}

// set-teams-webhook

int cmd_set_teams_webhook(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot set-teams-webhook <URL>\n\n"
            "Set the Teams incoming webhook URL for Kanban sync.\n\n"
            "The webhook URL is created in Teams channel settings under\n"
            "Connectors > Incoming Webhook. It is stored in .projot/config.\n\n"
            "Note: .projot/config is git-tracked. For public repos, avoid\n"
            "committing a webhook URL, or gitignore .projot/config.\n\n"
            "Example:\n"
            "  projot set-teams-webhook https://xxx.webhook.office.com/webhookb2/...\n";
        return 0;
    }

    if (args.positional.empty()) {
        std::cerr << "error: webhook URL is required. "
                     "Run 'projot set-teams-webhook --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    const std::string url = args.positional[0];

    return execute_config_command(ctx, [&url](Context& c) {
        c.config.teams_webhook = url;
        return ParseResult{true, ""};
    }, false, "Set teams_webhook = " + url);
}
