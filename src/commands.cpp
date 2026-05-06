#include "commands.h"
#include "config.h"
#include "repo.h"
#include "todo.h"
#include "markdown.h"
#include "renderer.h"
#include "utils.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <cstdlib>   // std::system — required for `git add` (no std alternative)
#include <optional>

#ifdef __APPLE__
#  include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

// ── Internal context ──────────────────────────────────────────────────────────

struct Context {
    fs::path repo_root;
    Config   config;
    bool     ok    = true;
    std::string error;
};

// Load repo root + config. Validates config_version.
static Context load_context() {
    Context ctx;

    auto root = find_repo_root();
    if (!root) {
        ctx.ok = false;
        ctx.error = "not inside a git repository. projot must be run from within a git repo.";
        return ctx;
    }
    ctx.repo_root = *root;

    fs::path cfg_path = ctx.repo_root / ".projot" / "config";
    if (!fs::exists(cfg_path)) {
        ctx.ok = false;
        ctx.error = "no .projot/config found. Run 'projot init' first.";
        return ctx;
    }

    auto result = parse_config(cfg_path.string(), ctx.config);
    if (!result.ok) {
        ctx.ok = false;
        ctx.error = result.error;
        return ctx;
    }

    if (ctx.config.config_version > PROJOT_CONFIG_SCHEMA_VERSION) {
        ctx.ok = false;
        ctx.error = "config_version " + std::to_string(ctx.config.config_version) +
                    " is newer than this binary supports (max: " +
                    std::to_string(PROJOT_CONFIG_SCHEMA_VERSION) +
                    "). Please upgrade projot.";
        return ctx;
    }
    if (ctx.config.config_version == 0) {
        std::cerr << "warning: config_version missing; treating as version 0.\n";
    }

    return ctx;
}

static std::string projot_file_path(const Context& ctx, const std::string& filename) {
    return (ctx.repo_root / ".projot" / filename).string();
}

// Execute a command that modifies a project's todos and re-renders the file.
// Takes a callback that modifies the Project and returns the result/message.
// If success_msg is non-empty, prints it after successful render.
// Special case: error message "already_completed" returns 0 (warning, not error).
// Returns exit code (0 = success, 1 = error).
using ProjectModifier = std::function<ParseResult(Project&)>;
static int execute_project_command(const Context& ctx,
                                    ProjectModifier modifier,
                                    const std::string& success_msg = "") {
    Project proj;
    auto parse = parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    auto result = modifier(proj);
    if (!result.ok) {
        // Special case: "already_completed" is a warning, not an error
        if (result.error != "already_completed") {
            std::cerr << "error: " << result.error << "\n";
            return 1;
        }
        return 0;
    }

    auto render = render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, proj.todos);
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    if (!success_msg.empty()) std::cout << success_msg << "\n";
    return 0;
}

// Verifies that a project is configured and the notes file exists.
static bool require_project(const Context& ctx) {
    if (ctx.config.rpm.empty()) {
        std::cerr << "error: no project configured. Run 'projot new' first.\n";
        return false;
    }
    fs::path notes = ctx.repo_root / ".projot" / (ctx.config.rpm + ".md");
    if (!fs::exists(notes)) {
        std::cerr << "error: notes file " << notes.string()
                  << " not found. Run 'projot new' first.\n";
        return false;
    }
    return true;
}

// Validate that RPM is safe to embed in a shell command.
static bool is_safe_rpm(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
            return false;
    }
    return true;
}

// ── Hook installation ─────────────────────────────────────────────────────────

static const std::string HOOK_BLOCK =
    "# projot: regenerate notes file before commit\n"
    "if command -v projot >/dev/null 2>&1; then\n"
    "    projot render\n"
    "fi\n";

