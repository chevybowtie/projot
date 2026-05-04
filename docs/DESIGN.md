# projot — Design Specification (v0.1)

## 1. Overview

projot is a cross-platform C++ command-line tool (Linux + Windows) that replaces a physical desk notepad used to track daily project work.

projot is **repo-centric**: it must be run from inside a git repository. All project configuration and notes are stored in a `.projot/` directory at the repository root, versioned alongside the code.

Each **project** is stored as a **single markdown file** with a strict, machine-parseable structure. The app allows a user to:

- Create and manage a project notes file identified by a **RPM project number**.
- Track todos, completion dates, and notes.
- Store relevant URLs (Teams channel, iTrack, RPM link, etc.).
- Generate consistent, human-readable files that can be shared with supervisors or project managers.

The initial version focuses on **one-shot CLI subcommands** (no TUI yet).  
The implementation uses **only the C++ standard library** and includes **unit tests** to ensure reliability.

---

## 2. Goals (v0.1)

- Replace the handwritten notepad with a fast, simple CLI workflow.
- Make it easy to answer questions such as:
  - “What’s the status of RPM <number>?”
  - “What are my current todos for this project?”
- Provide a **per-user configuration file** that is simple and human-editable.
- Store project data in markdown files with a strict, parseable structure.
- Build cross-platform support using standard C++ only (C++17 or later).
- Maintain high reliability through unit tests.

---

## 3. Non-Goals (v0.1)

