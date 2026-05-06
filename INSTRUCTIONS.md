# projot Development Standards & Guidelines

This file consolidates operational guidance for developers and AI assistants working on projot.

**Note:** This file supersedes `.instructions.md` and `.copilot-instructions.md`. See CLAUDE.md for technical guidance loaded into Claude Code sessions.

---

## Core Principles

### 1. No External Dependencies
- **C++ standard library only** — use `<vector>`, `<string>`, `<map>`, `<iostream>`, `<fstream>`, `<filesystem>`, `<optional>`, etc.
- **No third-party libraries** (no Boost, Fmt, nlohmann/json, curl, sqlite, etc.)
- **Rationale:** Keeps the codebase portable, easy to build anywhere, and free of dependency version conflicts.

### 2. C++17 Standard (Minimum)
- CMakeLists.txt enforces `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`
- Use modern C++ features: structured bindings, `std::optional`, `std::variant`, range-based for loops
- **Forbidden:** C++20 or later features (ranges, concepts, requires, etc.)
- Older compilers (GCC < 7, Clang < 5) will fail at CMake config; this is by design

### 3. Cross-Platform (Linux + Windows)
- **No platform-specific code** unless absolutely necessary
- Use `#ifdef _WIN32` or `#ifdef __linux__` only for unavoidable differences (e.g., binary_dir detection for MCP)
- **Use `<filesystem>`** for all path operations (not `boost::filesystem` or platform-specific APIs)
- **Avoid POSIX-only functions** (`fork`, `unlink`, `mmap`, etc.) without Windows equivalents
- **No hardcoded paths** — use `std::filesystem::absolute()` and environment variables

### 4. Full Test Coverage
- **All new features must include unit tests** in `tests/`
- **All bug fixes must include a regression test** that would have caught the bug
- Use existing doctest infrastructure (header-only in `tests/doctest.h`)
- Run before committing: `cmake --build build && ctest --test-dir build --output-on-failure`
- Tests must pass on both platforms (or document platform-specific behavior)

### 5. CMake Build System
- **CMake-only** — no Makefiles, shell scripts, or custom build steps outside CMake
- Build: `cmake -B build && cmake --build build`
- Configure with flags as needed: `-DCMAKE_BUILD_TYPE=Release` (already enforced for Windows)
- All targets, sources, and includes must be in CMakeLists.txt

### 6. CLI and Config Stability
- **Command names and arguments are stable** — breaking changes require a major version bump
- New flags can be added (`--new-flag`), but don't remove or rename existing ones without deprecation
- **Config file format is versioned** — `config_version` field tracks schema; migrations required if changed
- Notes file format (Markdown with YAML frontmatter) is stable
- **Always update README.md** if CLI, config format, or commands change

---

## Code Style

- **Naming:** `snake_case` for functions and variables; `CamelCase` for classes; `UPPER_SNAKE_CASE` for macros/constants
- **RAII:** Resources acquired in constructor, released in destructor (destructors are guaranteed to run)
- **const correctness:** Mark parameters and methods `const` where appropriate; avoid unnecessary `const_cast`
- **auto:** Use `auto` when type is obvious; explicit types for clarity when `auto` would hide intent
- **Single Responsibility:** Functions do one thing well; complex logic is broken into smaller functions
- **Comments:** Write few comments; code should be self-documenting via clear naming. Comment only *why*, not *what*

## Workflow

1. **Branch** from `master` (or `main`) for new features/fixes
2. **Commit format:** Follow conventional commits (`feat: ...`, `fix: ...`, `refactor: ...`, `docs: ...`, `test: ...`)
3. **Write tests first** (or at least alongside code) — test-driven development is preferred
4. **Build and test locally:**
   ```bash
   cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
   ```
5. **No external dependencies** — code review will reject any library additions
6. **Update docs:** README.md, CLAUDE.md, TECH_DEBT_AUDIT.md if behavior or architecture changes
7. **Create PR** with clear description of what changed and why
8. **Squash or rebase before merge** to keep history clean

## Forbidden Patterns (Will Be Rejected)

- ❌ External dependency includes: `#include <boost/...>`, `#include <fmt/...>`, `#include <nlohmann/json.hpp>`, `#include <curl/...>`, etc.
- ❌ Threading (`std::thread`) or platform-specific threading unless absolutely necessary
- ❌ Shell execution (`std::system()`) — fragile, cross-platform issues, security risk. For git ops, use direct file manipulation or a library (planned post-v0.1)
- ❌ Hardcoded absolute paths — use `std::filesystem` and environment variables
- ❌ Windows-only or Linux-only code without the other platform's equivalent
- ❌ C++20 or later features
- ❌ Breaking changes to CLI without major version bump
- ❌ Missing unit tests for new code
- ❌ Unformatted code — follow existing style (clang-format not enforced; style enforced by code review)

## Testing Expectations