// Returns true on success. Sets `appended=true` if content was appended to an
// existing hook rather than written fresh. Idempotent: does nothing if the block
// is already present.
static bool install_hook_impl(const fs::path& repo_root,
                               bool& appended,
                               std::string& error) {
    appended = false;
    auto hooks_dir = repo_root / ".git" / "hooks";
    auto hook_path = hooks_dir / "pre-commit";

    std::error_code ec;
    if (!fs::exists(hooks_dir, ec)) {
        fs::create_directories(hooks_dir, ec);
        if (ec) { error = "cannot create hooks directory: " + ec.message(); return false; }
    }

    bool exists = fs::exists(hook_path, ec);
    if (exists) {
        std::ifstream in(hook_path);
        std::string content((std::istreambuf_iterator<char>(in)), {});
        in.close();

        // Idempotent: block already present
        if (content.find("projot render") != std::string::npos)
            return true;

        std::ofstream f(hook_path, std::ios::app);
        if (!f.is_open()) { error = "cannot append to " + hook_path.string(); return false; }
        f << "\n" << HOOK_BLOCK;
        appended = true;
    } else {
        std::ofstream f(hook_path);
        if (!f.is_open()) { error = "cannot create " + hook_path.string(); return false; }
        f << "#!/bin/sh\n" << HOOK_BLOCK;
    }

#ifndef _WIN32
    fs::permissions(hook_path,
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
        fs::perm_options::add, ec);
    if (ec) { error = "cannot set executable permission: " + ec.message(); return false; }
#endif

    return true;
}

// ── MCP helpers ───────────────────────────────────────────────────────────────

// Returns the path to the directory containing the running binary, or nullopt.
static std::optional<fs::path> binary_dir() {
#if defined(__linux__)
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return exe.parent_path();
#elif defined(__APPLE__)
    // Query required buffer size first, then allocate.
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) == 0) {
        std::error_code ec;
        fs::path exe = fs::canonical(buf.c_str(), ec);
        if (!ec) return exe.parent_path();
    }
#endif
    return std::nullopt;
}

// Searches for the bundled MCP directory relative to the binary location.
// Checks:
//   <bin_dir>/../mcp/                (source-tree or single-dir install)
//   <bin_dir>/../share/projot/mcp/   (standard install prefix layout)
static std::optional<fs::path> find_mcp_source_dir() {
    auto dir = binary_dir();
    if (!dir) return std::nullopt;

    for (const fs::path& raw : {
            *dir / ".." / "mcp",
            *dir / ".." / "share" / "projot" / "mcp"}) {
        std::error_code ec;
        fs::path candidate = fs::weakly_canonical(raw, ec);
        if (!ec && fs::exists(candidate / "server.js", ec))
            return candidate;
    }
    return std::nullopt;
}

// ── init ──────────────────────────────────────────────────────────────────────

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

// ── new ───────────────────────────────────────────────────────────────────────

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

    auto save = write_config(projot_file_path(ctx, "config"), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    auto render = render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, {});
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

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

// ── add-todo ──────────────────────────────────────────────────────────────────

int cmd_add_todo(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot add-todo \"<description>\"\n\n"
            "Append a new todo to the project notes file.\n\n"
            "Required:\n"
            "  \"<description>\"   Text of the new todo\n\n"
            "Example:\n"
            "  projot add-todo \"Validate index rebuild plan\"\n";
        return 0;
    }

    if (args.positional.empty()) {
        std::cerr << "error: todo text is required. Run 'projot add-todo --help' for usage.\n";
        return 1;
    }
    std::string text = args.positional[0];

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    return execute_project_command(ctx, [&](Project& proj) {
        Todo t;
        t.id           = next_todo_id(proj.todos);
        t.text         = text;
        t.created_date = date_today();
        proj.todos.push_back(t);
        std::cout << "Added todo " << t.id << ": " << t.text << "\n";
        return ParseResult{true, ""};
    });
}

// ── list ─────────────────────────────────────────────────────────────────────