- No TUI or interactive terminal UI.
- No database or external storage beyond markdown files.
- No multiple profiles.
- No network access.
- No plugin system.
- No git commit automation (projot writes files; committing is the developer's responsibility).
- No API integrations or HTTP features (future feature).
- No dependency libraries (standard library only).

---

## 4. Core Concepts

### 4.1 Project Identity

- **Repo ID:** App ID, stored in `.projot/config`. Set once per repo via `init`.
- **Project ID:** RPM project number (string), stored in `.projot/config`. Set per project via `new`.
- **One repository = one active project = one markdown file.**
- projot discovers the project root by walking up from `$CWD` until it finds a `.git` directory. It is an error to run projot outside a git repository.
- The notes file lives at:

```
{repo_root}/.projot/{RPM}.md
```

### 4.3 Todo Identity

- Each todo is assigned a **stable numeric ID** at creation time (sequential, starting at 1).
- IDs are **never reassigned or renumbered** — completed todos retain their original number.
- Gaps in the sequence are possible in future versions if deletion is added.
- `--todo <number>` always refers to this stable ID, not a display position.

### 4.2 Project Content

Each project file contains:

- RPM number
- iTrack number (optional)
- App ID (optional) — stored in `.projot/config`; rendered into the document by projot
- Project name
- Date created
- A configurable list of URLs (Teams, iTrack, RPM, Other — single values)
- Zero or more GitHub repository URLs — stored in `.projot/config`; projot-managed section in the document
- Zero or more Swagger/OpenAPI URLs — stored in `.projot/config`; projot-managed section in the document
- Zero or more Blizzard URLs — stored in `.projot/config`; projot-managed section in the document
- A structured list of todos
- Notes under each todo

---

## 5. File & Directory Layout

All projot files are stored under a `.projot/` directory at the repository root:

```
{repo_root}/.projot/
    config          ← project configuration (key = value)
    {RPM}.md       ← project notes file
```

Example:
```
/home/paul/my-project/.projot/config
/home/paul/my-project/.projot/12345.md
```

### 5.1 Repo Root Discovery

projot walks up from `$CWD` until it finds a directory containing `.git`. That directory is the repo root. If no `.git` is found, projot exits with an error.

### 5.2 Config Location

```
{repo_root}/.projot/config
```

There is no global/user-level config. All configuration is per-repository.

Users may optionally specify `--config <path>` in later versions.

---

## 6. Config File

### 6.1 Format

`.projot/config` uses a simple line-based `key = value` format.

- Lines beginning with `#` are comments.
- Unknown keys are ignored (future-proofing).
- Lists are comma-separated values on a single line.
- `config_version` is written by projot and must not be hand-edited.

### 6.2 Fields

**Schema version** (written automatically by projot):

| Field | Description |
|---|---|
| `config_version` | Integer. Written by projot on `init`. Incremented only when the config schema changes in a breaking way. Current value: `1`. |

**Repo-level** (set by `init`, rarely change):

| Field | Required | Description |
|---|---|---|
| `app_id` | Required | Application ID associated with this repository |
| `github` | Optional | Comma-separated list of GitHub URLs |
| `swagger` | Optional | Comma-separated list of Swagger/OpenAPI URLs |
| `blizzard` | Optional | Comma-separated list of Blizzard URLs |

**Project-level** (set by `new`, specific to the RPM project):

| Field | Required | Description |
|---|---|---|
| `rpm` | Required | The RPM project number |
| `name` | Required | Human-readable project name |
| `itrack` | Required | iTrack ticket number |
| `date_format` | Optional | Display-only date format (stored ISO always) |
| `links` | Optional | Ordered list of single-value link keys to include |
| `label.<key>` | Optional | Human-friendly label for a link key |
| `link.<key>` | Optional | URL value for a single-value link key (e.g. `link.teams = https://...`) |

> Repo-level fields (`app_id`, `github`, `swagger`, `blizzard`) are set once and shared across projects. They are rendered into the project markdown file by projot and should not be hand-edited there.

### 6.3 Config Example (`.projot/config`)

```
# projot config
config_version = 1

# --- Repo-level fields (set by `init`) ---

# Application ID
app_id = MyApp

# GitHub repositories (projot-managed)
github = https://github.com/org/repo-one, https://github.com/org/repo-two

# Swagger/OpenAPI endpoints (projot-managed)
swagger = https://api.example.com/swagger

# Blizzard links (projot-managed)
blizzard = https://blizzard.example.com/project

# --- Project-level fields (set by `new`) ---

# RPM project number
rpm = 12345

# Project name
name = My Project

# iTrack ticket number
itrack = 67890

# Date format used for display only; stored ISO always
date_format = YYYY-MM-DD

# Which single-value URLs to include in the Links section
links = teams, itrack, rpm, other

# Human-friendly labels
label.teams = Teams
label.itrack = iTrack
label.rpm = RPM
label.other = Other

# Single-value link URLs
link.teams = https://teams.microsoft.com/l/channel/...
link.itrack = https://itrack.example.com/ticket/67890
link.rpm = https://rpm.example.com/project/12345
link.other = https://wiki.example.com/project
```

---

## 7. Markdown File Structure

Project markdown files use a strict structure with a **required section order**:

```markdown
# Project: {Project Name}

- RPM: {RPM}
- iTrack: {iTrack or "N/A"}
- App ID: {App ID or "N/A"}  <!-- projot-managed: set via set-app-id -->
- Created: {YYYY-MM-DD}

## Links
- Teams: {url or "N/A"}
- iTrack: {url or "N/A"}
- RPM: {url or "N/A"}
- Other: {url or "N/A"}

<!-- projot-managed: do not hand-edit sections below; use add-github/add-swagger/add-blizzard -->

## GitHub
- https://github.com/org/repo-one
- https://github.com/org/repo-two

## Swagger
- https://api.example.com/swagger

## Blizzard
- https://blizzard.example.com/project

## Todos

1. [ ] Todo text
   - Created: {YYYY-MM-DD}
   - Notes:
     - First note
     - Additional detail

2. [x] Completed example
   - Created: 2025-01-01
   - Completed: 2025-01-05
   - Notes:
     - Completed successfully
```

The section order **Links → GitHub → Swagger → Blizzard → Todos** is required and enforced by projot. Sections with no entries (GitHub, Swagger, Blizzard) are omitted from the rendered file.

---

## 8. Parsing Strategy

- On startup, walk up from `$CWD` to find a `.git` directory. Exit with an error if none found.
- Load `.projot/config` from the repo root. Exit with a clear error if it is missing or malformed.
- Read `rpm` from config to locate the notes file at `.projot/{RPM}.md`.
- Use line-based parsing to detect sections.
- Section order is canonical and required: **Links → GitHub → Swagger → Blizzard → Todos**. Parsing assumes this order.
- Identify `## Todos` and parse numbered items.
- Represent todos internally as a structured model.
- Modify in-memory data when adding, completing, or appending notes.
- Re-render the entire file on each write to guarantee consistency.
- `## GitHub`, `## Swagger`, and `## Blizzard` sections are **projot-managed**: their content is sourced from `.projot/config` and fully regenerated on each write. Manual edits to these sections will be overwritten.
- `App ID` in the file header is also sourced from `.projot/config` and regenerated on write.
- Sections with empty URL lists (GitHub, Swagger, Blizzard) are omitted from the rendered file.
- URLs are deduplicated before writing; duplicate entries in config are silently ignored.

---

## 9. Command-Line Interface

CLI uses **subcommands**:

```
projot <subcommand> [options]
```

All subcommands must be run from within a git repository. `init` and `new` are the two setup commands; all others require both to have been run.

### 9.0 Help

#### `projot --help` / `projot -h`
Print the top-level usage summary and exit 0:

```
Usage: projot <subcommand> [options]

Setup commands:
  init          Initialize projot for this repository
  new           Start a new RPM project in this repository

Project commands:
  add-todo      Append a new todo
  list          Show project summary and todos
  complete      Mark a todo completed
  add-note      Add a note to a todo
  set-link      Set or update a single-value link URL
  set-app-id    Set the application ID
  add-github    Add a GitHub URL
  add-swagger   Add a Swagger URL
  add-blizzard  Add a Blizzard URL
  render        Re-render the notes file and stage it

Maintenance commands:
  install-hook  Install the pre-commit git hook

Run 'projot <subcommand> --help' for subcommand options.
```

#### `projot <subcommand> --help` / `projot <subcommand> -h`
Print usage for that specific subcommand and exit 0. Each subcommand's help block must show:
- A one-line description
- All flags with their argument type (e.g. `<ID>`, `<URL>`, `"<text>"`) and whether required or optional
- A short usage example

Example — `projot add-todo --help`:
```
Usage: projot add-todo --text "<description>"

Append a new todo to the project notes file. The todo is assigned the next
available stable ID and written to .projot/{RPM}.md.

Required:
  --text "<description>"   Text of the new todo

Example:
  projot add-todo --text "Validate index rebuild plan"
```

- `--help` / `-h` are valid on every subcommand.
- Unrecognised flags print a short error then suggest `projot <subcommand> --help` before exiting non-zero.

### 9.1 Setup Commands

#### `init`
Initialize projot for the current repository. Creates the `.projot/` directory and writes repo-level fields to `.projot/config`. Fails with an error if `.projot/` already exists (remove `.projot/` to start over).

Required:
- `--app-id <App ID>`

Optional:
- `--github <URL>` (repeatable)
- `--swagger <URL>` (repeatable)
- `--blizzard <URL>` (repeatable)

#### `new`
Start a new project in this repository. Writes project-level fields to `.projot/config` and creates the notes file `.projot/{RPM}.md`. Fails if a project is already configured (i.e. `rpm` is already set in config).

After creating the notes file, `new` installs a `pre-commit` git hook (see section 9.3) that calls `projot render` before every commit. This ensures the committed notes file always reflects the current config and todo state.

Required:
- `--rpm <RPM>`
- `--name "<Project Name>"`
- `--itrack <iTrack>`

Optional:
- `--teams <URL>`
- `--rpm-url <URL>` — the RPM system link for this project
- `--itrack-url <URL>`
- `--other <URL>`
- `--no-hook` — skip git hook installation

> Repo-level fields (`github`, `swagger`, `blizzard`) already in config from `init` are automatically included in the new project file.

### 9.2 Project Commands

#### `add-todo`
Append a new todo to the project notes file. Assigned the next available stable ID.

Required:
- `--text "<description>"`

#### `list`
Display a project summary and todos.

Default output:
```
Project: {Project Name}  |  RPM: {RPM}  |  iTrack: {iTrack}

1. First open todo
2. Second open todo
```

Optional:
- `--open` — open todos only (default)
- `--closed` — closed todos only
- `--all` — all todos regardless of status

#### `complete`
Mark a todo completed. Warns if already completed but does not error.

Required:
- `--todo <ID>`

#### `add-note`
Add a note under a specific todo. Warns if the todo is already completed but proceeds.

Required:
- `--todo <ID>`
- `--text "<note>"`

#### `set-link`
Set or update a single-value link URL in `.projot/config`.

Required:
- `--key <key>` — e.g. `teams`, `itrack`, `rpm`, `other`
- `--url <URL>`

#### `set-app-id`
Set the app ID in `.projot/config`.

Required:
- `--app-id <App ID>`

Optional:
- `--force` — required if `app_id` is already set; prevents accidental overwrites.

#### `add-github`
Add a GitHub URL to `.projot/config`. Deduplicates silently. projot re-renders the `## GitHub` section in the notes file.

Required:
- `--url <URL>`

#### `add-swagger`
Add a Swagger URL to `.projot/config`. Deduplicates silently. projot re-renders the `## Swagger` section in the notes file.

Required:
- `--url <URL>`

#### `add-blizzard`
Add a Blizzard URL to `.projot/config`. Deduplicates silently. projot re-renders the `## Blizzard` section in the notes file.

Required:
- `--url <URL>`

#### `render`
Re-render `.projot/{RPM}.md` from `.projot/config` and the current todo state, then `git add .projot/{RPM}.md` so the regenerated file is staged for the current commit.

This subcommand is called automatically by the pre-commit hook installed by `new`. It can also be run manually at any time to sync the notes file after hand-editing `.projot/config`.

No flags required. Exits 0 on success.

### 9.3 Git Hook

`new` installs a `pre-commit` hook at `{repo_root}/.git/hooks/pre-commit` to keep the notes file current on every commit.

#### Hook behaviour

The hook calls `projot render`, which re-renders `.projot/{RPM}.md` from config and then runs `git add .projot/{RPM}.md`. This means:
- Config changes (e.g. a new GitHub URL added with `add-github`) are reflected in the committed notes file automatically.
- The committed markdown is always consistent with `.projot/config`.

#### Hook content written by `new`

```sh
#!/bin/sh
# projot: regenerate notes file before commit
if command -v projot >/dev/null 2>&1; then
    projot render
fi
```

If `.git/hooks/pre-commit` **does not exist**, projot writes the file and sets it executable.

If `.git/hooks/pre-commit` **already exists**, projot appends the guarded block above and prints a notice:
```
Note: appended projot render block to existing .git/hooks/pre-commit
```
The guard (`command -v projot`) ensures the hook is a no-op if projot is not on `PATH` (e.g. on a colleague's machine who hasn't installed it).

#### `--no-hook`

Pass `--no-hook` to `new` to skip hook installation entirely. The hook can be installed later with:
```
projot install-hook
```

#### `install-hook` subcommand

Installs or re-installs the pre-commit hook in the current repository. Follows the same append-or-create logic as `new`. Useful if the hook was skipped with `--no-hook` or was accidentally deleted.

No flags required.

### 9.4 Shell Tab-Completion

Tab completion is delivered as **generated shell scripts** — projot itself does not need to be running to complete. Scripts are installed separately; the binary does not need to shell-out or carry embedded completion logic.

#### Supported shells (v0.1)

| Shell | Script location | How it works |
|---|---|---|
| Bash | `completions/projot.bash` | `complete -F` with a function sourced from `~/.bashrc` or `/etc/bash_completion.d/` |
| Zsh | `completions/_projot` | Zsh `compdef` / `_arguments` style, placed in a `$fpath` directory |
| Fish | `completions/projot.fish` | `complete` commands, placed in `~/.config/fish/completions/` |
| PowerShell | `completions/projot.ps1` | `Register-ArgumentCompleter` block, dot-sourced from `$PROFILE` |

#### What is completed

- **Subcommand names** after `projot ` (e.g. `init`, `new`, `add-todo`, …)
- **Flag names** for the current subcommand (e.g. after `projot add-todo `, complete `--text`)
- **`--key` values** for `set-link`: complete `teams`, `itrack`, `rpm`, `other`
- **`--todo` values** for `complete` and `add-note`: read open todo IDs from `.projot/{RPM}.md` at completion time (best-effort; silently skip if no file found)
- **`-h` / `--help`** on every subcommand

#### Implementation note

The completion scripts are static and hand-authored in v0.1. The `--todo` dynamic completion reads the notes file directly with a small shell snippet; no projot binary invocation is required at completion time. This avoids startup latency and makes completion resilient to build state.

#### Installation

Run `make install-completion` after installing the binary (see section 14.3). The Makefile target detects the current shell from `$SHELL` and copies the appropriate script to the standard location automatically. PowerShell must be installed manually.

---

## 10. Error Handling

- **Not in a git repo:** exit with a clear error if no `.git` directory is found walking up from `$CWD`.
- **`init` on already-initialized repo:** exit with an error; user must remove `.projot/` to start over.
- **`new` when project already configured:** exit with an error indicating `rpm` is already set.
- **Missing `.projot/config`:** exit with a clear error suggesting `projot init`.
- **Missing notes file:** exit with a clear error if `.projot/{RPM}.md` does not exist; suggest `projot new`.
- **`complete` on already-completed todo:** print a warning and exit 0 (no-op).
- **`add-note` on completed todo:** print a warning then proceed normally.
- **`set-app-id` without `--force` when `app_id` already set:** exit with an error showing the current value.
- **Invalid todo ID:** exit with a clear error listing valid IDs.
- **Hook installation failure** (`new` or `install-hook`): if `.git/hooks/` is not writable, print a warning and continue — the project is created successfully; the hook is optional.
- **Config version mismatch:** if `config_version` is higher than the version the binary understands, exit with an error explaining the binary is too old and showing the current `config_version` value.
- **Missing `config_version`:** treat as version 0 (pre-versioning); warn and continue in v0.1.
- Non-zero exit codes on all hard errors.

---

## 11. Testing

### 11.1 Framework

The "no external dependencies" constraint applies to the **shipped binary** only. The test build may use a single-header test framework bundled in the repository under `tests/`.

**Chosen framework: [doctest](https://github.com/doctest/doctest)**
- Single header (`tests/doctest.h`) — no build system changes required.
- Fast compile times; no macros that conflict with standard library headers.
- Compatible with GCC 8+, Clang 7+, MSVC VS2017 15.7+.
- Test binary links the same object files as the main binary; no separate library target needed.

The test binary is built by `make test` / `ctest` and is distinct from the `projot` binary.

### 11.2 Test File Layout

```
tests/
    doctest.h                    ← vendored single-header framework
    data/                        ← fixture files
        configs/
            valid_full.cfg       ← repo+project fields, config_version=1
            repo_only.cfg        ← only repo-level fields set
            version_zero.cfg     ← no config_version field
            version_future.cfg   ← config_version=999
            malformed.cfg        ← bad key=value lines
        notes/
            basic.md             ← one open todo, one closed todo
            no_todos.md          ← header + links only
            multi_todo.md        ← 10 todos, various states
            managed_sections.md  ← GitHub/Swagger/Blizzard present
    test_config.cpp
    test_repo_discovery.cpp
    test_markdown_parser.cpp
    test_renderer.cpp
    test_todo_model.cpp
    test_commands.cpp
    test_hook.cpp
    test_versioning.cpp
    test_errors.cpp
```

### 11.3 Config Parsing (`test_config.cpp`)

| Test | Description |
|---|---|
| `parse_valid_full` | Parse `valid_full.cfg`; verify all repo-level and project-level fields read correctly |
| `parse_repo_only` | Parse `repo_only.cfg`; project-level fields absent → default/empty |
| `parse_config_version_present` | `config_version = 1` → parsed as integer 1 |
| `parse_config_version_missing` | No `config_version` field → returns 0 |
| `parse_config_version_future` | `config_version = 999` → triggers version-mismatch error path |
| `parse_comments_ignored` | Lines starting with `#` are not returned as keys |
| `parse_blank_lines_ignored` | Blank lines do not produce entries |
| `parse_whitespace_trimmed` | `key  =  value  ` → key `"key"`, value `"value"` |
| `parse_list_field` | `github = url1, url2, url3` → vector of 3 strings |
| `parse_list_single` | `github = url1` → vector of 1 string |
| `parse_list_empty` | `github = ` → empty vector |
| `parse_link_dotted_key` | `link.teams = https://...` → stored under key `link.teams` |
| `parse_label_dotted_key` | `label.teams = Teams` → stored under key `label.teams` |
| `parse_unknown_keys_ignored` | Unknown key in config → no error, key not in result |
| `parse_crlf_line_endings` | File with `\r\n` endings → values have no trailing `\r` |
| `parse_missing_file` | Non-existent path → returns error / throws |
| `parse_malformed_no_equals` | Line with no `=` → ignored (not an error) |
| `write_round_trip` | Write config, re-parse, verify all fields preserved |
| `write_preserves_config_version` | Written config always contains `config_version = 1` |
| `write_comments_header` | Written config contains section comment headers |

### 11.4 Repo Root Discovery (`test_repo_discovery.cpp`)

| Test | Description |
|---|---|
| `find_root_at_cwd` | `.git` in current directory → returns current directory |
| `find_root_two_levels_up` | `.git` two directories above CWD → returns correct root |
| `find_root_at_filesystem_root` | No `.git` found walking all the way up → returns error |
| `find_root_ignores_file_named_git` | A file named `.git` (not a directory) → not treated as repo root |
| `find_root_bare_repo` | `.git` is a file (worktree) → treated as repo root (file presence is sufficient) |

### 11.5 Markdown Parsing (`test_markdown_parser.cpp`)

| Test | Description |
|---|---|
| `parse_header_fields` | Project name, RPM, iTrack, App ID, Created date all parsed correctly from header |
| `parse_header_na_values` | `N/A` values for optional fields → stored as empty string |
| `parse_links_section` | All configured link keys and URLs extracted |
| `parse_github_section` | Multiple GitHub URLs extracted as list |
| `parse_swagger_section` | Multiple Swagger URLs extracted as list |
| `parse_blizzard_section` | Multiple Blizzard URLs extracted as list |
| `parse_missing_managed_sections` | File with no GitHub/Swagger/Blizzard sections → empty lists, no error |
| `parse_todo_open` | `1. [ ] text` → todo ID=1, open, text correct |
| `parse_todo_closed` | `2. [x] text` → todo ID=2, closed |
| `parse_todo_created_date` | `- Created: 2025-01-01` under todo → date parsed |
| `parse_todo_completed_date` | `- Completed: 2025-01-05` under closed todo → date parsed |
| `parse_todo_notes` | `- Notes:` block with bullet entries → notes list populated |
| `parse_todo_no_notes` | Todo with empty `- Notes:` → empty notes list, no error |
| `parse_multiple_todos` | File with 10 todos → all 10 parsed, IDs correct |
| `parse_stable_ids_with_gap` | Todos numbered 1, 2, 4 (gap at 3) → IDs preserved as-is |
| `parse_crlf_in_notes_file` | Notes file with `\r\n` endings → no stray `\r` in parsed values |
| `parse_empty_todos_section` | `## Todos` present but no entries → empty list, no error |
| `parse_no_todos_section` | File has no `## Todos` → empty list, no error |

### 11.6 Renderer (`test_renderer.cpp`)

| Test | Description |
|---|---|
| `render_header` | Correct `# Project:` line and metadata bullets |
| `render_links_from_config` | Links section uses `links` key order and `label.*` values |
| `render_links_na_when_missing` | Link key in `links` list but no `link.<key>` in config → `N/A` |
| `render_github_section` | One or more GitHub URLs → correct `## GitHub` block |
| `render_swagger_section` | Swagger URLs → correct `## Swagger` block |
| `render_blizzard_section` | Blizzard URLs → correct `## Blizzard` block |
| `render_omits_empty_github` | No GitHub URLs in config → `## GitHub` section absent |
| `render_omits_empty_swagger` | No Swagger URLs → `## Swagger` absent |
| `render_omits_empty_blizzard` | No Blizzard URLs → `## Blizzard` absent |
| `render_managed_comment` | Managed-sections comment present before first managed section |
| `render_section_order` | Output order is Links → GitHub → Swagger → Blizzard → Todos |
| `render_todo_open` | Open todo renders as `N. [ ] text` |
| `render_todo_closed` | Closed todo renders as `N. [x] text` with `Completed:` line |
| `render_todo_notes` | Notes bullets rendered correctly under todo |
| `render_todo_no_notes_block` | Todo with empty notes → `- Notes:` line still present |
| `render_deduplicates_github_urls` | Duplicate GitHub URL in config → appears only once in output |
| `render_deduplicates_swagger_urls` | Same for Swagger |
| `render_lf_line_endings` | Output file uses `\n` only, never `\r\n` |
| `render_round_trip` | Parse a file, render it, re-parse → model identical |
| `render_app_id_from_config` | `App ID` in header sourced from config, not from notes file |
| `render_app_id_na` | No `app_id` in config → `App ID: N/A` |

### 11.7 Todo Model (`test_todo_model.cpp`)

| Test | Description |
|---|---|
| `add_first_todo` | Empty list → new todo gets ID 1 |
| `add_second_todo` | List with ID 1 → new todo gets ID 2 |
| `add_after_gap` | List with IDs 1, 3 (gap) → new todo gets ID 4 (next after max) |
| `complete_todo` | Mark ID 2 completed → `completed = true`, `completed_date` set |
| `complete_already_done` | Mark already-completed todo → returns warning flag, no state change |
| `add_note_to_open` | Append note to open todo → note appended to notes list |
| `add_note_to_closed` | Append note to closed todo → returns warning flag, note still appended |
| `id_stability_after_complete` | Complete todo 2; list still has ID 1, 2, 3 with correct states |
| `find_by_id_valid` | `find(2)` on list containing ID 2 → returns correct todo |
| `find_by_id_missing` | `find(99)` → returns not-found |
| `list_open` | Filter open → only uncompleted todos |
| `list_closed` | Filter closed → only completed todos |
| `list_all` | No filter → all todos in ID order |

### 11.8 Command Behaviour (`test_commands.cpp`)

These tests use temporary directories created with `std::filesystem::temp_directory_path()` and cleaned up in test teardown.

| Test | Description |
|---|---|
| `init_creates_projot_dir` | `init` creates `.projot/` directory |
| `init_writes_config` | `init` writes `.projot/config` with `config_version`, `app_id` |
| `init_with_github_url` | `--github <url>` → written to config |
| `init_with_multiple_github` | `--github` repeated twice → both URLs in config |
| `init_fails_if_already_init` | Second `init` on same repo → non-zero exit |
| `init_requires_app_id` | `init` without `--app-id` → non-zero exit |
| `new_writes_project_fields` | `new` writes `rpm`, `name`, `itrack` to config |
| `new_creates_notes_file` | `new` creates `.projot/{RPM}.md` |
| `new_notes_file_has_correct_header` | Notes file has `# Project:` with correct name |
| `new_with_teams_url` | `--teams <url>` → `link.teams` in config, Teams in Links section |
| `new_fails_if_rpm_set` | `rpm` already in config → non-zero exit |
| `new_fails_without_required_flags` | Missing `--rpm`, `--name`, or `--itrack` → non-zero exit |
| `new_inherits_repo_fields` | GitHub URL set by `init` appears in rendered notes file |
| `add_todo_appends` | `add-todo` adds entry with next ID, `Created` date |
| `add_todo_stable_id` | Two `add-todo` calls → IDs 1 and 2 |
| `list_default_shows_open` | `list` with no flag → only open todos shown |
| `list_closed_flag` | `list --closed` → only closed todos |
| `list_all_flag` | `list --all` → all todos |
| `list_shows_header` | Output includes project name, RPM, iTrack line |
| `complete_marks_done` | `complete --todo 1` → re-parsed file shows `[x]`, `Completed:` date |
| `complete_warns_if_already_done` | Re-completing → warning on stderr, exit 0, file unchanged |
| `add_note_appends` | `add-note --todo 1 --text "note"` → note appears in re-parsed file |
| `add_note_warns_if_closed` | `add-note` on closed todo → warning on stderr, note still written |
| `set_link_new_key` | `set-link --key teams --url <url>` → `link.teams` written to config |
| `set_link_update_key` | `set-link` on existing key → value replaced, not duplicated |
| `set_app_id_sets_value` | `set-app-id --app-id X` → `app_id = X` in config |
| `set_app_id_no_force_fails` | `set-app-id` when `app_id` already set, no `--force` → non-zero exit |
| `set_app_id_force_overwrites` | `set-app-id --force --app-id X` → value replaced |
| `add_github_deduplicates` | Adding same URL twice → appears once in config |
| `add_swagger_deduplicates` | Same for Swagger |
| `add_blizzard_deduplicates` | Same for Blizzard |
| `render_updates_file` | `render` re-writes notes file; re-parse yields consistent state |
| `render_stages_file` | `render` calls `git add .projot/{RPM}.md` (verify via `git status`) |
| `version_flag` | `projot --version` → prints `projot X.Y.Z`, exits 0 |

### 11.9 Git Hook (`test_hook.cpp`)

| Test | Description |
|---|---|
| `new_installs_hook` | After `new`, `.git/hooks/pre-commit` exists and is executable |
| `new_hook_content` | Hook file contains the `projot render` guard block |
| `new_no_hook_flag` | `new --no-hook` → hook file not created |
| `install_hook_creates_file` | `install-hook` on repo with no hook → creates file, sets executable |
| `install_hook_appends_if_exists` | Pre-existing hook with other content → block appended, original content preserved |
| `install_hook_append_notice` | Append case → notice printed to stdout |
| `install_hook_idempotent` | Running `install-hook` twice → block not duplicated |
| `install_hook_not_writable` | `.git/hooks/` not writable → warning printed, exit 0, project created |

### 11.10 Versioning (`test_versioning.cpp`)

| Test | Description |
|---|---|
| `config_version_written_on_init` | `init` writes `config_version = 1` to config |
| `config_version_correct_value` | Parsed value equals `PROJOT_CONFIG_VERSION` compile-time constant |
| `config_version_future_hard_error` | `config_version` > known max → non-zero exit, error message contains version number |
| `config_version_zero_warns` | Missing `config_version` → warning on stderr, continues successfully |
| `app_version_format` | `--version` output matches `projot \d+\.\d+\.\d+` |

### 11.11 Error Cases (`test_errors.cpp`)

| Test | Description |
|---|---|
| `not_in_git_repo` | Run any command in a directory with no `.git` → non-zero exit, clear error message |
| `missing_config` | `.git` exists but no `.projot/config` → message suggests `projot init` |
| `missing_notes_file` | Config has `rpm` but `.projot/{RPM}.md` absent → message suggests `projot new` |
| `invalid_todo_id` | `--todo 99` when only IDs 1–3 exist → non-zero exit, lists valid IDs |
| `unknown_flag` | `projot add-todo --bogus` → non-zero exit, message includes `projot add-todo --help` |
| `help_exits_zero` | `projot --help` → exit 0 |
| `subcommand_help_exits_zero` | `projot add-todo --help` → exit 0 |
| `version_exits_zero` | `projot --version` → exit 0 |
| `no_args_exits_nonzero` | `projot` with no arguments → non-zero exit, prints usage hint |

### 11.12 Fixture Files

Sample markdown files and config fixtures live under `tests/data/`. Each fixture is a static file committed to the repository. Tests that need a writable copy must duplicate the fixture to a temp directory before use.

```
tests/data/
    configs/
        valid_full.cfg
        repo_only.cfg
        version_zero.cfg
        version_future.cfg
        malformed.cfg
    notes/
        basic.md
        no_todos.md
        multi_todo.md
        managed_sections.md
```

---

## 12. Roadmap (Post v0.1)

### 12.1 TUI Mode
- ncurses-based UI
- Browse projects and todos interactively

### 12.2 Search & Filtering
- Search todos by text
- Filter by age, date range, or status

### 12.3 Reporting
- Weekly / monthly summaries
- Optional HTML/CSV output

### 12.4 Version Control Integration

Since projot is repo-centric, the notes file and config are already inside the git repository and versioned automatically when committed. No separate sync step is needed.

**Git hook (installed by `new`):**
- `new` installs a `pre-commit` hook that calls `projot render` before every commit.
- The hook is a no-op if projot is not on `PATH`, so it is safe to commit without projot installed.
- `install-hook` can install or reinstall the hook at any time.

**`.gitignore` guidance:**
- Commit `.projot/config` and `.projot/{RPM}.md` — this is the point.
- Optionally add `.projot/*.bak` or similar if projot ever creates backup files.

### 12.5 Richer Configuration
- TOML or YAML support
- Per-project overrides

### 12.6 Multi-Format Output
- Export to HTML or PDF (via external tools)

### 12.7 Shell Tab-Completion Install Command
- Post-v0.1: `projot install-completion [--shell bash|zsh|fish|powershell]` subcommand to install completion scripts from inside the binary, without needing the source tree.
- v0.1 installs completions via `make install-completion`.

## 13. Cross-Platform Notes

projot targets Linux and Windows. The following guidelines apply to the implementation:

### 13.1 Path Handling
- Always use `std::filesystem::path` for path construction and traversal. Never concatenate paths with string operations.
- `std::filesystem::current_path()` provides the current working directory on both platforms.
- `std::filesystem::exists(root / ".git")` works identically on both platforms for repo root discovery.

### 13.2 Line Endings
- Linux editors write `\n`; Windows editors often write `\r\n`.
- The parser **must** strip trailing `\r` from every line after splitting on `\n`. Failure to do so causes stray `\r` characters in parsed field values and corrupts todo detection.
- projot **writes** files with `\n` line endings only, regardless of platform. This keeps files consistent when teammates on different OSes edit the same repo.

### 13.3 Hidden Directories
- `.projot/` is a conventional hidden directory on Linux. On Windows it is a normal folder — no special handling required.

### 13.4 Terminal Output
- Avoid ANSI color/escape codes in v0.1. They work on Linux terminals and modern Windows Terminal but not legacy `cmd.exe`.
- Plain text output ensures compatibility everywhere.

### 13.6 Git Hook — Cross-Platform
- On Linux/macOS, `new` must `chmod +x` the hook file after writing it. Use `std::filesystem::permissions()` with `std::filesystem::perms::owner_exec | group_exec | others_exec`.
- On Windows, `.git/hooks/pre-commit` is a shell script and is not directly executable by `cmd.exe` or PowerShell. Git for Windows ships with Git Bash, which will run the hook. No `chmod` equivalent is needed on Windows; projot skips the `chmod` call on that platform (`#ifdef _WIN32`).
- The guard `command -v projot >/dev/null 2>&1` is POSIX sh. The Windows Git Bash environment supports this syntax.

### 13.7 Tab-Completion Scripts
- Bash and Zsh scripts use POSIX-compatible shell syntax and should work on macOS as well.
- The Fish script requires Fish 3+.
- The PowerShell script requires PowerShell 5.1+ (Windows) or PowerShell 7+ (cross-platform).
- Dynamic `--todo` completion reads `.projot/{RPM}.md` using the shell's own file-reading primitives, not by calling projot. This avoids Windows path issues and works even when the binary is not on `PATH` in the completion context.

### 13.5 Compiler Requirements
- `std::filesystem` requires C++17. Minimum supported compilers:
  - GCC 8+
  - Clang 7+
  - MSVC (Visual Studio 2017 15.7+)

---

## 14. Build, Installation & Distribution

### 14.1 Build System

projot uses **CMake** as its canonical build system with a thin **`Makefile` wrapper** at the repo root for Linux convenience. CMake 3.16+ is required.

#### Makefile targets

| Target | Action |
|---|---|
| `make` | Configure (Release) and build |
| `make debug` | Configure (Debug) and build |
| `make test` | Build and run unit tests |
| `make install` | Install binary to `$(PREFIX)/bin` (default: `/usr/local`) |
| `make install-completion` | Install shell completion for the current shell (detected via `$SHELL`) |
| `make clean` | Remove the `build/` directory |

The Makefile delegates to CMake internally:

```makefile
PREFIX   ?= /usr/local
BUILD_DIR ?= build

all:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

debug:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)

test: all
	ctest --test-dir $(BUILD_DIR) --output-on-failure

install: all
	cmake --install $(BUILD_DIR) --prefix $(DESTDIR)$(PREFIX)

install-completion:
	@scripts/install-completion.sh

clean:
	rm -rf $(BUILD_DIR)
```

Direct CMake usage is also fully supported (required on Windows):

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /usr/local
```

### 14.2 Installation

#### Linux — from source

```sh
git clone https://github.com/.../projot
cd projot
make
sudo make install          # installs to /usr/local/bin
make install-completion    # installs completion for current shell
```

`PREFIX` can be overridden for a user-local install (no `sudo`):

```sh
make install PREFIX=~/.local
```

#### Windows — from source

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --prefix "C:\Users\<user>\AppData\Local\Programs\projot"
```

Then add the install directory to `PATH`. Install the PowerShell completion script manually by appending `completions/projot.ps1` to `$PROFILE`.

#### Pre-built binaries

Download the appropriate binary from the **GitHub Releases** page. No build tools required.

| Asset | Platform |
|---|---|
| `projot-linux-x86_64` | Linux (glibc) |
| `projot-windows-x86_64.exe` | Windows 10+ |

Place the binary on `PATH`. Download the matching completion script from the release assets and install manually, or clone the repo and run `make install-completion`.

### 14.3 `make install-completion`

`scripts/install-completion.sh` reads `$SHELL` and copies the appropriate script:

| Shell | Installed path |
|---|---|
| Bash | `~/.local/share/bash-completion/completions/projot` |
| Zsh | `~/.zsh/completions/_projot` (user must ensure this is in `$fpath`) |
| Fish | `~/.config/fish/completions/projot.fish` |

If `$SHELL` is not recognised, the script lists available scripts in `completions/` and exits without installing. PowerShell completion must always be installed manually (append `completions/projot.ps1` to `$PROFILE`).

The script creates the target directory if it does not exist.

### 14.4 GitHub Actions CI/CD

Two workflows live in `.github/workflows/`.

#### `ci.yml` — build and test on every push/PR

Matrix:

| Runner | Compiler |
|---|---|
| `ubuntu-latest` | GCC (default) |
| `ubuntu-latest` | Clang |
| `windows-latest` | MSVC |

Steps per job:
1. Checkout
2. `cmake -B build -DCMAKE_BUILD_TYPE=Release`
3. `cmake --build build`
4. `ctest --test-dir build --output-on-failure`

#### `release.yml` — publish GitHub Release on version tag

Triggered by a push of a tag matching `v*` (e.g. `v0.1.0`).

Steps:
1. Run the same build matrix as `ci.yml`.
2. Strip the Linux binary (`strip projot`).
3. Upload release assets:
   - `projot-linux-x86_64`
   - `projot-windows-x86_64.exe`
   - `completions/projot.bash`
   - `completions/_projot`
   - `completions/projot.fish`
   - `completions/projot.ps1`
4. Create a GitHub Release with all assets attached. Release notes are generated from the tag annotation.

### 14.5 Application Versioning

projot uses **semantic versioning**: `MAJOR.MINOR.PATCH`.

| Component | Incremented when |
|---|---|
| `MAJOR` | Incompatible breaking change (e.g. config schema bump requiring migration) |
| `MINOR` | New backwards-compatible feature (new subcommand, new optional config field) |
| `PATCH` | Bug fix with no behaviour change |

The version is defined once in `CMakeLists.txt`:

```cmake
project(projot VERSION 0.1.0)
```

`projot --version` prints the version and exits 0:

```
projot 0.1.0
```

The version string is baked into the binary at compile time via a `VERSION_STRING` preprocessor define set in `CMakeLists.txt`. No runtime file reads required.

#### Relationship to `config_version`

These are **independent numbers** with different purposes:

| | `config_version` | App version (`MAJOR.MINOR.PATCH`) |
|---|---|---|
| What it tracks | Schema of `.projot/config` | Feature set and compatibility of the binary |
| Format | Integer (1, 2, 3, …) | Semantic version string |
| Who sets it | projot writes it to `.projot/config` on `init` | Set by the developer in `CMakeLists.txt` at release time |
| When it changes | Only when the config format changes in a breaking way | On every release |

A `MAJOR` app version bump does not automatically mean `config_version` increments — only actual config schema changes trigger that.