- **Unit tests required:** All new functions/classes must have tests in `tests/`
- **Regression tests:** Bug fixes must include a test that would have caught the original bug
- **Test data:** Use `tests/data/configs/` and `tests/data/notes/` for test fixtures
- **Test helpers:** `TempRepo` helper sets up temporary git repos; see `tests/CLAUDE.md` for patterns
- **Coverage:** Aim for >80% line coverage; error paths (parse failures, permission errors) often have gaps
- **Platform testing:** Run tests on both Linux and Windows (or document platform-specific behavior)

## When Reviewing Code

✅ **Look for:**
- Standard library usage (`std::vector`, `std::string`, `std::map`, `std::optional`, `std::filesystem`)
- Test-driven approach (tests added with code)
- RAII patterns (cleanup in destructors, no manual resource management)
- const correctness (parameters marked const, methods marked const)
- Clear variable and function names (comments should be rare)
- Handling of errors (return codes checked, error messages clear)

❌ **Red flags:**
- Any `#include` of non-standard libraries
- Code without tests
- Platform-specific code without equivalent on other platform
- Hardcoded paths or environment assumptions
- Extensive comments explaining *what* (code should be self-documenting)
- Breaking changes without version bump or deprecation

## Documentation References

- **CLAUDE.md** — Technical guidance for Claude Code and AI assistants (loaded in every session)
- **src/CLAUDE.md** — Command architecture, patterns, registration (scoped to command layer)
- **tests/CLAUDE.md** — Test infrastructure, TempRepo usage, common failures (scoped to tests)
- **TECH_DEBT_AUDIT.md** — Known debt items, completed refactorings, status tracking
- **README.md** — User-facing documentation, CLI reference, installation, examples
- **docs/DESIGN.md** — Architecture specification, data model, design decisions

## File Structure

```
projot/
├── src/
│   ├── commands_project.cpp    ← project todo modifications
│   ├── commands_config.cpp     ← configuration modifications
│   ├── commands_maint.cpp      ← maintenance (render, install-hook, MCP)
│   ├── commands_shared.cpp     ← shared infrastructure
│   ├── commands_internal.h     ← internal declarations
│   ├── commands.h              ← public API (all cmd_* functions)
│   ├── config.cpp/h            ← config parsing and writing
│   ├── repo.cpp/h              ← git repo discovery
│   ├── todo.cpp/h              ← todo model and manipulation
│   ├── markdown.cpp/h          ← markdown parsing and rendering
│   ├── renderer.cpp/h          ← file rendering
│   ├── cli.cpp/h               ← CLI argument parsing
│   ├── utils.cpp/h             ← shared utilities
│   ├── main.cpp                ← entry point, command dispatch
│   └── CLAUDE.md               ← command architecture guidance
├── tests/
│   ├── test_commands.cpp       ← command tests
│   ├── test_config.cpp         ← config parsing tests
│   ├── test_markdown_parser.cpp
│   ├── test_renderer.cpp
│   ├── test_hook.cpp           ← hook installation tests
│   ├── test_versioning.cpp
│   ├── test_errors.cpp
│   ├── data/
│   │   ├── configs/            ← test config files
│   │   └── notes/              ← test markdown files
│   ├── doctest.h               ← test framework (header-only)
│   └── CLAUDE.md               ← test infrastructure guidance
├── docs/DESIGN.md              ← architecture specification
├── CLAUDE.md                   ← technical guidance (canonical, loaded in Claude sessions)
├── INSTRUCTIONS.md             ← this file (comprehensive reference)
├── TECH_DEBT_AUDIT.md          ← debt tracking and completion status
├── README.md                   ← user documentation
├── CMakeLists.txt              ← build configuration
└── LICENSE                     ← MIT
```

## Common Commands

```bash
# Build
cmake -B build && cmake --build build

# Test
ctest --test-dir build --output-on-failure

# Build and test together
cmake --build build && ctest --test-dir build --output-on-failure

# Build with explicit config (Windows)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# Clean rebuild
rm -rf build && cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

## Troubleshooting

| Issue | Cause | Fix |
|-------|-------|-----|
| Tests fail with "file not found" in test data | Tests run from wrong cwd or stale CMakeCache.txt | `rm build/CMakeCache.txt`, reconfigure, run from repo root |
| Build fails with "C++20 feature" error | Code uses C++20 syntax | Replace with C++17 equivalent; check `std::ranges`, `concepts`, `<=>` operator |
| Linker error: "undefined reference to ..." | Missing source file in CMakeLists.txt | Add `.cpp` file to `CORE_SOURCES` in CMakeLists.txt |
| Commit message linter error | Commit format doesn't match convention | Reword to `<type>: <subject>` (feat:, fix:, refactor:, docs:, test:, chore:) |
| Cross-platform path issue | Hardcoded path or platform-specific API | Use `std::filesystem::path` and `fs::path` throughout |

## When in Doubt

1. Check existing similar code for patterns
2. Read CLAUDE.md, src/CLAUDE.md, or tests/CLAUDE.md for subsystem-specific guidance
3. Review TECH_DEBT_AUDIT.md for known issues and planned refactorings
4. Consult README.md for user-facing behavior and CLI reference
5. Read docs/DESIGN.md for architecture decisions
6. Ask in PR comments if approach is unclear
