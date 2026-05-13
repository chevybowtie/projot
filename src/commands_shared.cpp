#include "commands_internal.h"
#include "config.h"
#include "markdown.h"
#include "renderer.h"
#include "repo.h"

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

    // Merge global config (provides defaults for base URL fields)
    auto global_path = global_config_path();
    if (global_path) {
        Config global_cfg;
        if (parse_config(global_path->string(), global_cfg).ok) {
            if (ctx.config.rpm_base_url.empty())
                ctx.config.rpm_base_url = global_cfg.rpm_base_url;
            if (ctx.config.itrack_base_url.empty())
                ctx.config.itrack_base_url = global_cfg.itrack_base_url;
        }
    }

    return ctx;
}

std::string projot_file_path(const Context& ctx, const std::string& filename) {
    return (ctx.repo_root / ".projot" / filename).string();
}

int execute_project_command(const Context& ctx,
                            ProjectModifier modifier,
                            const std::string& success_msg) {
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
        if (parse_markdown(projot_file_path(ctx, ctx.config.rpm + ".md"), proj).ok)
            render_to_file(projot_file_path(ctx, ctx.config.rpm + ".md"), ctx.config, proj.todos);
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

bool install_hook_impl(const fs::path& repo_root,
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
