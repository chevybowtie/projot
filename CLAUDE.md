
## Auto-generated signatures
<!-- Updated by gen-context.js -->
# Code signatures

## todos

```
src/markdown.cpp:93  # TODO: s")) {
src/markdown.h:26  # TODO: s
src/renderer.cpp:123  # TODO: s\n";
docs/DESIGN.md:243  # TODO: s
docs/DESIGN.md:269  # TODO: s` and parse numbered items.
docs/DESIGN.md:676  # TODO: s` present but no entries → empty list, no error                    |
docs/DESIGN.md:677  # TODO: s` → empty list, no error                               |
tests/data/notes/basic.md:13  # TODO: s
tests/data/notes/managed_sections.md:24  # TODO: s
tests/data/notes/multi_todo.md:11  # TODO: s
tests/data/notes/no_todos.md:11  # TODO: s
tests/doctest.h:885  # TODO: :
tests/doctest.h:1818  # TODO: Why do we need this? (To remove NOLINT)
tests/doctest.h:4293  # TODO: figure out if this is indeed necessary/correct - seems like either the
tests/doctest.h:5284  # TODO: :
tests/doctest.h:5612  # TODO: change this to use std::stoi or something else! currently it uses unde
tests/doctest.h:5926  # TODO: check if there is nothing in reporters_currently_used
tests/doctest.h:6690  # TODO: under DOCTEST_MSVC: does the comparison need strnicmp() to work with d
tests/test_errors.cpp:76  # TODO: s\n\n"
tests/test_hook.cpp:30  # TODO: s\n\n";
```

## changes (last 5 commits — 2 days ago)

```
src/repo.cpp                                  +_WIN32
completions/projot.fish                       ~__projot_no_subcommand
mcp/server.js                                 +getGlobalConfigValue  ~getConfigValue  ~handleRequest  ~slugifyBranchName
```

## completions

### completions/projot.bash

```
# projot bash completion
```

### completions/projot.fish

```
# projot fish completion
```

## docs

### docs/BUILD.md

```
h1 Build
h2 .deb build
code-fence sh
code-fence plain
```

### docs/DEVELOPER.md

```
h1 Developer Guide
h2 Requirements
h2 Building
h3 Build with make (Linux)
h3 Configure and build directly with CMake (all platforms)
h2 Running the tests
h2 Installing
h3 System-wide (Linux/macOS)
h3 Per-user (Linux/macOS)
h3 Windows
h2 Shell completion
h2 Project layout
h2 Compile-time definitions
code-fence sh
code-fence plain
code-fence ---
```

### docs/INSTALL-LINUX.md

```
h1 Installation — Linux
h2 From a pre-built release (recommended)
h3 Option 1: Debian package (recommended)
h1 Download and install the .deb package
h3 Option 2: Binary only
h1 Download and install the binary
h3 Option 3: Tarball
h2 Shell completion (optional)
h3 If you installed via .deb
h3 For binary or tarball installs
h2 Building from source
h2 Global config (optional)
h2 Troubleshooting
code-fence sh
code-fence plain
```

### docs/INSTALL-WINDOWS.md

```
h1 Installation — Windows
h2 From a pre-built release (recommended)
h3 Option 1: Chocolatey package (recommended)
h1 Download the .nupkg from GitHub Releases, then install
h3 Option 2: Manual binary installation
h2 PowerShell completion (optional)
h3 If you installed via Chocolatey
h3 For manual binary install
h2 Building from source
h3 Using CMake (all platforms)
h3 Using Visual Studio (Windows native)
h2 Global config (optional)
h2 Troubleshooting
code-fence powershell
code-fence plain
```

### docs/RELEASE.md

```
h1 Release Process
h2 Overview
h2 Release Checklist
h3 1. Prepare on feature branch
h3 2. Create PR and merge to master
h1 Create PR from feature branch → master
h1 Review, then merge to master
h3 3. Tag and release from master
h3 4. GitHub Actions builds and publishes
h2 What gets released
h2 Version numbering
h2 Automation notes
code-fence sh
code-fence plain
code-fence cmake
```

### docs/DESIGN.md

```
h1 projot — Design Specification (v0.1)
h2 1. Overview
h2 2. Goals (v0.1)
h2 3. Non-Goals (v0.1)
h2 4. Core Concepts
h3 4.1 Project Identity
h3 4.3 Todo Identity
h3 4.2 Project Content
h2 5. File & Directory Layout
h3 5.1 Repo Root Discovery
h3 5.2 Config Location
h2 6. Config File
h3 6.1 Format
h3 6.2 Fields
h3 6.3 Config Example (`.projot/config`)
h1 projot config
h1 --- Repo-level fields (set by `init`) ---
h1 Application ID
h1 GitHub repositories (projot-managed)
h1 Swagger/OpenAPI endpoints (projot-managed)
h1 Blizzard links (projot-managed)
h1 --- Project-level fields (set by `new`) ---
h1 RPM project number
h1 Project name
h1 iTrack ticket number
```

## mcp

### mcp/server.js

```
function execCommand(cmd)
function getConfigValue(key)
function getGlobalConfigValue(key)
function slugifyBranchName(str)
function handleRequest(request)
```

### mcp/README.md

```
h1 Projot MCP Server
h2 Why Use This?
h2 Installation
h3 Prerequisites
h3 Configure Your IDE
h4 Claude Code
h4 Copilot (VS Code)
h3 Verify Setup
h2 Tools
h3 `get_open_todos`
h3 `complete_todo`
h3 `add_todo`
h3 `add_note_to_todo`
h3 `open_itrack`
h3 `setup_new_project`
h3 Open Links
h3 Set Links
h2 Testing
h1 Test the server responds
h1 Try it interactively (requires projot in PATH)
h1 Then send a JSON request on stdin, e.g.:
h1 {"method":"initialize"}
h2 How It Works
code-fence json
code-fence plain
```

## scripts

### scripts/package.sh

```
# Package .deb and tar.gz from a staged install tree
```

### scripts/install-completion.sh

```
# scripts/install-completion.sh
```

## src

### src/commands.cpp

```
struct Context
struct AzureTypeDef
load_context() → static Context
if(!root)
if(!result.ok)
if(ctx.config.config_version > PROJOT_CONFIG_SCHEMA_VERSION)
if(ctx.config.config_version == 0)
if(global_path)
config_path_str(const Context& ctx) → static std::string
notes_path_str(const Context& ctx) → static std::string
require_project(const Context& ctx) → static bool
is_safe_rpm(const std::string& s) → static bool
for(char c : s)
install_hook_impl(const fs::path& repo_root, bool& appended, std::string& error) → static bool
if(ec)
if(exists)
if(ec)
binary_dir() → static std::optional<fs::path>
find_mcp_source_dir() → static std::optional<fs::path>
for(const fs::path& raw : { *dir / ".." / "mcp", *dir / ".." / "share" / "projot" / "mcp"})
cmd_init(const Args& args) → int
if(args.help_requested)
if(!root)
if(!result.ok)
cmd_new(const Args& args) → int
```

### src/config.cpp

```
trim(const std::string& s) → std::string
split_list(const std::string& value) → std::vector<std::string>
join_list(const std::vector<std::string>& items) → std::string
dedup(const std::vector<std::string>& v) → static std::vector<std::string
for(const auto& s : v)
parse_azure_entry(const std::string& s) → AzureEntry
format_azure_entry(const AzureEntry& e) → std::string
is_list_key(const std::string& key) → static bool
parse_config(const std::string& path, Config& out) → ParseResult
if(key == "config_version")
catch(...)
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

