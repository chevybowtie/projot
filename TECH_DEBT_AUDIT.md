# Tech Debt Audit — projot

Generated: 2026-05-05
Last Updated: 2026-05-24 (Repeat-run: 3 new findings detected and resolved same session)

## Repeat-Run Summary (2026-05-24)

**Status:** All prior findings remain resolved. 3 new findings detected and resolved within the same session.

**Changes since 2026-05-19:** Minimal. The `feature/rendered-doc` branch has 2 commits ahead of master, both documentation-only (`doc: fix path`). No functional changes to CLI or MCP server.

**Resolved this session:** F026+F027 (replaced all `execCommand` shell-string calls with `execArgs`/`execFileSync` arg arrays in `mcp/server.js`; removed unused `execSync` import). F028 (added 9 new tool smoke tests to `mcp/test.js`; fixture config extended with `github`, `swagger`, `blizzard`, `link.teams`, `link.rpm` entries).

**Test health:** C++ suite: all 152 tests passing. MCP suite: 15/15 passing (was 6/6 on 6 tools; now 15 tests covering all 15 tools).

---

## Repeat-Run Summary (2026-05-19)

**Status:** All prior findings resolved or accepted. 5 new findings detected; all resolved within the same session.

**New since last audit:** Global config (`set-global`, `rpm_base_url`, `itrack_base_url`), `close` command, `uninstall-hook`, `uninstall-mcp-server`, JSON-RPC 2.0 compliance fix for MCP server, date formatting, Windows improvements, docs split into platform-specific install guides.

**MCP test harness added:** `mcp/test.js` exercises each tool handler with a mock CLI shim and asserts correct flag usage. Detects the class of drift that caused F021. Run with `make test-mcp` or `node mcp/test.js`.

**Resolved this session:** F021 (three broken MCP call sites), F022 (silent re-render failure), F023 (shell injection in openUrl), F024 (double-parse in complete), F025 (RANP terminology in tool description). F016 discovered to already be implemented in `test_hook.cpp:377-388`.

**Test health:** C++ suite: all 152 tests passing. MCP suite: 6/6 passing.

---

## Executive Summary

projot is a well-structured single-purpose C++ CLI tool. All structural debt is resolved. The MCP server still uses a shell-based exec pattern for most tool invocations, creating a persistent but bounded injection risk. Three new findings this run: two are promotions of the previously open shell-injection question, one is a test-coverage gap.

~~**MCP `setup_new_project` passes user args unescaped into shell (F026)**~~ — **RESOLVED**; `execArgs`/`execFileSync` arg array
~~**Incomplete shell quoting in `add_todo`, `add_note_to_todo`, `set_*_link` (F027)**~~ — **RESOLVED**; same fix as F026
~~**MCP test suite covers 6 of 15 registered tools (F028)**~~ — **RESOLVED**; 9 new smoke tests added

~~Previous findings (F001–F025): all resolved or accepted.~~

---

## Architectural Mental Model

projot is a **single-purpose repo-centric CLI tool** that:

- Discovers the git repo root and loads `.projot/config` for all operations
- Represents projects as markdown files (`.projot/{RPM}.md`) with a strict structure
- Implements 19 subcommands with uniform patterns: load config → validate → parse markdown → modify in-memory model → render to disk → optionally stage in git
- Installs a git hook (`pre-commit`) that auto-renders the notes file before every commit
- Provides MCP server integration for AI assistants (Claude Code, VS Code Copilot)
- Supports a global config (`~/.config/projot/config`) that provides `rpm_base_url` and `itrack_base_url` defaults across all projects

The codebase has five layers:

1. **CLI parsing** (`cli.cpp/h`) — arg tokenization and flag collection
2. **Config/persistence** (`config.cpp/h`) — config file parsing, global config
3. **Markdown I/O** (`markdown.cpp/h`, `renderer.cpp/h`) — parsing and rendering project files
4. **Command implementations** (`commands_*.cpp`, `commands_shared.cpp`) — per-subcommand logic and shared helpers
5. **MCP server** (`mcp/server.js`) — Node.js JSON-RPC 2.0 bridge between AI assistants and the CLI

All layers are cleanly separated. The MCP server is a thin shell-command wrapper around the CLI. The `openUrl` function uses `execFileSync` with argument arrays (no shell). All other MCP invocations still use `execSync` with template-literal shell strings, creating a structural inconsistency flagged below.

---

## Findings

