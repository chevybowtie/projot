# CLAUDE.md — Test Infrastructure

Guidance for working with projot's test suite.

## Test Framework and Setup

Tests use **doctest** (header-only library in `tests/doctest.h`). All tests are in `tests/test_*.cpp`.

**Build and run:**
```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

**Run a single test file:**
```bash
./build/projot_tests -tc test_commands.cpp
```

**Run tests matching a name:**
```bash
./build/projot_tests -tc "test_commands.cpp" -ts "cmd_"
```

## Test Data Directory

Test data is compiled into the binary as an absolute path via `PROJOT_TEST_DATA_DIR` macro (set in CMakeLists.txt line 79). **Tests must run from repo root:**

```bash
cd /home/paul/Documents/projects/projot
ctest --test-dir build --output-on-failure  # ✓ Correct
```

Do NOT run the binary directly from the build directory unless cwd is the repo root:
```bash
cd /home/paul/Documents/projects/projot
./build/projot_tests  # ✓ Correct

cd build && ./projot_tests  # ✗ Wrong cwd; test data paths will fail
```

Test data lives in:
- `tests/data/configs/` — sample `.projot/config` files
- `tests/data/notes/` — sample markdown files

Reference via `PROJOT_TEST_DATA_DIR` macro:
```cpp
std::string config_path = std::string(PROJOT_TEST_DATA_DIR) + "/configs/basic_config";
```

## TempRepo Helper

Use `TempRepo` to set up a temporary git repo and `.projot` directory for testing.

**Definition** (tests/test_commands.cpp lines 16–25):
```cpp
struct TempRepo {
    TempRepo(const std::string& name);
    ~TempRepo();  // Cleanup: remove all temp files
    fs::path path();  // Returns temp repo root
};
```

**Usage:**
```cpp
TEST_CASE("cmd_add_todo basic") {
    TempRepo repo("add_todo_basic");
    fs::path repo_root = repo.path();
    
    // Repo structure is now:
    // repo_root/.git/        (empty git repo)
    // repo_root/.projot/config
    // repo_root/.projot/{rpm}.md (empty notes file)
    
    // ... test command execution ...
}
```

**Important:** TempRepo uses `fs::temp_directory_path() / "projot_" + name`. If tests crash or are interrupted, temp directories may remain in `/tmp/projot_*` (Linux) or `%TEMP%\projot_*` (Windows). Manual cleanup may be required.

## Common Test Patterns

### Testing a Command with Success Path

```cpp
TEST_CASE("cmd_add_todo success") {
    TempRepo repo("add_todo_success");
    
    // Change to repo directory (commands load context from cwd)
    fs::current_path(repo.path());
    
    // Execute the command
    Args args;
    args.positional.push_back("Learn C++17");
    int rc = cmd_add_todo(args);
    
    // Verify success
    CHECK(rc == 0);
    
    // Verify side effect: todo was added and rendered
    Project proj;
    auto result = parse_markdown((repo.path() / ".projot" / "12345.md").string(), proj);
    CHECK(result.ok);
    CHECK(proj.todos.size() == 1);
    CHECK(proj.todos[0].text == "Learn C++17");
}
```

### Testing Error Handling

```cpp
TEST_CASE("cmd_add_todo missing text") {
    TempRepo repo("add_todo_no_text");
    fs::current_path(repo.path());
    
    Args args;
    args.positional.clear();  // Missing required positional
    
    int rc = cmd_add_todo(args);
    CHECK(rc == 1);  // Should fail
}
```

### Testing Config Modification

```cpp
TEST_CASE("cmd_set_link") {
    TempRepo repo("set_link");
    fs::current_path(repo.path());
    
    Args args;
    args.flags["key"] = "teams";
    args.flags["url"] = "https://teams.microsoft.com/...";
    
    int rc = cmd_set_link(args);
    CHECK(rc == 0);
    
    // Verify config was saved
    Config cfg;
    auto result = parse_config((repo.path() / ".projot" / "config").string(), cfg);
    CHECK(result.ok);
    CHECK(cfg.link_urls["teams"] == "https://teams.microsoft.com/...");
}
```

## Special Return Codes

Two commands use special return codes that look like errors but aren't:

1. **`already_completed`** — used by `complete` when a todo is already marked done
   ```cpp
   return execute_project_command(ctx, [id](Project& proj) {
       auto result = complete_todo(proj.todos, id, date_today());
       if (result.warned) {
           std::cerr << "warning: " << result.message << "\n";
           return ParseResult{false, "already_completed"};  // Returns 0, not 1
       }
       return ParseResult{true, ""};
   });
   ```
   Test: `int rc = cmd_complete(args); CHECK(rc == 0);` (success)

2. **`already_present`** — used by config commands when adding a duplicate entry
   ```cpp
   if (std::find(list.begin(), list.end(), url) != list.end()) {
       return ParseResult{false, "already_present"};  // Returns 0, not 1
   }
   ```
   Test: `int rc = cmd_add_github(args); CHECK(rc == 0);` (idempotent success)

## Test Coverage

Current coverage (from TECH_DEBT_AUDIT.md):
- ✓ Config parsing, writing, versioning
- ✓ Markdown parsing, rendering
- ✓ All 15 commands (happy path)
- ✓ Hook installation (basic)
- ✗ Error cases: parse_markdown failures, render_to_file failures on set-link/add-github/add-azure
- ✗ Unwritable hooks directory scenario

Gaps are noted in TECH_DEBT_AUDIT.md F008. If adding tests, focus on error paths.

## Test Isolation

Each test uses a unique temp repo name to avoid conflicts. Temp directories are cleaned up in TempRepo destructor. Tests run in parallel (ctest); ensure test names are unique and don't share temp directories.

## Doctest Configuration

Doctest is configured in CMakeLists.txt via `target_include_directories()` and is included directly in test files. No configuration file is needed. To upgrade doctest:

1. Download the latest `doctest.h` from https://github.com/doctest/doctest/releases
2. Replace `tests/doctest.h`
3. Rebuild and test

## Common Test Failures

| Error | Cause | Fix |
|-------|-------|-----|
| "no such file or directory" in test data | Test run from wrong cwd or stale CMakeCache.txt | Delete `build/CMakeCache.txt`, reconfigure, and run from repo root |
| "projot_*: Permission denied" in temp dir | Permissions issue on /tmp or %TEMP% | Manual cleanup: `rm -rf /tmp/projot_*` or `rmdir /tmp/projot_*` |
| Tests hang or segfault | Uncaught exception in TempRepo destructor | Check test for missing/incorrect cleanup; ensure Args are initialized properly |
| Random test failures | Temp directory name collision | Ensure test names are unique and don't conflict with other parallel runs |

## Running All Tests with Diagnostics

```bash
# Build
cmake --build build

# Run with verbose output and on-failure diagnostics
ctest --test-dir build --output-on-failure -V
```

This will print each test's output and stop at the first failure, making it easier to debug.
