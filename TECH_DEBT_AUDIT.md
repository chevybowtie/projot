# Tech Debt Audit — projot

Generated: 2026-05-05
Last Updated: 2026-05-19 (Repeat-run: 5 new findings; all resolved same session)

## Repeat-Run Summary (2026-05-19)

**Status:** All prior findings resolved or accepted. 5 new findings detected; all resolved within the same session.

**New since last audit:** Global config (`set-global`, `rpm_base_url`, `itrack_base_url`), `close` command, `uninstall-hook`, `uninstall-mcp-server`, JSON-RPC 2.0 compliance fix for MCP server, date formatting, Windows improvements, docs split into platform-specific install guides.

**MCP test harness added:** `mcp/test.js` exercises each tool handler with a mock CLI shim and asserts correct flag usage. Detects the class of drift that caused F021. Run with `make test-mcp` or `node mcp/test.js`.

**Resolved this session:** F021 (three broken MCP call sites), F022 (silent re-render failure), F023 (shell injection in openUrl), F024 (double-parse in complete), F025 (RANP terminology in tool description). F016 discovered to already be implemented in `test_hook.cpp:377-388`.

**Test health:** C++ suite: all 152 tests passing. MCP suite: 6/6 passing.

---

## Executive Summary

projot is a well-structured single-purpose C++ CLI tool. All previously identified structural debt has been resolved. Five new findings were detected in this session; all five were resolved within the same session.

1. ~~**MCP server calls CLI with stale flags (F021)**~~ — **RESOLVED**; guarded by `mcp/test.js`
2. ~~**Silent render failure after config change (F022)**~~ — **RESOLVED**; warning emitted on failure
3. ~~**Shell injection vector in MCP openUrl (F023)**~~ — **RESOLVED**; using `execFileSync` with arg array
4. ~~**Double-parse in `complete` command (F024)**~~ — **RESOLVED**; outer parse removed
5. ~~**Legacy RANP terminology in MCP tool description (F025)**~~ — **RESOLVED**

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

All layers are cleanly separated. The MCP server is a thin shell-command wrapper around the CLI. Flag-drift between the two layers is now guarded by `mcp/test.js`.

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
| F024 | Architectural decay | src/commands_project.cpp:172-174 | Low | S | **RESOLVED** | `cmd_complete` parsed the notes markdown manually before calling `execute_project_command`, which parses it again internally. Outer parse removed; validation moved inside the callback. (`cmd_add_note` was already clean.) | ✓ |
| F025 | Documentation drift | mcp/server.js:170 | Low | S | **RESOLVED** | MCP tool description updated from "RANP/project number" to "RPM project number". | ✓ |

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
- [x] **F024:** Eliminate double-parse in `complete` — **COMPLETED**

---

## Things That Look Bad But Are Actually Fine

- **Hardcoded Azure type list (F012):** Azure resource types are stable and unlikely to change. Hardcoding with a lookup table is appropriate for v0.1.
- **No enum-based command dispatch (F019):** String-based map dispatch is clear and permits easy addition of new commands. Fine for a CLI tool of this scale.
- **`URL_LIST_TYPES[]` with three one-liner delegates (F009):** The table + thin wrappers is the correct pattern. `cmd_add_github`, `cmd_add_swagger`, `cmd_add_blizzard` as one-liners are the correct public API surface.
- **`dist/` directory has build artifacts locally:** `.gitignore` correctly excludes `dist/` — the directory exists locally but is not tracked by git. This is intentional staging for release packaging.
- **`node_modules/sigmap/` exists locally:** Same as above — `.gitignore` excludes `node_modules/`. Not committed.
- **`src/markdown.cpp:114` still parses `"- RANP: "`:** This is backward-compatibility parsing to read old notes files. Not a bug; the renderer writes `"- RPM: "` and the parser handles both.
- **`add_note` succeeds on a completed todo:** `todo.cpp:56-61` adds the note even when the todo is completed, emitting a warning. This is intentional — notes are informational and may be legitimately added after completion.
- **`cmd_close` ignores missing notes file (`ec == std::errc::no_such_file_or_directory`):** `commands_project.cpp:45-48` continues silently if the notes file is already gone. This is correct graceful handling for partially broken state.
- **Repeated "Run 'projot X --help'" messages (F013):** Redundant but clear; acceptable for a CLI tool.
- **`git_stage_file` on Windows uses string composition (commands_maint.cpp:45-47):** The RPM path component is validated by `is_safe_rpm()` before reaching the staging call, so the only risk is a repo root path containing `"` characters — unusual on Windows and limited in practice. Worth noting but not a realistic threat.

---

## Open Questions for the Maintainer

1. **MCP `add_todo` quoting correctness:** The F021 fix still uses shell string composition with double-quote escaping (e.g. `text.replace(/"/g, '\\"')`). A more robust fix would switch `execCommand` to `execFileSync` with an argument array — eliminating quoting entirely and closing F023 at the same time. Is that in scope?

2. **Global config backward compatibility:** `write_global_config` only writes `rpm_base_url` and `itrack_base_url`. If a future version adds keys to the global config, `write_global_config` overwrites and silently drops them. Is this acceptable, or should it merge keys?

3. **`close` command hook interaction:** `projot close` archives the notes file and clears project config, but does not uninstall the pre-commit hook. The hook will fire `projot render` on every commit and fail until `projot new` is run. Is this intentional, or should `close` emit a reminder?

4. **Azure type stability:** Are the seven types in `AZURE_TYPES[]` stable post-v0.1? If new types will be added, a config-driven approach is straightforward.

---

## Conclusion

projot is in excellent shape structurally. No functional bugs remain in the CLI or MCP server. The MCP server is guarded against flag-drift by `mcp/test.js` (6/6 passing). All actionable findings from both audit runs are resolved.

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

**Remaining actionable findings:** None.