| ID | Category | File:Line | Severity | Effort | Status | Description | Recommendation |
|----|----------|-----------|----------|--------|--------|-------------|----------------|
| F001 | Architectural decay | src/commands.cpp | High | M | **RESOLVED** | Split into 5 focused files | ✓ |
| F002 | Consistency rot | src/config.cpp, renderer.cpp | Medium | S | **RESOLVED** | Dedup logic unified in utils.h | ✓ |
| F003 | Architectural decay | src/commands.cpp | High | M | **RESOLVED** | `execute_project_command`/`execute_config_command` extracted | ✓ |
| F004 | Error handling | src/commands.cpp | High | M | **RESOLVED** | Silent error skips eliminated | ✓ |
| F005 | Security hygiene | src/commands_maint.cpp | Medium | M | **RESOLVED** | `std::system()` replaced with `fork()+execvp()`/`CreateProcess` | ✓ |
| F006 | Documentation drift | src/commands.h | Medium | S | **RESOLVED** | Docstrings added to all command declarations | ✓ |
| F007 | Consistency rot | src/commands.cpp | Medium | S | **RESOLVED** | Legacy `--text` flag removed from add-todo and add-note CLI | ✓ |
| F008 | Test debt | tests/test_commands.cpp | Medium | M | **RESOLVED** | Error-path tests added | ✓ |
| F009 | Architectural decay | src/commands_config.cpp | Medium | S | **RESOLVED** | `URL_LIST_TYPES[]` lookup table; one-liner delegates | ✓ |
| F010 | Consistency rot | src/commands_shared.cpp | Medium | S | **RESOLVED** | Merged into `projot_file_path()` helper | ✓ |
| F011 | Type & contract debt | src/commands_config.cpp | Medium | S | **RESOLVED** | `URL_LIST_TYPES[]` with explicit "not found" error | ✓ |
| F012 | Dependency & config debt | src/commands_config.cpp | Low | S | **ACCEPTED** | Azure types hardcoded; acceptable for v0.1 | — |
| F013 | Error handling | src/commands_*.cpp | Low | S | **ACCEPTED** | Per-command help text; acceptable redundancy | — |
| F014 | Consistency rot | src/commands_shared.cpp | Low | S | **ACCEPTED** | Single-use Context struct; improves readability | — |
| F015 | Documentation drift | src/markdown.h | Low | S | **RESOLVED** | RANP→RPM docstring updated | ✓ |
| F016 | Test debt | tests/test_hook.cpp:377-388 | Low | M | **RESOLVED** | Hook test `install_hook_unwritable_dir` already covered this case | ✓ |
| F017 | Consistency rot | src/config.cpp | Low | S | **RESOLVED** | Legacy `ranp` config key parsing removed | ✓ |
| F018 | Performance | src/renderer.cpp | Low | S | **ACCEPTED** | Azure section empty-check inside render loop; negligible | — |
| F019 | Architectural decay | src/main.cpp | Low | S | **ACCEPTED** | Per-subcommand valid_flags table; clean, no pressing need to change | — |
| F020 | Security hygiene | src/commands_maint.cpp | Low | S | **ACCEPTED** | `is_safe_rpm()` as defence-in-depth post-F005; no shell used now | — |
| F021 | Consistency rot | mcp/server.js:301,307,362 | **Critical** | S | **RESOLVED** | MCP server called CLI with removed/renamed flags. Fixed: positional arg for add-todo/add-note; `--rpm` for new. Guarded by `mcp/test.js`. | ✓ |
| F022 | Error handling | src/commands_shared.cpp:113-115 | Medium | S | **RESOLVED** | `execute_config_command` silently dropped re-render failures. Fixed: check `render_to_file` result and emit `std::cerr << "warning: ..."` on failure (non-fatal). | ✓ |
| F023 | Security hygiene | mcp/server.js:284 | Medium | S | **RESOLVED** | Shell injection in `openUrl`. Replaced `execSync` with `execFileSync` and argument array; Windows uses `cmd.exe /c start "" url`. | ✓ |
| F024 | Architectural decay | src/commands_project.cpp:172-174 | Low | S | **RESOLVED** | `cmd_complete` parsed the notes markdown manually before calling `execute_project_command`, which parses it again internally. Outer parse removed; validation moved inside the callback. | ✓ |
| F025 | Documentation drift | mcp/server.js:170 | Low | S | **RESOLVED** | MCP tool description updated from "RANP/project number" to "RPM project number". | ✓ |
| F026 | Security hygiene | mcp/server.js:361-364 | **Medium** | S | **RESOLVED** | `setup_new_project` embedded `branch_name`, `project_number`, and `itrack_number` into shell strings via `execCommand`. Fixed: `git checkout -b` and `projot new` now use `execArgs`/`execFileSync` with argument arrays; `teamsUrl` pushed as a separate array element. | ✓ |
| F027 | Security hygiene | mcp/server.js:303,309,369-384 | **Low** | S | **RESOLVED** | All remaining `execCommand` shell-string callsites (`add_todo`, `add_note_to_todo`, `set_*_link`) replaced with `execArgs`/`execFileSync` arg arrays. `execCommand` function and unused `execSync` import removed. | ✓ |
| F028 | Test debt | mcp/test.js | **Low** | S | **RESOLVED** | Added 9 smoke tests covering all previously untested tools: `set_teams_link`, `set_github_link`, `set_swagger_link`, `set_blizzard_link`, `open_github`, `open_swagger`, `open_blizzard`, `open_teams`, `open_rpm`. Fixture config extended with URLs for those tools. Suite: 15/15 passing. | ✓ |

