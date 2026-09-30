#pragma once

#include "config.h"
#include "markdown.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace fs = std::filesystem;
static constexpr const char* CARRYOVER_TODOS_FILE = "carryover_todos.md";

class RepoLock;

struct Context {
    fs::path repo_root;
    Config   config;        // exactly what .projot/config holds; safe to write back
    bool     ok    = true;
    std::string error;

    // Effective base URLs: the repo-level value if set, else the global default.
    // Kept out of `config` so a global default is never persisted into the repo.
    std::string rpm_base_url;
    std::string jira_base_url;

    // Held for the command's lifetime; see acquire_repo_lock().
    std::shared_ptr<RepoLock> lock;
};

// True if rpm is usable as a file name inside .projot/: letters, digits, '-', '_'
// and '.', not starting with '.' (which rules out "..", hidden files, and paths).
bool is_safe_rpm(const std::string& rpm);

// Load repo root + config. Validates config_version.
Context load_context();

// Helper to build paths like .projot/config, .projot/{rpm}.md
std::string projot_file_path(const Context& ctx, const std::string& filename);

// Returns why a parsed notes file must not be rewritten (rendering would drop content
// the parser could not read), or "" if it is safe to rewrite.
std::string unrewritable_notes_reason(const Project& proj, const std::string& path);

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

// Install or re-install the pre-commit git hook in the directory git actually runs
// hooks from (see resolve_hooks_dir()).
// Sets appended=true if content was added to an existing hook rather than written fresh.
// Sets notice to extra information for the user (e.g. where the block was placed).
// Idempotent: does nothing if the block is already present.
bool install_hook_impl(const fs::path& repo_root,
                       bool& appended,
                       std::string& notice,
                       std::string& error);