int cmd_list(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot list [--open | --closed | --all]\n\n"
            "Display project summary and todos.\n\n"
            "Optional:\n"
            "  --open    Show open todos only (default)\n"
            "  --closed  Show closed todos only\n"
            "  --all     Show all todos\n\n"
            "Example:\n"
            "  projot list\n"
            "  projot list --all\n";
        return 0;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    TodoFilter filter = TodoFilter::Open;
    if (args.has("closed")) filter = TodoFilter::Closed;
    else if (args.has("all")) filter = TodoFilter::All;

    std::cout << "Project: " << ctx.config.name
              << "  |  RPM: " << ctx.config.rpm
              << "  |  iTrack: " << (ctx.config.itrack.empty() ? "N/A" : ctx.config.itrack)
              << "\n\n";

    auto todos = filter_todos(proj.todos, filter);
    if (todos.empty()) {
        std::cout << "(no todos)\n";
    } else {
        for (const auto* t : todos) {
            std::cout << t->id << ". "
                      << (t->completed ? "[x]" : "[ ]")
                      << " " << t->text << "\n";
        }
    }
    return 0;
}

// ── complete ─────────────────────────────────────────────────────────────────

int cmd_complete(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot complete --todo <ID>\n\n"
            "Mark a todo as completed.\n\n"
            "Required:\n"
            "  --todo <ID>   Stable numeric todo ID\n\n"
            "Example:\n"
            "  projot complete --todo 1\n";
        return 0;
    }

    if (!args.has("todo")) {
        std::cerr << "error: --todo is required. Run 'projot complete --help' for usage.\n";
        return 1;
    }

    int id;
    try { id = std::stoi(args.get("todo")); }
    catch (...) { std::cerr << "error: --todo must be a number.\n"; return 1; }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    return execute_project_command(ctx, [id](Project& proj) {
        if (!find_todo(proj.todos, id)) {
            std::string msg = "todo " + std::to_string(id) + " not found.";
            if (!proj.todos.empty()) {
                msg += " Valid IDs:";
                for (const auto& t : proj.todos) msg += " " + std::to_string(t.id);
            }
            return ParseResult{false, msg};
        }

        auto result = complete_todo(proj.todos, id, date_today());
        if (result.warned) {
            std::cerr << "warning: " << result.message << "\n";
            // Don't modify the project if warning occurred
            return ParseResult{false, "already_completed"};
        }

        std::cout << "Completed todo " << id << ".\n";
        return ParseResult{true, ""};
    });
}

// ── add-note ─────────────────────────────────────────────────────────────────

int cmd_add_note(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot add-note --todo <ID> \"<note>\"\n\n"
            "Add a note to a todo.\n\n"
            "Required:\n"
            "  --todo <ID>    Stable numeric todo ID\n"
            "  \"<note>\"       Note text\n\n"
            "Example:\n"
            "  projot add-note --todo 1 \"Waiting on supervisor feedback\"\n";
        return 0;
    }

    if (!args.has("todo")) {
        std::cerr << "error: --todo is required. "
                     "Run 'projot add-note --help' for usage.\n";
        return 1;
    }

    if (args.positional.empty()) {
        std::cerr << "error: note text is required. Run 'projot add-note --help' for usage.\n";
        return 1;
    }

    int id;
    try { id = std::stoi(args.get("todo")); }
    catch (...) { std::cerr << "error: --todo must be a number.\n"; return 1; }

    std::string text = args.positional[0];

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    return execute_project_command(ctx, [id, &text](Project& proj) {
        if (!find_todo(proj.todos, id)) {
            std::string msg = "todo " + std::to_string(id) + " not found.";
            if (!proj.todos.empty()) {
                msg += " Valid IDs:";
                for (const auto& t : proj.todos) msg += " " + std::to_string(t.id);
            }
            return ParseResult{false, msg};
        }

        auto result = add_note(proj.todos, id, text);
        if (result.warned) std::cerr << "warning: " << result.message << "\n";

        std::cout << "Added note to todo " << id << ".\n";
        return ParseResult{true, ""};
    });
}

// ── set-link ─────────────────────────────────────────────────────────────────

