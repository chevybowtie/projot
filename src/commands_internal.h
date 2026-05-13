#pragma once

#include "config.h"
#include "markdown.h"

#include <filesystem>
#include <functional>
#include <string>

namespace fs = std::filesystem;

struct Context {
    fs::path repo_root;
    Config   config;
    bool     ok    = true;
    std::string error;
};

// Load repo root + config. Validates config_version.
Context load_context();

// Helper to build paths like .projot/config, .projot/{rpm}.md
std::string projot_file_path(const Context& ctx, const std::string& filename);

// Verifies that a project is configured and the notes file exists.
bool require_project(const Context& ctx);

// Execute a command that modifies a project's todos and re-renders the file.
// Takes a callback that modifies the Project and returns the result/message.
// If success_msg is non-empty, prints it after successful render.
// Special case: error message "already_completed" returns 0 (warning, not error).
// Returns exit code (0 = success, 1 = error).
using ProjectModifier = std::function<ParseResult(Project&)>;
int execute_project_command(const Context& ctx,
                            ProjectModifier modifier,
                            const std::string& success_msg = "");

// Execute a command that modifies the config and optionally re-renders the project notes.
// Takes a callback that modifies ctx.config and returns the result/message.
// If re_render is true, re-parses and renders the project notes after config change.
// Special cases: "already_present" returns 0 (no-op, not error).
// Returns exit code (0 = success, 1 = error).
using ConfigModifier = std::function<ParseResult(Context&)>;
int execute_config_command(Context& ctx,
                           ConfigModifier modifier,
                           bool re_render = true,
                           const std::string& success_msg = "");

// The projot block that gets inserted into git hooks.
extern const std::string HOOK_BLOCK;

// Install or re-install the pre-commit git hook.
// Sets appended=true if content was appended to an existing hook rather than written fresh.
// Idempotent: does nothing if the block is already present.
bool install_hook_impl(const fs::path& repo_root,
                       bool& appended,
                       std::string& error);
