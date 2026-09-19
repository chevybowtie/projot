# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Test Commands

Always use:

```bash
cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

**Windows gotcha:** On Windows, CMake config-time flag is required: `cmake -B build -DCMAKE_BUILD_TYPE=Release`. Then run tests with `ctest --test-dir build -C Release`.

**Test data paths:** Tests load data from a path compiled at CMake time (`PROJOT_TEST_DATA_DIR`). If tests fail with "file not found" in test data, delete `build/CMakeCache.txt` and reconfigure. Always run tests from repo root; do not run the binary directly.

**Temp directory cleanup:** Tests create orphaned temp dirs in `/tmp/projot_*` (Linux) or `%TEMP%\projot_*` (Windows) if interrupted. Clean manually if tests fail with "already exists" errors.

## Language and Dependencies

**C++17 is mandatory.** Do not use C++20 features (ranges, concepts, requires). Compiler must support C++17 with `std::filesystem` (GCC 8+, Clang 7+, MSVC 2017 15.7+); there is no explicit version check in CMake — older compilers simply fail to build.

**No external dependencies.** projot forbids Boost, fmt, nlohmann/json, and other libraries. Use only `<filesystem>`, `<optional>`, `<string>`, `std::stringstream`, and standard library only. String parsing is manual; JSON is parsed with `std::string::find()` and `std::stringstream`. This is a hard constraint enforced in code review.

**doctest header is checked in.** Tests use `tests/doctest.h` (363 KB, included directly). To upgrade: download the latest doctest.h from <https://github.com/doctest/doctest/releases> and replace the file.

**Code style:** `snake_case` for functions/variables, `CamelCase` for classes, `UPPER_SNAKE_CASE` for macros/constants. RAII for resources, const correctness throughout, comments explain *why* not *what*. No `std::thread`. CLI command names and flags are stable — removing or renaming one is a breaking change requiring a major version bump.

## Command Architecture

All 21 commands are split across three files for clarity:

- `src/commands_project.cpp` — project operations (add-todo, list, complete, status, add-note, set-link)
- `src/commands_config.cpp` — configuration (init, new, close, set-app-id, add-github/swagger/blizzard, add-azure, set-teams-webhook)
- `src/commands_maint.cpp` — maintenance (render, install-hook, uninstall-hook, install-mcp-server, uninstall-mcp-server, set-global)

**To add a new command:**

1. Implement `int cmd_<name>(const Args& args)` in the appropriate file.
2. Declare it in `src/commands.h`.
3. Register in `src/main.cpp` in both the `valid_flags` map (line ~45) and the `commands` map (line ~87); if it takes positional arguments, also add it to the `positional_commands` set (line ~138).

**Command pattern:** Most commands use one of two helpers:

- `execute_project_command(ctx, [&](Project& proj) { ... })` — for modifying todos in-memory, parsing, and rendering.
- `execute_config_command(ctx, [&](Context& c) { ... })` — for modifying config, saving, and optionally re-rendering.

Both helpers handle error checking, file I/O, and success messages. Use them for consistency; avoid custom logic unless the command is stateless (like `list` or `render`).

**Special return codes:** Commands that use `execute_project_command` treat `ParseResult{false, "already_completed"}` as a warning, not an error (returns 0, not 1). This is used by `complete` to warn if a todo is already marked done.

## Git Workflow

**Worktree hazard:** Active worktree on `feature/optimization` occupies a branch lock. Before switching branches in the main repo, check `git worktree list`. If the worktree is stale, use `git worktree remove`. Do not rely on `git checkout` to move between branches if a worktree holds a lock.

**Commit format is conventional, not enforced.** Follow the pattern from history: `refactor: ...`, `docs: ...`, `fix: ...`, `feat: ...`, `test: ...`, `chore: ...`. No linter enforces this; it is a team convention.

## Critical Gotchas

**No shell invocations.** Git staging goes through `git_stage_file()` in `src/commands_maint.cpp`, which uses `fork()`+`execvp()` (`CreateProcess` on Windows) with no shell. Do not add `std::system()` or other shell-string calls; extend the existing helpers instead. The Windows `CreateProcess` command lines are built with `quote_windows_arg()` (`src/utils.h`); quote every argument through it rather than concatenating raw strings.

**MCP server requires Node.js.** The `install-mcp-server` command checks for Node.js by scanning `PATH` (`node_available()`, no shell). Node must be on PATH. If missing, the tool warns but doesn't fail. Test locally with `which node` before relying on MCP integration.

**Pre-commit hook uses string markers.** The hook is idempotent; it checks for a BEGIN/END marker to avoid duplication. If manually edited and markers are broken, the tool will re-append, creating duplicates. Do not hand-edit `.git/hooks/pre-commit`.

**Config versioning is forward-compatible.** If a newer projot version writes `config_version=2`, older binaries will refuse to run (clear error). Downgrades to older binaries may fail silently. Document format changes in docs/DESIGN.md and increment `PROJOT_CONFIG_SCHEMA_VERSION` in CMakeLists.txt.

**Azure resource types are hardcoded.** Adding a new Azure type (e.g., "vm", "container-registry") requires editing `src/commands_config.cpp` and recompiling. This is acceptable for v0.1 (types are stable); post-v0.1 could use a config file.

## Testing

All tests pass. Use the `TempRepo` helper in `tests/test_commands.cpp` to set up temporary git repos for testing. Test data lives in `tests/data/configs/` and `tests/data/notes/`. Reference it with the `PROJOT_TEST_DATA_DIR` macro (set at CMake time to an absolute path).

Tests verify config parsing, markdown I/O, command execution, versioning, error handling, and hooks. Coverage is comprehensive for the happy path; error cases (render failures, permission errors) have minimal coverage.

## Documentation Consolidation

This file (`CLAUDE.md`) is the canonical source for operational guidance; the legacy `INSTRUCTIONS.md` and `.copilot-instructions.md` files have been removed. If guidance changes, update only this file. (`.github/copilot-instructions.md` is auto-generated by SigMap's gen-context.js — do not hand-edit it.)

## Auto-generated signatures
<!-- Updated by gen-context.js -->
# Code signatures

## SigMap commands

| When | Command |
|------|---------|
| Before answering a question about code | `sigmap ask "<your question>"` |
| To rank files by topic | `sigmap --query "<topic>"` |
| After changing config or source dirs | `sigmap validate` |
| To verify an AI answer is grounded | `sigmap judge --response <file>` |

Always run `sigmap ask` (or `sigmap --query`) before searching for files relevant to a task.

## todos
```
tests/test_commands.cpp:1102  # TODO: is already in Todo state
.github/copilot-instructions.md:22  # TODO: s
.github/copilot-instructions.md:24  # TODO: s")) {
.github/copilot-instructions.md:25  # TODO: s section
.github/copilot-instructions.md:26  # TODO: s
.github/copilot-instructions.md:27  # TODO: s
.github/copilot-instructions.md:28  # TODO: s\n";
.github/copilot-instructions.md:29  # TODO: s\n\n"
.github/copilot-instructions.md:30  # TODO: s\n\n";
.github/copilot-instructions.md:31  # TODO: s\n";
.github/copilot-instructions.md:32  # TODO: s\n";
.github/copilot-instructions.md:33  # TODO: 1 has empty notes block
.github/copilot-instructions.md:34  # TODO: s\n\n"
.github/copilot-instructions.md:35  # TODO: s\r\n"
.github/copilot-instructions.md:36  # TODO: s\n\n"
.github/copilot-instructions.md:37  # TODO: s\n\n"
.github/copilot-instructions.md:38  # TODO: s.
.github/copilot-instructions.md:39  # TODO: s");
.github/copilot-instructions.md:40  # TODO: s heading.
.github/copilot-instructions.md:41  # TODO: s");
```

## changes (last 10 commits — 6 seconds ago)
```
.github/copilot-instructions.md               +execArgs  +getConfigValue  +getGlobalConfigValue  +slugifyBranchName
mcp/server.js                                 ~handleRequest
mcp/teams-sync.js                             +buildAdaptiveCard  +buildLegacyWebhookBody  +renderSummary  +buildWorkflowBody
```

## .claude

### .claude/skills/projot-new-command/SKILL.md
```
h1 Add a New Command to projot
h2 Step 1: Decide the Category
h2 Step 2: Write the Command Implementation
h3 Pattern A: Project Modifier (modifies todos)
h3 Pattern B: Config Modifier (modifies config)
h3 Pattern C: Stateless Command (no modification)
h2 Step 3: Declare the Function in commands.h
h2 Step 4: Register in main.cpp
h2 Step 5: Test
h2 Common Patterns
h3 Argument Validation
h3 Handling "Already Present" (idempotent)
h3 Parsing Integer Arguments
h2 Do Not
h2 Example: Add "set-color" Config Command
code-fence cpp
code-fence plain
code-fence bash
```

## .github

### .github/copilot-instructions.md
```
h2 Auto-generated signatures
h1 Code signatures
h2 SigMap commands
h2 todos
h2 changes (last 10 commits — 3 minutes ago)
h2 .claude
h3 .claude/skills/projot-new-command/SKILL.md
h2 .github
h3 .github/copilot-instructions.md
h3 .github/workflows/ci.yml
h3 .github/workflows/release.yml
h2 mcp
h3 mcp/server.js
h3 mcp/test.js
h3 mcp/README.md
h3 mcp/teams-sync.js
h2 src
h3 src/CLAUDE.md
h3 src/cli.cpp
h3 src/cli.h
h3 src/commands_config.cpp
h3 src/commands_maint.cpp
h3 src/commands_project.cpp
h3 src/commands_shared.cpp
h3 src/config.cpp
```

### .github/workflows/ci.yml
```
keys: [name, on, jobs]
job: build-and-test
```

### .github/workflows/release.yml
```
keys: [name, on, jobs]
job: build
job: release
```

## mcp

### mcp/README.md
```
h1 Projot MCP Server
h2 Why Use This?
h2 Installation
h3 Prerequisites
h3 Configure Your IDE
h4 Manual configuration (if needed)
h4 Claude Code
h4 Copilot (VS Code)
h3 Verify Setup
h2 Tools
h2 Testing
h1 Test the server responds
h1 Try it interactively (requires projot in PATH)
h1 Then send a JSON request on stdin, e.g.:
h1 {"method":"initialize"}
h2 How It Works
code-fence bash
code-fence plain
code-fence json
```

### mcp/server.js
```
function execArgs(cmd, args)  :44-50
function getConfigValue(key)  :52-61
function getGlobalConfigValue(key)  :63-80
function slugifyBranchName(str)  :82-90
function handleRequest(request)  :92-426
```

### mcp/teams-sync.js
```
function readConfigValue(text, key)  :15-18
function parseTodos(mdText)  :25-38
function textBlock(text, options)  :42-44
function kanbanColumn(title, items, color)  :46-56
function buildAdaptiveCard(projectName, rpm, buckets)  :58-76
function buildLegacyWebhookBody(card)  :78-88
function renderSummary(projectName, rpm, buckets)  :90-107
function buildWorkflowBody(projectName, rpm, buckets, card)  :109-124
function isLegacyTeamsWebhook(syncUrl)  :126-135
function buildSyncBody(syncUrl, projectName, rpm, buckets)  :137-142
function postJson(webhookUrl, body)  :146-179
async function main(argv = process.argv.slice(2))  :183-209
```

### mcp/test.js
```
function runTool(toolName, toolArgs)  :54-77
function test(name, fn)  :81-92
function runInitialize(protocolVersion)  :269-284
```

## src

### src/commands_maint.cpp
```
is_safe_rpm(const std::string& s) → static bool
for(char c : s)
git_stage_file(const fs::path& repo_root, const std::string& rel_path) → static bool
if(pid == 0)
if(fd >= 0)
binary_dir() → static std::optional<fs::path>
for(;;)
find_mcp_source_dir() → static std::optional<fs::path>
for(const fs::path& raw : { *dir / ".." / "mcp", *dir / ".." / "share" / "projot" / "mcp", *dir / ".." / ".." / "mcp", *dir / ".." / ".." / "share" / "projot" / "mcp"})
json_escape(const std::string& s) → static std::string
for(unsigned char c : s)
switch(c)
if(c < 0x20) → default:
claude_settings_fresh(const std::string& server_arg) → static std::string
claude_settings_injected_block(const std::string& server_arg) → static std::string
invoke_teams_sync(const Context& ctx) → static void
if(!mcp_dir)
if(pi.hProcess)
if(pid == 0)
if(fd >= 0)
for(int i = 0; i < 100; ++i)
cmd_render(const Args& args) → int
if(!ctx.ok)
if(!parse.ok)
if(!render.ok)
```

### src/utils.h
```
date_today() → inline std::string
quote_windows_arg(const std::string& arg) → inline std::string
if(*it == '"')
format_date(const std::string& fmt) → inline std::string
deduplicate(const std::vector<T>& v) → inline std::vector<T>
for(const auto& item : v)
```

### src/CLAUDE.md
```
h1 CLAUDE.md — Command Architecture
h2 Command Registration and Structure
h2 Command Implementation Patterns
h3 Pattern: Project Modifier
h3 Pattern: Config Modifier
h3 Pattern: Stateless Query
h2 Registration (main.cpp)
h2 Helper Functions: When to Use Each
h2 Error Handling
h2 Accessing Config and Project Data
h2 Helper Functions for Todo Manipulation
h2 Do Not
h2 Testing Commands
code-fence cpp
code-fence plain
```

### src/cli.cpp
```
parse_args(int argc, char* argv[]) → Args
if(first == "--help" || first == "-h")
if(first == "--version" || first == "-v")
if(!argv[i] || argv[i][0] != '-')
while(i < argc)
if(arg == "--help" || arg == "-h")
normalize_flag_aliases(Args& args) → void
for(const auto& [alias, canonical] : aliases)
```

### src/cli.h
```
struct Args
has(const std::string& key) → bool
get(const std::string& key, const std::string& def = "") → std::string
get_all(const std::string& key) → std::vector<std::string>
boolean_flags() → inline const std::set<std::str
```

### src/commands_config.cpp
```
struct UrlListDef
struct AzureTypeDef
cmd_init(const Args& args) → int
if(args.help_requested)
if(!root)
if(!result.ok)
cmd_new(const Args& args) → int
if(args.help_requested)
if(!ctx.ok)
for(const auto& ld : std::vector<LinkDef>{ {"teams", "Teams", "teams"}, {"jira", "Jira", "jira-url"}, {"rpm", "RPM", "rpm-url"}, {"other", "Other", "other"}, })
if(!save.ok)
if(!parse.ok)
if(!render.ok)
if(ec && ec != std::errc::no_such_file_or_directory)
cmd_set_app_id(const Args& args) → int
if(args.help_requested)
if(!ctx.ok)
execute_config_command(ctx, [&new_app_id, force](Context& c) → return
find_url_list_type(const std::string& key) → static const UrlListDef*
for(const auto& t : URL_LIST_TYPES)
cmd_add_url(const Args& args, const std::string& kind) → static int
if(args.help_requested)
if(!tdef)
if(!ctx.ok)
execute_config_command(ctx, [tdef, &url, &kind](Context& c) → return
```

### src/commands_internal.h
```
struct Context
```

### src/commands_project.cpp
```
cmd_close(const Args& args) → int
if(args.help_requested)
if(!ctx.ok)
if(ec)
if(!parse.ok)
if(ec)
if(!carryover_render.ok)
if(ec && ec != std::errc::no_such_file_or_directory)
if(!save.ok)
cmd_add_todo(const Args& args) → int
if(args.help_requested)
if(!ctx.ok)
execute_project_command(ctx, [&](Project& proj) → return
cmd_list(const Args& args) → int
if(args.help_requested)
if(!ctx.ok)
if(!parse.ok)
for(const auto* t : todos)
switch(t->status)
cmd_links(const Args& args) → int
if(args.help_requested)
if(!ctx.ok)
for(const auto& key : cfg.links)
for(const auto& sec : sections)
for(const auto& sec : azure_sections)
```

### src/commands_shared.cpp
```
load_context() → Context
if(!root)
if(!result.ok)
if(ctx.config.config_version > PROJOT_CONFIG_SCHEMA_VERSION)
if(ctx.config.config_version == 0)
if(global_path)
projot_file_path(const Context& ctx, const std::string& filename) → std::string
execute_project_command(const Context& ctx, ProjectModifier modifier, const std::string& success_msg) → int
if(!parse.ok)
if(!result.ok)
if(result.error != "already_completed")
if(!render.ok)
execute_config_command(Context& ctx, ConfigModifier modifier, bool re_render, const std::string& success_msg) → int
if(!result.ok)
if(result.error == "already_present")
if(!save.ok)
require_project(const Context& ctx) → bool
install_hook_impl(const fs::path& repo_root, bool& appended, std::string& error) → bool
if(ec)
if(exists)
if(ec)
```

### src/config.cpp
```
trim(const std::string& s) → std::string
split_list(const std::string& value) → std::vector<std::string>
join_list(const std::vector<std::string>& items) → std::string
parse_azure_entry(const std::string& s) → AzureEntry
format_azure_entry(const AzureEntry& e) → std::string
is_list_key(const std::string& key) → static bool
parse_config(const std::string& path, Config& out) → ParseResult
if(key == "config_version")
catch(...)
for(auto* m : {&out.labels, &out.link_urls})
write_config(const std::string& path, const Config& cfg) → ParseResult
for(const auto& key : cfg.links)
for(const auto& [k, v] : cfg.labels)
for(const auto& key : cfg.links)
for(const auto& [k, v] : cfg.link_urls)
if(has_azure)
write_global_config(const std::string& path, const Config& cfg) → ParseResult
```

### src/config.h
```
struct AzureEntry
struct Config
struct ParseResult
clear_project() → void
```

### src/main.cpp
```
print_usage() → static void
main(int argc, char* argv[]) → int
if(args.version_requested)
if(args.help_requested)
if(!args.help_requested)
for(const auto& [flag, _] : args.flags)
```

### src/markdown.cpp
```
starts_with(const std::string& s, const std::string& prefix) → static bool
parse_todo_line(const std::string& line, int& id, TodoStatus& status, std::string& text) → static bool
parse_lines(const std::vector<std::string>& lines, Project& out) → static MarkdownParseResult
for(const auto& raw : lines)
if(section == Section::Header)
if(section == Section::Links)
if(section == Section::GitHub)
if(section == Section::Swagger)
if(section == Section::Blizzard)
if(section == Section::Todos)
parse_markdown(const std::string& path, Project& out) → MarkdownParseResult
parse_markdown_string(const std::string& content, Project& out) → MarkdownParseResult
```

### src/markdown.h
```
struct Project
struct MarkdownParseResult
```

### src/renderer.cpp
```
render_markdown(const Config& cfg, const std::vector<Todo>& todos) → std::string
for(const auto& key : cfg.links)
for(const auto& sec : azure_sections)
if(any_azure)
for(const auto& sec : azure_sections)
for(const auto& raw : deduped)
for(const auto& todo : todos)
switch(todo.status)
for(const auto& note : todo.notes)
render_to_file(const std::string& path, const Config& cfg, const std::vector<Todo>& todos) → RenderResult
```

### src/renderer.h
```
struct RenderResult
```

### src/repo.cpp
```
find_repo_root(const std::filesystem::path& start) → std::optional<std::filesystem:
while(true)
if(parent == current)
global_config_path() → std::optional<std::filesystem:
```

### src/todo.cpp
```
next_todo_id(const std::vector<Todo>& todos) → int
for(const auto& t : todos)
find_todo(std::vector<Todo>& todos, int id) → Todo*
for(auto& t : todos)
find_todo(const std::vector<Todo>& todos, int id) → const Todo*
for(const auto& t : todos)
filter_todos(const std::vector<Todo>& todos, TodoFilter filter) → std::vector<const Todo*>
for(const auto& t : todos)
complete_todo(std::vector<Todo>& todos, int id, const std::string& date) → TodoResult
if(t->status == TodoStatus::Done)
set_todo_status(std::vector<Todo>& todos, int id, TodoStatus status, const std::string& date) → TodoResult
if(t->status == status)
if(status == TodoStatus::Done)
add_note(std::vector<Todo>& todos, int id, const std::string& note) → TodoResult
if(t->status == TodoStatus::Done)
```

### src/todo.h
```
struct Todo
struct TodoResult
```

## tests

### tests/test_commands.cpp
```
struct TempRepo
  cmd_init(a) → return
  cmd_new(a) → return
struct TempGlobalConfig
  free(prev)
  unsetenv("XDG_CONFIG_HOME") → else
TempRepo(const std::string& name) → explicit
init(const std::string& app_id = "TestApp") → int
new_project(const std::string& rpm = "12345", const std::string& name = "Test Project", const std::string& jira = "67890", bool no_hook = true) → int
make_args(const std::string& sub, std::initializer_list<std::pair<std::string,std::string>> flags = {}, const std::string& positional = "") → static Args
CHECK(cfg.github[0] == "https: } TEST_CASE("init_with_multiple_github")
CHECK(cfg.link_urls["teams"] == "https: } TEST_CASE("new_with_teams_sync_url")
CHECK(cfg.teams_sync_url == "https: } TEST_CASE("new_fails_if_rpm_set")
CHECK(proj.github_urls[0] == "https: } TEST_CASE("close_clears_project_fields")
CHECK(cfg.link_urls["teams"] == "https: } TEST_CASE("set_link_legacy_itrack_key_maps_to_jira")
CHECK(cfg.azure_subscription[0] == "MySub|https: } TEST_CASE("add_azure_private_dns_url_only")
CHECK(cfg.azure_private_dns[0] == "https: } TEST_CASE("add_azure_deduplicates")
CHECK(content.find("[my-kv](https: } TEST_CASE("add_azure_multiple_of_same_type")
CHECK(content.find("[PROD](https: CHECK(content.find("[NPRD](https: } TEST_CASE("add_todo_missing_text_fails")
CHECK(cmd_set_link(make_args("set-link", {{"url", "https: } TEST_CASE("add_github_missing_url_fails")
TempGlobalConfig(const std::string& name) → explicit
if(had_appdata)
config_path() → fs::path
CHECK(cfg.rpm_base_url == "https: } TEST_CASE("set_global_writes_jira_base_url")
CHECK(cfg.jira_base_url == "https: } TEST_CASE("set_global_preserves_existing_value")
```

### tests/test_errors.cpp
```
struct ErrorTempRepo
ErrorTempRepo(const std::string& name, bool with_git = true) → explicit
make_err_args(const std::string& sub, std::initializer_list<std::pair<std::string,std::string>> flags = {}) → static Args
```


> **Not everything is here.** 13 file(s) omitted to stay under the 4000-token budget (tests and configs go first). The retrieval index still has them all — run `sigmap ask "<question>"` to pull in anything missing.