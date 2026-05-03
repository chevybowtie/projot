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
#include <cstdlib>   // std::system — required for `git add` (no std alternative)

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

static std::string config_path_str(const Context& ctx) {
    return (ctx.repo_root / ".projot" / "config").string();
}

static std::string notes_path_str(const Context& ctx) {
    return (ctx.repo_root / ".projot" / (ctx.config.ranp + ".md")).string();
}

// Verifies that a project is configured and the notes file exists.
static bool require_project(const Context& ctx) {
    if (ctx.config.ranp.empty()) {
        std::cerr << "error: no project configured. Run 'projot new' first.\n";
        return false;
    }
    fs::path notes = ctx.repo_root / ".projot" / (ctx.config.ranp + ".md");
    if (!fs::exists(notes)) {
        std::cerr << "error: notes file " << notes.string()
                  << " not found. Run 'projot new' first.\n";
        return false;
    }
    return true;
}

// Validate that RANP is safe to embed in a shell command.
static bool is_safe_ranp(const std::string& s) {
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
            "Usage: projot new --ranp <RANP> --name \"<Name>\" --itrack <iTrack> [options]\n\n"
            "Start a new RANP project in this repository.\n\n"
            "Required:\n"
            "  --ranp <RANP>             RANP project number\n"
            "  --name \"<Project Name>\"   Human-readable project name\n"
            "  --itrack <iTrack>         iTrack ticket number\n\n"
            "Optional:\n"
            "  --teams <URL>             Teams channel URL\n"
            "  --ranp-url <URL>          RANP system link\n"
            "  --itrack-url <URL>        iTrack link\n"
            "  --other <URL>             Other URL\n"
            "  --no-hook                 Skip pre-commit hook installation\n\n"
            "Example:\n"
            "  projot new --ranp 12345 --name \"Widget Redesign\" --itrack 67890\n";
        return 0;
    }

    if (!args.has("ranp") || !args.has("name") || !args.has("itrack")) {
        std::cerr << "error: --ranp, --name, and --itrack are required. "
                     "Run 'projot new --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }

    if (!ctx.config.ranp.empty()) {
        std::cerr << "error: a project (RANP " << ctx.config.ranp
                  << ") is already configured.\n";
        return 1;
    }

    ctx.config.ranp    = args.get("ranp");
    ctx.config.name    = args.get("name");
    ctx.config.itrack  = args.get("itrack");
    ctx.config.created = date_today();

    // Build links list from provided optional flags
    struct LinkDef { std::string key, label, flag; };
    for (const auto& ld : std::vector<LinkDef>{
            {"teams",  "Teams",  "teams"},
            {"itrack", "iTrack", "itrack-url"},
            {"ranp",   "RANP",   "ranp-url"},
            {"other",  "Other",  "other"},
        }) {
        if (args.has(ld.flag)) {
            ctx.config.links.push_back(ld.key);
            ctx.config.labels[ld.key]    = ld.label;
            ctx.config.link_urls[ld.key] = args.get(ld.flag);
        }
    }

    auto save = write_config(config_path_str(ctx), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    auto render = render_to_file(notes_path_str(ctx), ctx.config, {});
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    std::cout << "Created project " << ctx.config.name
              << " (RANP " << ctx.config.ranp << ")\n";

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
            "Usage: projot add-todo --text \"<description>\"\n\n"
            "Append a new todo to the project notes file.\n\n"
            "Required:\n"
            "  --text \"<description>\"   Text of the new todo\n\n"
            "Example:\n"
            "  projot add-todo --text \"Validate index rebuild plan\"\n";
        return 0;
    }

    if (!args.has("text")) {
        std::cerr << "error: --text is required. Run 'projot add-todo --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(notes_path_str(ctx), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    Todo t;
    t.id           = next_todo_id(proj.todos);
    t.text         = args.get("text");
    t.created_date = date_today();
    proj.todos.push_back(t);

    auto render = render_to_file(notes_path_str(ctx), ctx.config, proj.todos);
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    std::cout << "Added todo " << t.id << ": " << t.text << "\n";
    return 0;
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
    auto parse = parse_markdown(notes_path_str(ctx), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    TodoFilter filter = TodoFilter::Open;
    if (args.has("closed")) filter = TodoFilter::Closed;
    else if (args.has("all")) filter = TodoFilter::All;

    std::cout << "Project: " << ctx.config.name
              << "  |  RANP: " << ctx.config.ranp
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

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(notes_path_str(ctx), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    int id;
    try { id = std::stoi(args.get("todo")); }
    catch (...) { std::cerr << "error: --todo must be a number.\n"; return 1; }

    if (!find_todo(proj.todos, id)) {
        std::cerr << "error: todo " << id << " not found.";
        if (!proj.todos.empty()) {
            std::cerr << " Valid IDs:";
            for (const auto& t : proj.todos) std::cerr << " " << t.id;
        }
        std::cerr << "\n";
        return 1;
    }

    auto result = complete_todo(proj.todos, id, date_today());
    if (result.warned) {
        std::cerr << "warning: " << result.message << "\n";
        return 0;
    }

    auto render = render_to_file(notes_path_str(ctx), ctx.config, proj.todos);
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    std::cout << "Completed todo " << id << ".\n";
    return 0;
}

// ── add-note ─────────────────────────────────────────────────────────────────

int cmd_add_note(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot add-note --todo <ID> --text \"<note>\"\n\n"
            "Add a note to a todo.\n\n"
            "Required:\n"
            "  --todo <ID>      Stable numeric todo ID\n"
            "  --text \"<note>\"  Note text\n\n"
            "Example:\n"
            "  projot add-note --todo 1 --text \"Waiting on supervisor feedback\"\n";
        return 0;
    }

    if (!args.has("todo") || !args.has("text")) {
        std::cerr << "error: --todo and --text are required. "
                     "Run 'projot add-note --help' for usage.\n";
        return 1;
    }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(notes_path_str(ctx), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    int id;
    try { id = std::stoi(args.get("todo")); }
    catch (...) { std::cerr << "error: --todo must be a number.\n"; return 1; }

    if (!find_todo(proj.todos, id)) {
        std::cerr << "error: todo " << id << " not found.";
        if (!proj.todos.empty()) {
            std::cerr << " Valid IDs:";
            for (const auto& t : proj.todos) std::cerr << " " << t.id;
        }
        std::cerr << "\n";
        return 1;
    }

    auto result = add_note(proj.todos, id, args.get("text"));
    if (result.warned) std::cerr << "warning: " << result.message << "\n";

    auto render = render_to_file(notes_path_str(ctx), ctx.config, proj.todos);
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    std::cout << "Added note to todo " << id << ".\n";
    return 0;
}

// ── set-link ─────────────────────────────────────────────────────────────────

int cmd_set_link(const Args& args) {
    if (args.help_requested) {
        std::cout <<
            "Usage: projot set-link --key <key> --url <URL>\n\n"
            "Set or update a single-value link URL.\n\n"
            "Required:\n"
            "  --key <key>   Link key (e.g. teams, itrack, ranp, other)\n"
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

    auto save = write_config(config_path_str(ctx), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    // Re-render notes to pick up the updated link
    Project proj;
    if (parse_markdown(notes_path_str(ctx), proj).ok)
        render_to_file(notes_path_str(ctx), ctx.config, proj.todos);

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

    auto save = write_config(config_path_str(ctx), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    // Re-render notes if a project exists
    if (!ctx.config.ranp.empty()) {
        fs::path notes = ctx.repo_root / ".projot" / (ctx.config.ranp + ".md");
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

    auto save = write_config(config_path_str(ctx), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    Project proj;
    if (parse_markdown(notes_path_str(ctx), proj).ok)
        render_to_file(notes_path_str(ctx), ctx.config, proj.todos);

    std::cout << "Added " << kind << " URL: " << url << "\n";
    return 0;
}

int cmd_add_github(const Args& args)   { return cmd_add_url(args, "github");   }
int cmd_add_swagger(const Args& args)  { return cmd_add_url(args, "swagger");  }
int cmd_add_blizzard(const Args& args) { return cmd_add_url(args, "blizzard"); }

// ── render ────────────────────────────────────────────────────────────────────

int cmd_render(const Args& args) {
    (void)args;

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    Project proj;
    auto parse = parse_markdown(notes_path_str(ctx), proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }

    auto render = render_to_file(notes_path_str(ctx), ctx.config, proj.todos);
    if (!render.ok) { std::cerr << "error: " << render.error << "\n"; return 1; }

    // Stage the rendered file. RANP is validated before embedding in the command.
    if (is_safe_ranp(ctx.config.ranp)) {
        std::string cmd = "git -C \"" + ctx.repo_root.string() +
                          "\" add .projot/" + ctx.config.ranp + ".md 2>/dev/null";
        (void)std::system(cmd.c_str()); // return value intentionally ignored
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