---

## Top 5 "If You Fix Nothing Else, Fix These"

### 1. **F021 — Fix three broken MCP tool call sites** ⭐ **COMPLETED**

Three MCP tools silently failed because the CLI flag API changed (F007) but the MCP server was not updated.

Fixed: positional arg for `add-todo` and `add-note`; `--rpm` instead of `--ranp` for `new`. Regression-guarded by `mcp/test.js` (5 tests covering all tool call sites).

### 2. **F003 — Extract command boilerplate** ⭐ **COMPLETED**

*Status: Resolved. All 13 config/project-modifying commands use helpers.*

### 3. **F004 — Remove silent error skips** ⭐ **COMPLETED**

*Status: Resolved as a side effect of F003.*

### 4. **F002 — Deduplicate utility functions** ⭐ **COMPLETED**

*Status: Resolved. `deduplicate<T>` template in utils.h.*

### 5. **F005 — Replace std::system calls** ⭐ **COMPLETED**

*Status: Resolved. `git_stage_file()` uses fork+execvp (POSIX) / CreateProcess (Windows). `node_available()` walks PATH directly.*

---

## Quick Wins

Low-effort, medium+ severity improvements:

- [x] **F006:** Docstrings on command declarations — **COMPLETED**
- [x] **F015:** RANP→RPM docstring update — **COMPLETED**
- [x] **F017:** Remove legacy `ranp` config key parsing — **COMPLETED**
- [x] **F007:** Remove `--text` flag from add-todo and add-note — **COMPLETED**
- [x] **F010:** Merge path builders into `projot_file_path()` — **COMPLETED**
- [x] **F021:** Fix three broken MCP call sites — **COMPLETED**
- [x] **F025:** Update MCP tool description "RANP" → "RPM" — **COMPLETED**
- [x] **F022:** Emit warning when re-render fails in `execute_config_command` — **COMPLETED**
- [x] **F023:** Use `execFileSync` array API in MCP `openUrl` — **COMPLETED**
- [x] **F024:** Eliminate double-parse in `cmd_complete` — **COMPLETED**
- [x] **F026:** Switch `setup_new_project` git/projot calls to `execFileSync` arg arrays — **COMPLETED**
- [x] **F027:** Replace shell-interpolated `execCommand` with `execFileSync` for all user-controlled inputs — **COMPLETED**
- [x] **F028:** Add smoke tests for all 9 previously untested MCP tools — **COMPLETED**

---

## Things That Look Bad But Are Actually Fine