### src/repo.cpp

```
find_repo_root(const std::filesystem::path& start) → std::optional<std::filesystem:
while(true)
if(parent == current)
global_config_path() → std::optional<std::filesystem:
```

### src/cli.cpp

```
parse_args(int argc, char* argv[]) → Args
if(first == "--help" || first == "-h")
if(first == "--version" || first == "-v")
if(!argv[i] || argv[i][0] != '-')
while(i < argc)
if(arg == "--help" || arg == "-h")
```

### src/cli.h

```
struct Args
has(const std::string& key) → bool
get(const std::string& key, const std::string& def = "") → std::string
get_all(const std::string& key) → std::vector<std::string>
boolean_flags() → inline const std::set<std::str
```

### src/markdown.cpp

```
starts_with(const std::string& s, const std::string& prefix) → static bool
parse_todo_line(const std::string& line, int& id, bool& completed, std::string& text) → static bool
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
dedup_urls(const std::vector<std::string>& v) → static std::vector<std::string
for(const auto& s : v)
render_markdown(const Config& cfg, const std::vector<Todo>& todos) → std::string
for(const auto& key : cfg.links)
if(has_managed)
for(const auto& sec : azure_sections)
if(any_azure)
for(const auto& sec : azure_sections)
for(const auto& raw : deduped)
for(const auto& todo : todos)
for(const auto& note : todo.notes)
render_to_file(const std::string& path, const Config& cfg, const std::vector<Todo>& todos) → RenderResult
```

### src/renderer.h

```
struct RenderResult
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
if(t->completed)
add_note(std::vector<Todo>& todos, int id, const std::string& note) → TodoResult
if(t->completed)
```

### src/todo.h

```
struct Todo
struct TodoResult
```

### src/utils.h

```
date_today() → inline std::string
```

## tests

### tests/test_commands.cpp