int cmd_set_link(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot set-link --key <key> --url <URL>\n\n"
            "Set or update a single-value link URL.\n\n"
            "Required:\n"
            "  --key <key>   Link key (e.g. teams, itrack, rpm, other)\n"
            "  --url <URL>   URL value\n\n"
            "Example:\n"
            "  projot set-link --key teams --url https://teams.microsoft.com/...\n";
        return 0;
    }

    if (!args.has("key") || !args.has("url")) {
        std::cerr << "error: --key and --url are required. "
                     "Run 'projot set-link --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    const std::string key = args.get("key");
    ctx.config.link_urls[key] = args.get("url");

    if (std::find(ctx.config.links.begin(), ctx.config.links.end(), key)
            == ctx.config.links.end()) {
        ctx.config.links.push_back(key);
    }

    auto save = write_config(projot_file_path(ctx, "config"), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    // Re-render notes to pick up the updated link
    Project proj;
    if (parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj).ok)
        render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, proj.todos);

    std::cout << "Set link " << key << " = " << args.get("url") << "\n";
    return 0;
}

// ── set-app-id ───────────────────────────────────────────────────────────────

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

    if (!ctx.config.app_id.empty() && !args.has("force")) {
        std::cerr << "error: app_id is already set to '" << ctx.config.app_id
                  << "'. Use --force to overwrite.\n";
        return 1;
    }

    ctx.config.app_id = args.get("app-id");

    auto save = write_config(projot_file_path(ctx, "config"), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    // Re-render notes if a project exists
    if (!ctx.config.rpm.empty()) {
        fs::path notes = ctx.repo_root / ".projot" / (ctx.config.rpm + ".md");
        if (fs::exists(notes)) {
            Project proj;
            if (parse_markdown(notes.string(), proj).ok)
                render_to_file(notes.string(), ctx.config, proj.todos);
        }
    }

    std::cout << "Set app_id = " << ctx.config.app_id << "\n";
    return 0;
}

// ── add-github / add-swagger / add-blizzard ───────────────────────────────────

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

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    auto& list = (kind == "github")  ? ctx.config.github  :
                 (kind == "swagger") ? ctx.config.swagger : ctx.config.blizzard;

    const std::string url = args.get("url");
    if (std::find(list.begin(), list.end(), url) != list.end()) {
        std::cout << "URL already present (skipped).\n";
        return 0;
    }
    list.push_back(url);

    auto save = write_config(projot_file_path(ctx, "config"), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    Project proj;
    if (parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj).ok)
        render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, proj.todos);

    std::cout << "Added " << kind << " URL: " << url << "\n";
    return 0;
}

int cmd_add_github(const Args& args)   { return cmd_add_url(args, "github");   }
int cmd_add_swagger(const Args& args)  { return cmd_add_url(args, "swagger");  }
int cmd_add_blizzard(const Args& args) { return cmd_add_url(args, "blizzard"); }

// ── add-azure ─────────────────────────────────────────────────────────────────

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

    AzureEntry entry;
    entry.name = args.get("name");
    entry.url  = args.get("url");
    const std::string raw = format_azure_entry(entry);

    auto& list = ctx.config.*tdef->field;
    if (std::find(list.begin(), list.end(), raw) != list.end()) {
        std::cout << "Entry already present (skipped).\n";
        return 0;
    }
    list.push_back(raw);

    auto save = write_config(projot_file_path(ctx, "config"), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    Project proj;
    if (parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj).ok)
        render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, proj.todos);

    std::cout << "Added Azure " << tdef->label << ": " << raw << "\n";
    return 0;
}

// ── render ────────────────────────────────────────────────────────────────────

int cmd_render(const Args& args) {
    (void)args;

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    auto render = render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, proj.todos);
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    // Stage the rendered file. RPM is validated before embedding in the command.
    if (is_safe_rpm(ctx.config.rpm)) {
        std::string cmd = "git -C \"" + ctx.repo_root.string() +
                          "\" add .projot/" + ctx.config.rpm + ".md 2>/dev/null";
        int rc = std::system(cmd.c_str()); (void)rc;
    }

    return 0;
}