- **Hardcoded Azure type list (F012):** Azure resource types are stable and unlikely to change. Hardcoding with a lookup table is appropriate for v0.1.
- **No enum-based command dispatch (F019):** String-based map dispatch is clear and permits easy addition of new commands. Fine for a CLI tool of this scale.
- **`URL_LIST_TYPES[]` with three one-liner delegates (F009):** The table + thin wrappers is the correct pattern. `cmd_add_github`, `cmd_add_swagger`, `cmd_add_blizzard` as one-liners are the correct public API surface.
- **`dist/` directory has build artifacts locally:** `.gitignore` correctly excludes `dist/` — the directory exists locally but is not tracked by git. This is intentional staging for release packaging.
- **`node_modules/sigmap/` exists locally:** Same as above — `.gitignore` excludes `node_modules/`. Not committed. `sigmap` is a devDependency for context generation (`package.json:3`), not part of the MCP server.
- **`src/markdown.cpp:114` still parses `"- RANP: "`:** This is backward-compatibility parsing to read old notes files. Not a bug; the renderer writes `"- RPM: "` and the parser handles both.
- **`add_note` succeeds on a completed todo:** `todo.cpp:56-61` adds the note even when the todo is completed, emitting a warning. This is intentional — notes are informational and may be legitimately added after completion.
- **`cmd_close` ignores missing notes file (`ec == std::errc::no_such_file_or_directory`):** `commands_project.cpp:45-48` continues silently if the notes file is already gone. This is correct graceful handling for partially broken state.
- **Repeated "Run 'projot X --help'" messages (F013):** Redundant but clear; acceptable for a CLI tool.
- **`git_stage_file` on Windows uses string composition (commands_maint.cpp:45-47):** The RPM path component is validated by `is_safe_rpm()` before reaching the staging call, so the only risk is a repo root path containing `"` characters — impossible on Windows by filesystem convention.
- **`execCommand` in `get_open_todos` and `complete_todo` (mcp/server.js:292,297):** The CLI arguments here are either entirely fixed (`projot list --open`) or a numeric ID (`--todo ${todo_id}`). A numeric ID from an MCP JSON payload is already type-checked (`type: "number"` in the schema); JavaScript won't produce a string from `args.todo_id` that contains shell metacharacters. This specific usage is safe.
- **`format_date` replacement order (utils.h:42-44):** Replaces YYYY, then MM, then DD. Could theoretically reprocess — e.g., if the year value contained "MM" — but since years are four-digit numbers (2026, etc.) and months/days are zero-padded two-digit numbers (01–12, 01–31), none of the replacement values contain the other tokens. Order is irrelevant; this is safe.
- **`write_global_config` only writes two keys (config.cpp:240-261):** Appears to drop unknown keys on round-trip. However, `cmd_set_global` always calls `parse_config()` before writing, which reads the existing file into `cfg`. `write_global_config` then writes `rpm_base_url` and `itrack_base_url` from that merged struct. So both known keys are preserved. The concern is only about *future* keys unknown to the current binary — documented in Open Question #2 below.

---

## Open Questions for the Maintainer

1. ~~**MCP `add_todo` quoting correctness:**~~ Promoted to **F026** and **F027**; both resolved. All `execCommand` call sites replaced with `execArgs`/`execFileSync` arg arrays. `execCommand` and the unused `execSync` import removed entirely.

2. **Global config backward compatibility:** `write_global_config` only writes `rpm_base_url` and `itrack_base_url`. If a future binary version adds keys to the global config schema, running `projot set-global` with the older binary will overwrite and drop those keys (it reads both keys into `cfg` then writes only those two). Is forward-compatible global config merging needed for post-v0.1?

3. **`close` command hook interaction:** `projot close` archives the notes file and clears project config, but does not uninstall the pre-commit hook. The hook will fire `projot render` on every commit and fail until `projot new` is run. Is this intentional, or should `close` emit a reminder ("Note: pre-commit hook still installed — run 'projot install-hook' after starting the next project, or 'projot uninstall-hook' to remove it")?

4. **Azure type stability:** Are the seven types in `AZURE_TYPES[]` stable post-v0.1? If new types will be added, a config-driven approach is straightforward.

---

## Conclusion

projot is in excellent structural shape. No functional bugs in the CLI or MCP server. All findings from all three audit runs are resolved.

**Completed (cumulative):**

High-impact:
1. ✓ Extract command boilerplate (F003)
2. ✓ Remove silent error skips (F004)
3. ✓ Deduplicate utils (F002)
4. ✓ Reduce file size (F001)
5. ✓ Replace `std::system` calls (F005)

Quick wins:
6. ✓ Add docstrings (F006)
7. ✓ Update RANP→RPM (F015)
8. ✓ Remove legacy `ranp` config key (F017)
9. ✓ Remove `--text` flag from CLI (F007)
10. ✓ Merge path builders (F010)
11. ✓ Replace ternary URL-kind selector (F011)
12. ✓ Error-path test coverage (F008)
13. ✓ Fix three broken MCP call sites (F021)
14. ✓ Update MCP "RANP" description text (F025)
15. ✓ Add MCP test harness (`mcp/test.js`)
16. ✓ Warn on re-render failure in `execute_config_command` (F022)
17. ✓ Replace shell-interpolated `openUrl` with `execFileSync` (F023)
18. ✓ Eliminate double-parse in `cmd_complete` (F024)
19. ✓ Unwritable hooks dir test already existed (F016 — discovered resolved)
20. ✓ Replace `execCommand` with `execArgs`/`execFileSync` for all user-controlled MCP inputs (F026+F027)
21. ✓ Add 9 new MCP tool smoke tests; suite now 15/15 (F028)

**Remaining actionable findings:** None.
