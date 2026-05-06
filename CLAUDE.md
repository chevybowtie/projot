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

**C++17 is mandatory.** Do not use C++20 features (ranges, concepts, requires). Compiler must support C++17; build will fail on GCC < 7 or Clang < 5 by design.

**No external dependencies.** projot forbids Boost, fmt, nlohmann/json, and other libraries. Use only `<filesystem>`, `<optional>`, `<string>`, `std::stringstream`, and standard library only. String parsing is manual; JSON is parsed with `std::string::find()` and `std::stringstream`. This is a hard constraint enforced in code review.

**doctest header is checked in.** Tests use `tests/doctest.h` (363 KB, included directly). To upgrade: download the latest doctest.h from https://github.com/doctest/doctest/releases and replace the file.

## Command Architecture

All 15 commands are split across three files for clarity:
- `src/commands_project.cpp` — project operations (add-todo, list, complete, add-note, set-link)
- `src/commands_config.cpp` — configuration (init, new, set-app-id, add-github/swagger/blizzard, add-azure)
- `src/commands_maint.cpp` — maintenance (render, install-hook, install-mcp-server)

**To add a new command:**
1. Implement `int cmd_<name>(const Args& args)` in the appropriate file.
2. Declare it in `src/commands.h`.
3. Register in `src/main.cpp` in both the `commands` map (line ~75) and `valid_flags` map (lines ~100–110).

**Command pattern:** Most commands use one of two helpers:
- `execute_project_command(ctx, [&](Project& proj) { ... })` — for modifying todos in-memory, parsing, and rendering.
- `execute_config_command(ctx, [&](Context& c) { ... })` — for modifying config, saving, and optionally re-rendering.

Both helpers handle error checking, file I/O, and success messages. Use them for consistency; avoid custom logic unless the command is stateless (like `list` or `render`).

**Special return codes:** Commands that use `execute_project_command` treat `ParseResult{false, "already_completed"}` as a warning, not an error (returns 0, not 1). This is used by `complete` to warn if a todo is already marked done.

## Git Workflow

**Worktree hazard:** Active worktree on `feature/optimization` occupies a branch lock. Before switching branches in the main repo, check `git worktree list`. If the worktree is stale, use `git worktree remove`. Do not rely on `git checkout` to move between branches if a worktree holds a lock.

**Commit format is conventional, not enforced.** Follow the pattern from history: `refactor: ...`, `docs: ...`, `fix: ...`, `feat: ...`, `test: ...`, `chore: ...`. No linter enforces this; it is a team convention.

## Critical Gotchas

**std::system() is fragile.** Git operations use `std::system("git -C ... add ...")` to stage files (src/commands_maint.cpp:91). The RPM is validated, but the path is trusted; shell injection is theoretically possible if `.projot/config` is malformed. Do not add new `std::system()` calls; prefer direct file operations or a git library (post-v0.1 planned in TECH_DEBT_AUDIT.md F005).

**MCP server requires Node.js.** The `install-mcp-server` command checks for Node.js with a shell invocation. Node must be on PATH. If missing, the tool warns but doesn't fail. Test locally with `which node` before relying on MCP integration.

**Pre-commit hook uses string markers.** The hook is idempotent; it checks for a BEGIN/END marker to avoid duplication. If manually edited and markers are broken, the tool will re-append, creating duplicates. Do not hand-edit `.git/hooks/pre-commit`.

**Config versioning is forward-compatible.** If a newer projot version writes `config_version=2`, older binaries will refuse to run (clear error). Downgrades to older binaries may fail silently. Document format changes in TECH_DEBT_AUDIT.md and increment `PROJOT_CONFIG_SCHEMA_VERSION` in CMakeLists.txt.

**Azure resource types are hardcoded.** Adding a new Azure type (e.g., "vm", "container-registry") requires editing `src/commands_config.cpp` and recompiling. This is acceptable for v0.1 (types are stable); post-v0.1 could use a config file.

## Testing

All 152 tests pass. Use the `TempRepo` helper in `tests/test_commands.cpp` to set up temporary git repos for testing. Test data lives in `tests/data/configs/` and `tests/data/notes/`. Reference it with the `PROJOT_TEST_DATA_DIR` macro (set at CMake time to an absolute path).

Tests verify config parsing, markdown I/O, command execution, versioning, error handling, and hooks. Coverage is comprehensive for the happy path; error cases (render failures, permission errors) have minimal coverage (noted in TECH_DEBT_AUDIT.md F008).

## Documentation Consolidation

This file (`CLAUDE.md`) is now the canonical source for operational guidance. The older `.instructions.md` and `.copilot-instructions.md` files remain as legacy references but should not be updated separately. If guidance changes, update only this file.