// ── install-hook ──────────────────────────────────────────────────────────────

int cmd_install_hook(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot install-hook\n\n"
            "Install or re-install the pre-commit git hook.\n\n"
            "No flags required.\n\n"
            "Example:\n"
            "  projot install-hook\n";
        return 0;
    }

    auto root = find_repo_root();
    if (!root) {
        std::cerr << "error: not inside a git repository.\n";
        return 1;
    }

    bool appended = false;
    std::string error;
    if (!install_hook_impl(*root, appended, error)) {
        std::cerr << "warning: could not install pre-commit hook: " << error << "\n";
        return 1;
    }

    if (appended) {
        std::cout << "Note: appended projot render block to existing "
                     ".git/hooks/pre-commit\n";
    } else {
        std::cout << "Installed pre-commit hook at "
                  << (*root / ".git" / "hooks" / "pre-commit").string() << "\n";
    }
    return 0;
}

// ── install-mcp-server ────────────────────────────────────────────────────────

// Writes or updates .claude/settings.json with the projot MCP server config.
// Returns true on success or idempotent "already done" cases.
// Returns false on hard error (sets error string).
// If manual action is required, sets manual_action to the instruction text and returns true.
static bool install_claude_mcp(const fs::path& repo_root,
                                std::string& error,
                                std::string& manual_action) {
    fs::path claude_dir = repo_root / ".claude";
    fs::path settings_file = claude_dir / "settings.json";
    std::error_code ec;

    // Sentinel for "projot is already configured"
    const std::string projot_sentinel = "\"projot\"";

    // Case 1: File does not exist — create directory and write fresh config
    if (!fs::exists(settings_file, ec)) {
        fs::create_directories(claude_dir, ec);
        if (ec) { error = "cannot create .claude/: " + ec.message(); return false; }

        std::ofstream f(settings_file);
        if (!f.is_open()) { error = "cannot write .claude/settings.json"; return false; }

        f << "{\n"
          << "  \"mcpServers\": {\n"
          << "    \"projot\": {\n"
          << "      \"command\": \"node\",\n"
          << "      \"args\": [\"./mcp/server.js\"]\n"
          << "    }\n"
          << "  }\n"
          << "}\n";
        return true;
    }

    // Case 2+: File exists — check if already configured
    std::ifstream f_read(settings_file);
    if (!f_read.is_open()) { error = "cannot read .claude/settings.json"; return false; }

    std::string content((std::istreambuf_iterator<char>(f_read)),
                        std::istreambuf_iterator<char>());
    f_read.close();

    // Check if projot is already configured
    if (content.find(projot_sentinel) != std::string::npos) {
        // Already done — idempotent
        return true;
    }

    // Case 3: File has mcpServers but no projot entry — manual instruction
    if (content.find("\"mcpServers\"") != std::string::npos) {
        manual_action = "note: .claude/settings.json already has an mcpServers block.\n"
                       "Add the following entry to it manually:\n\n"
                       "    \"projot\": {\n"
                       "      \"command\": \"node\",\n"
                       "      \"args\": [\"./mcp/server.js\"]\n"
                       "    }\n";
        return true;
    }

    // Case 4: File has no mcpServers — inject the full block before the last }
    size_t last_brace = content.rfind('}');
    if (last_brace == std::string::npos) {
        error = ".claude/settings.json is malformed (no closing brace)";
        return false;
    }

    std::string to_inject = ",\n  \"mcpServers\": {\n"
                           "    \"projot\": {\n"
                           "      \"command\": \"node\",\n"
                           "      \"args\": [\"./mcp/server.js\"]\n"
                           "    }\n"
                           "  }";

    content.insert(last_brace, to_inject);

    std::ofstream f_write(settings_file);
    if (!f_write.is_open()) { error = "cannot write .claude/settings.json"; return false; }
    f_write << content;
    return true;
}

// Check if Node.js is available on PATH
static bool node_available() {
    return std::system("node --version > /dev/null 2>&1") == 0;
}

