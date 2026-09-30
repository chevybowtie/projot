#include "commands_internal.h"
#include "config.h"
#include "markdown.h"
#include "renderer.h"
#include "repo.h"
#include "utils.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// Load repo root + config. Validates config_version.
Context load_context() {
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

    // Take the lock before reading so the whole read-modify-write is serialised.
    std::string lock_error;
    ctx.lock = acquire_repo_lock(ctx.repo_root, lock_error);
    if (!ctx.lock) {
        ctx.ok = false;
        ctx.error = lock_error;
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
    // rpm names the notes file, so a hand-edited value must not escape .projot/.
    if (!ctx.config.rpm.empty() && !is_safe_rpm(ctx.config.rpm)) {
        ctx.ok = false;
        ctx.error = "rpm '" + ctx.config.rpm + "' in .projot/config is not a valid file name "
                    "(use letters, digits, '-', '_' and '.', not starting with '.').";
        return ctx;
    }

    // Global config provides defaults for base URL fields.
    ctx.rpm_base_url  = ctx.config.rpm_base_url;
    ctx.jira_base_url = ctx.config.jira_base_url;
    auto global_path = global_config_path();
    if (global_path) {
        Config global_cfg;
        if (parse_config(global_path->string(), global_cfg).ok) {
            if (ctx.rpm_base_url.empty())  ctx.rpm_base_url  = global_cfg.rpm_base_url;
            if (ctx.jira_base_url.empty()) ctx.jira_base_url = global_cfg.jira_base_url;
        }
    }

    return ctx;
}

bool is_safe_rpm(const std::string& rpm) {
    if (rpm.empty() || rpm[0] == '.') return false;
    for (char c : rpm) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.')
            return false;
    }
    return true;
}

std::string projot_file_path(const Context& ctx, const std::string& filename) {
    return (ctx.repo_root / ".projot" / filename).string();
}

std::string unrewritable_notes_reason(const Project& proj, const std::string& path) {
    if (!proj.has_todos_section) {
        return path + " has no '## Todos' heading, so its todos cannot be read. "
               "Restore the heading; projot will not rewrite the file until then.";
    }
    // Commands address todos by ID, so a duplicate would make them act on whichever
    // copy comes first.
    if (!proj.duplicate_ids.empty()) {
        std::string ids;
        for (int id : proj.duplicate_ids) ids += (ids.empty() ? "" : ", ") + std::to_string(id);
        return path + " has todo IDs used more than once (" + ids + "), so an ID no longer "
               "identifies a single todo. Renumber the duplicates; commands that change "
               "todos will refuse until then.";
    }
    if (proj.unparsed_lines.empty()) return "";
    const auto& [line_no, text] = proj.unparsed_lines.front();
    return path + " has " + std::to_string(proj.unparsed_lines.size()) +
           " line(s) in the Todos section that projot does not understand (first: line " +
           std::to_string(line_no) + ": \"" + text + "\"). Fix or remove them; projot will "
           "not rewrite the file until then, so they are not lost.";
}

