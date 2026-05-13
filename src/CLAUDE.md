# CLAUDE.md — Command Architecture

Guidance for working with projot's command layer.

## Command Registration and Structure

Commands are split by concern into three files:

| File | Responsibility | Examples |
|------|-----------------|----------|
| `commands_project.cpp` | Modify project todos in-memory, parse markdown, render | add-todo, list, complete, add-note, set-link |
| `commands_config.cpp` | Modify `.projot/config`, validate, optionally re-render | init, new, set-app-id, add-github/swagger/blizzard, add-azure |
| `commands_maint.cpp` | Utility and infrastructure | render, install-hook, install-mcp-server |

Shared infrastructure lives in `commands_internal.h` and `commands_shared.cpp`:
- `struct Context` — repo root, config, and error state
- `load_context()` — discover repo, load config, validate schema
- `projot_file_path()` — build `.projot/filename` paths
- `require_project()` — verify a project is configured
- `execute_project_command()` — helper for todo modifications
- `execute_config_command()` — helper for config modifications
- `install_hook_impl()` — shared pre-commit hook installer

## Command Implementation Patterns

### Pattern: Project Modifier

For commands that read todos, modify in-memory, and render:

```cpp
int cmd_my_command(const Args& args) {
    if (args.help_requested) { /* help text */ }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }
    if (!require_project(ctx)) return 1;

    return execute_project_command(ctx, [&](Project& proj) {
        // Modify proj.todos
        // Return ParseResult{true, ""} on success
        // Return ParseResult{false, "msg"} on error
        // Special: ParseResult{false, "already_completed"} returns 0 (warning, not error)
    }, "Success message");
}
```

Examples: `add_todo`, `complete`, `add_note`

### Pattern: Config Modifier

For commands that modify `.projot/config` and optionally re-render:

```cpp
int cmd_my_config_command(const Args& args) {
    if (args.help_requested) { /* help text */ }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }

    return execute_config_command(ctx, [&](Context& c) {
        // Modify c.config
        // Return ParseResult{true, ""} on success
        // Return ParseResult{false, "already_present"} for idempotent no-ops (returns 0)
    }, true, "Config updated");  // true = re-render after config change
}
```

Examples: `set_link`, `set_app_id`, `add_github`

### Pattern: Stateless Query

For read-only commands (no modification):

```cpp
int cmd_my_query(const Args& args) {
    if (args.help_requested) { /* help text */ }

    auto ctx = load_context();
    if (!ctx.ok) { std::cerr << "error: " << ctx.error << "\n"; return 1; }

    // Perform read-only I/O
    // Return 0 (success) or 1 (error)
    return 0;
}
```

Examples: `list`, `render`

## Registration (main.cpp)

Every command must be registered in two places:

1. **Commands map** (line ~75):
   ```cpp
   commands["my-command"] = cmd_my_command;
   ```

2. **Valid flags map** (line ~100–110):
   ```cpp
   valid_flags["my-command"] = {"--flag-1", "--flag-2", "<positional>"};
   ```

The `valid_flags` map lists all flags the command accepts. If the command takes a positional argument (like `add-todo "text"`), add `"<positional>"` to the list. The main dispatcher validates flags against this list.

## Helper Functions: When to Use Each

| Helper | Use When |
|--------|----------|
| `execute_project_command()` | Modifying todos; read notes → modify → render |
| `execute_config_command()` | Modifying config; optionally re-render notes |
| Manual logic | Read-only (no file modification); special validation logic not covered by helpers |

**Rationale:** Helpers standardize error handling, file I/O, and success output. They reduce boilerplate and ensure consistency. Use them unless the command is fundamentally different (e.g., a diagnostic utility).

## Error Handling

Use `ParseResult{ok, error_message}`:
- `ParseResult{true, ""}` — success; helper prints nothing unless success_msg is provided
- `ParseResult{false, "msg"}` — error; helper prints "error: msg" and returns 1
- `ParseResult{false, "already_present"}` — idempotent no-op; returns 0, not 1 (special case)
- `ParseResult{false, "already_completed"}` — warning, not error; returns 0, not 1 (special case in project commands)

Validation should happen **before** calling helpers:
```cpp
if (!args.has("required-flag")) {
    std::cerr << "error: --required-flag is required. Run 'projot cmd --help' for usage.\n";
    return 1;
}
```

## Accessing Config and Project Data

Via `Context`:
- `ctx.repo_root` — git repo root (fs::path)
- `ctx.config` — loaded config (Config struct)
  - `ctx.config.app_id`, `ctx.config.rpm`, `ctx.config.name`, `ctx.config.itrack`
  - `ctx.config.links`, `ctx.config.labels`, `ctx.config.link_urls`
  - `ctx.config.github`, `ctx.config.swagger`, `ctx.config.blizzard`
  - `ctx.config.azure_*` (azure_subscription, azure_key_vault, etc.)
- `ctx.config.created` — creation date (string, YYYY-MM-DD)

Via `Project` (in modify callbacks):
- `proj.todos` — `std::vector<Todo>`
  - `t.id` — stable numeric ID
  - `t.text` — todo text
  - `t.completed` — bool
  - `t.created_date`, `t.completed_date` — date strings
  - `t.notes` — `std::vector<std::string>`

## Helper Functions for Todo Manipulation

From `todo.h`:
- `next_todo_id(proj.todos)` — returns next available ID
- `find_todo(proj.todos, id)` — returns pointer to todo or nullptr
- `complete_todo(proj.todos, id, date_string)` — marks todo done; returns warning if already done
- `add_note(proj.todos, id, text)` — appends note; returns warning if todo not found

Use these instead of manually searching or modifying the todo vector.

## Do Not

- ❌ Call `std::system()` for git operations (use `execute_config_command` which saves and optionally renders)
- ❌ Manually parse/render markdown; use `parse_markdown()` and `render_to_file()` via helpers
- ❌ Create temporary files without cleanup
- ❌ Assume a project is configured without calling `require_project(ctx)` first
- ❌ Modify `ctx.config` without calling `execute_config_command()`; helpers save to disk

## Testing Commands

Test files in `tests/test_commands.cpp` use `TempRepo` helper to set up a temporary git repo and `.projot` directory. See `tests/CLAUDE.md` for test infrastructure details.

Pattern:
```cpp
TEST_CASE("cmd_my_command") {
    TempRepo repo("my_command_test");
    // Repo root is at repo.path(); ".projot/config" and ".projot/{rpm}.md" exist
    // Run the command via the public API or directly
}
```