int cmd_install_mcp_server(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot install-mcp-server [options]\n\n"
            "Configure the projot MCP server for use with Claude Code and VS Code.\n\n"
            "Writes .claude/settings.json (Claude Code) and .vscode/mcp.json (VS Code)\n"
            "with the server configuration. Skips any file that is already configured.\n\n"
            "Requires Node.js 16+ to be installed for the MCP server to run.\n\n"
            "Optional:\n"
            "  --no-vscode    Skip VS Code (.vscode/mcp.json) configuration\n\n"
            "Example:\n"
            "  projot install-mcp-server\n";
        return 0;
    }

    auto root = find_repo_root();
    if (!root) {
        std::cerr << "error: not inside a git repository.\n";
        return 1;
    }

    // Warn if Node.js is not available (non-fatal)
    if (!node_available()) {
        std::cerr << "warning: Node.js not found on PATH. MCP server requires Node.js 16+.\n"
                  << "         Install Node.js and try again, or see mcp/README.md for manual setup.\n";
    }

    // Ensure mcp/server.js exists in the repo
    fs::path mcp_in_repo = *root / "mcp" / "server.js";
    if (!fs::exists(mcp_in_repo)) {
        // Try to copy from install location
        auto mcp_src = find_mcp_source_dir();
        if (!mcp_src) {
            std::cerr << "error: mcp/server.js not found and cannot locate bundled MCP files.\n"
                      << "       ensure projot was installed correctly.\n";
            return 1;
        }

        fs::path mcp_dest = *root / "mcp";
        std::error_code ec;
        fs::create_directories(mcp_dest, ec);
        if (ec) {
            std::cerr << "error: cannot create mcp/: " << ec.message() << "\n";
            return 1;
        }

        for (const char* fname : {"server.js", "package.json"}) {
            fs::path src_file = *mcp_src / fname;
            if (!fs::exists(src_file, ec)) {
                std::cerr << "warning: expected MCP file not found: " << src_file.string() << "\n";
                continue;
            }
            fs::copy_file(src_file, mcp_dest / fname,
                         fs::copy_options::overwrite_existing, ec);
            if (ec) {
                std::cerr << "error: cannot copy " << fname << ": " << ec.message() << "\n";
                return 1;
            }
        }
    }

    // Configure .claude/settings.json
    std::string error, manual_action;
    if (!install_claude_mcp(*root, error, manual_action)) {
        std::cerr << "error: " << error << "\n";
        return 1;
    }

    if (!manual_action.empty()) {
        std::cout << manual_action;
    } else {
        std::cout << "Configured MCP server in .claude/settings.json\n";
    }

    // Configure .vscode/mcp.json unless --no-vscode
    if (!args.has("no-vscode")) {
        fs::path vscode_dir = *root / ".vscode";
        fs::path mcp_json = vscode_dir / "mcp.json";
        std::error_code ec;

        if (!fs::exists(mcp_json, ec)) {
            fs::create_directories(vscode_dir, ec);
            if (ec) {
                std::cerr << "error: cannot create .vscode/: " << ec.message() << "\n";
                return 1;
            }

            std::ofstream f(mcp_json);
            if (!f.is_open()) {
                std::cerr << "error: cannot write .vscode/mcp.json\n";
                return 1;
            }

            f << "{\n"
              << "  \"inputs\": [],\n"
              << "  \"servers\": {\n"
              << "    \"projot\": {\n"
              << "      \"type\": \"stdio\",\n"
              << "      \"command\": \"node\",\n"
              << "      \"args\": [\"./mcp/server.js\"]\n"
              << "    }\n"
              << "  }\n"
              << "}\n";

            std::cout << "Configured MCP server in .vscode/mcp.json\n";
        } else {
            std::cout << "VS Code MCP configuration already exists in .vscode/mcp.json\n";
        }
    }

    std::cout << "MCP server is ready. Reload your editor to enable tab completion and tools.\n";
    return 0;
}