int execute_project_command(const Context& ctx,
                            ProjectModifier modifier,
                            const std::string& success_msg) {
    Project proj;
    const std::string notes_path = projot_file_path(ctx, ctx.config.rpm + ".md");
    auto parse = parse_markdown(notes_path, proj);
    if (!parse.ok) { std::cerr << "error: " << parse.error << "\n"; return 1; }
    const std::string unrewritable = unrewritable_notes_reason(proj, notes_path);
    if (!unrewritable.empty()) { std::cerr << "error: " << unrewritable << "\n"; return 1; }

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

int execute_config_command(Context& ctx,
                           ConfigModifier modifier,
                           bool re_render,
                           const std::string& success_msg) {
    auto result = modifier(ctx);
    if (!result.ok) {
        // Special cases: these are no-ops, not errors
        if (result.error == "already_present") {
            return 0;
        }
        std::cerr << "error: " << result.error << "\n";
        return 1;
    }

    auto save = write_config(projot_file_path(ctx, "config"), ctx.config);
    if (!save.ok) { std::cerr << "error: " << save.error << "\n"; return 1; }

    if (re_render && !ctx.config.rpm.empty()) {
        Project proj;
        const std::string notes_path = projot_file_path(ctx, ctx.config.rpm + ".md");
        if (parse_markdown(notes_path, proj).ok) {
            const std::string unrewritable = unrewritable_notes_reason(proj, notes_path);
            if (!unrewritable.empty()) {
                std::cerr << "warning: notes file not updated: " << unrewritable << "\n";
            } else {
                auto render = render_to_file(notes_path, ctx.config, proj.todos);
                if (!render.ok)
                    std::cerr << "warning: notes file not updated: " << render.error << "\n";
            }
        }
    }

    if (!success_msg.empty()) std::cout << success_msg << "\n";
    return 0;
}

bool require_project(const Context& ctx) {
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

const std::string HOOK_BLOCK =
    "# projot: regenerate notes file before commit\n"
    "if command -v projot >/dev/null 2>&1; then\n"
    "    projot render\n"
    "fi\n";

// Offset of the hook's last statement if it is `exec ...` or `exit ...`, which would
// end the script before an appended block could run (git's own pre-commit.sample ends
// with `exec git diff-index ...`); npos otherwise.
static std::size_t terminal_statement_offset(const std::string& content) {
    std::size_t end = content.size();
    while (end > 0) {
        std::size_t start = content.rfind('\n', end - 1);
        start = (start == std::string::npos) ? 0 : start + 1;
        const std::string line = trim(content.substr(start, end - start));
        if (!line.empty() && line[0] != '#') {
            const bool is_exit = line == "exit" || line.rfind("exit ", 0) == 0 || line.rfind("exit;", 0) == 0;
            const bool is_exec = line.rfind("exec ", 0) == 0;
            return (is_exit || is_exec) ? start : std::string::npos;
        }
        if (start == 0) break;
        end = start - 1;
    }
    return std::string::npos;
}

bool install_hook_impl(const fs::path& repo_root,
                       bool& appended,
                       std::string& notice,
                       std::string& error) {
    appended = false;
    auto hooks_dir = resolve_hooks_dir(repo_root);
    auto hook_path = hooks_dir / "pre-commit";

    std::error_code ec;
    if (!fs::exists(hooks_dir, ec)) {
        fs::create_directories(hooks_dir, ec);
        if (ec) { error = "cannot create hooks directory: " + ec.message(); return false; }
    }

    bool exists = fs::exists(hook_path, ec);
    if (exists) {
        std::ifstream in(hook_path);
        if (!in.is_open()) { error = "cannot read " + hook_path.string(); return false; }
        std::string content((std::istreambuf_iterator<char>(in)), {});
        in.close();
        if (!in.good() && !in.eof()) { error = "failed to read " + hook_path.string(); return false; }

        // Idempotent: block already present
        if (content.find("projot render") != std::string::npos)
            return true;

        // Re-verify directory exists before append (protect against race condition)
        if (!fs::exists(hooks_dir, ec)) {
            fs::create_directories(hooks_dir, ec);
            if (ec) { error = "cannot create hooks directory: " + ec.message(); return false; }
        }

        // Insert before a final exec/exit so the block actually runs. Either way the
        // block is preceded by "\n", which uninstall removes with it, restoring the
        // original hook byte for byte.
        std::string updated;
        const std::size_t terminal = terminal_statement_offset(content);
        if (terminal != std::string::npos) {
            updated = content.substr(0, terminal) + "\n" + HOOK_BLOCK + content.substr(terminal);
            notice = "the existing hook ends with exec/exit, so the projot block was "
                     "inserted before that last statement.";
        } else {
            updated = content + "\n" + HOOK_BLOCK;
        }

        // Rewrite atomically so an interrupted write can't leave a half-written
        // block in the user's hook.
        if (!atomic_write_file(hook_path, updated, error)) return false;
        appended = true;
    } else {
        std::ofstream f(hook_path);
        if (!f.is_open()) { error = "cannot create " + hook_path.string(); return false; }
        f << "#!/bin/sh\n" << HOOK_BLOCK;
        f.flush();
        if (!f.good()) { error = "write error to " + hook_path.string(); return false; }
        f.close();
        if (f.fail()) { error = "failed to close " + hook_path.string(); return false; }
    }

#ifndef _WIN32
    fs::permissions(hook_path,
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
        fs::perm_options::add, ec);
    if (ec) { error = "cannot set executable permission: " + ec.message(); return false; }
#endif

    return true;
}