```
struct TempRepo
  cmd_init(a) → return
  cmd_new(a) → return
TempRepo(const std::string& name) → explicit
init(const std::string& app_id = "TestApp") → int
new_project(const std::string& rpm = "12345", const std::string& name = "Test Project", const std::string& itrack = "67890", bool no_hook = true) → int
make_args(const std::string& sub, std::initializer_list<std::pair<std::string,std::string>> flags = {}) → static Args
CHECK(cfg.github[0] == "https: } TEST_CASE("init_with_multiple_github")
CHECK(cfg.link_urls["teams"] == "https: } TEST_CASE("new_fails_if_rpm_set")
CHECK(proj.github_urls[0] == "https: } TEST_CASE("close_clears_project_fields")
CHECK(cfg.link_urls["teams"] == "https: } TEST_CASE("set_link_update_key")
CHECK(cfg.azure_subscription[0] == "MySub|https: } TEST_CASE("add_azure_private_dns_url_only")
CHECK(cfg.azure_private_dns[0] == "https: } TEST_CASE("add_azure_deduplicates")
CHECK(content.find("[my-kv](https: } TEST_CASE("add_azure_multiple_of_same_type")
CHECK(content.find("[PROD](https: CHECK(content.find("[NPRD](https: } TEST_CASE("render_updates_file")
```

### tests/data/notes/basic.md

```
h1 Project: Test Project
h2 Links
h2 Todos
```

### tests/data/notes/managed_sections.md

```
h1 Project: Managed Sections Project
h2 Links
h2 GitHub
h2 Swagger
h2 Blizzard
h2 Todos
```

### tests/data/notes/multi_todo.md

```
h1 Project: Multi Todo Project
h2 Links
h2 Todos
```

### tests/data/notes/no_todos.md

```
h1 Project: Empty Project
h2 Links
h2 Todos
```

### tests/doctest.h

```
struct enable_if
struct true_type
struct false_type
struct remove_reference
struct remove_const
struct is_enum
struct underlying_type
struct has_insertion_operator
struct should_stringify_as_underlying_type
struct StringMakerBase
  static_assert(deferred_false<T>::value, "No stringification detected for type T. See string conversion manual")
struct filldata
struct ContextOptions
struct Expression_lhs
class ExceptionTranslator
class ContextScope
  lambda_(s)
  destroy()
struct QueryData
struct Timer
  start() → void
  getElapsedMicroseconds() → unsigned int
  getElapsedSeconds() → double
class MultiLaneAtomic
  fetch_add(1) → return
```

### tests/test_config.cpp

```
write_temp(const std::string& content) → static std::string
CHECK(cfg.link_urls["teams"] == "https: } TEST_CASE("parse_repo_only")
CHECK(cfg.github[0] == "https: CHECK(cfg.github[2] == "https: } TEST_CASE("parse_list_single")
CHECK(cfg.github[0] == "https: } TEST_CASE("parse_list_empty")
CHECK(cfg.link_urls["teams"] == "https: } TEST_CASE("parse_label_dotted_key")
CHECK(e.url == "https: } TEST_CASE("parse_azure_entry_url_only")
CHECK(e.url == "https: } TEST_CASE("parse_azure_entry_whitespace_trimmed")
CHECK(e.url == "https: } TEST_CASE("format_azure_entry_with_name")
CHECK(cfg.azure_private_dns[0] == "https: } TEST_CASE("write_azure_round_trip")
```

### tests/test_errors.cpp

```
struct ErrorTempRepo
ErrorTempRepo(const std::string& name, bool with_git = true) → explicit
make_err_args(const std::string& sub, std::initializer_list<std::pair<std::string,std::string>> flags = {}) → static Args
```

### tests/test_hook.cpp

```
struct HookTempRepo
  cfg(path / ".projot" / "config") → std::ofstream
  notes(path / ".projot" / "1.md") → std::ofstream
  f(p) → std::ifstream
HookTempRepo(const std::string& name) → explicit
hook_path() → fs::path
read_file(const fs::path& p) → static std::string
make_hook_args(const std::string& sub) → static Args
```

### tests/test_markdown_parser.cpp

```
CHECK(proj.link_entries[0].second == "https: } TEST_CASE("parse_github_section")
CHECK(proj.github_urls[0] == "https: CHECK(proj.github_urls[1] == "https: } TEST_CASE("parse_swagger_section")
CHECK(proj.swagger_urls[0] == "https: } TEST_CASE("parse_blizzard_section")
CHECK(proj.blizzard_urls[0] == "https: } TEST_CASE("parse_missing_managed_sections")
```

### tests/test_renderer.cpp

```
make_base_config() → static Config
contains(const std::string& haystack, const std::string& needle) → static bool
CHECK(contains(output, "- Teams: https: CHECK(contains(output, "- iTrack: https: } TEST_CASE("render_links_na_when_missing")
CHECK(contains(output, "- https: } TEST_CASE("render_swagger_section")
CHECK(contains(output, "- https: } TEST_CASE("render_blizzard_section")
CHECK(contains(output, "- https: } TEST_CASE("render_omits_empty_github")
CHECK(contains(output, "[MySub](https: } TEST_CASE("render_azure_section_url_only")
CHECK(contains(output, "- https: } TEST_CASE("render_azure_all_types")
```

### tests/test_repo_discovery.cpp

```
make_temp_dir(const std::string& name) → static fs::path
cleanup(const fs::path& p) → static void
```

### tests/test_todo_model.cpp

```
make_todos() → static std::vector<Todo>
```
