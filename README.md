# projot

- [Features (v0.1)](#features-v01)
- [Quickstart](#quickstart)
  - [1. Initialize the repository](#1-initialize-the-repository)
  - [2. Start a project](#2-start-a-project)
  - [3. Work with todos](#3-work-with-todos)
  - [4. Manage URLs](#4-manage-urls)
- [File Layout](#file-layout)
- [Configuration](#configuration)
- [Commands](#commands)
- [Project Structure](#project-structure)
- [Building](#building)
- [Installation](#installation)
- [AI Integration (MCP Server)](#ai-integration-mcp-server)
- [Roadmap](#roadmap)
- [License](#license)
- [Contributing](#contributing)

projot is a cross-platform C++ command-line tool (Linux + Windows) that replaces a physical desk notepad for tracking project work, todos, notes, and URLs.

projot is **repo-centric** — it runs inside a git repository and stores all project configuration and notes in a `.projot/` directory at the repo root, versioned alongside your code.

---

## Features (v0.1)

- Initialize a repo with app-level metadata (`init`) then start a project (`new`).
- Track todos with created/completed dates and stable numeric IDs.
- Append notes under each todo.
- Store project metadata: RPM number, iTrack, project name, app ID, created date, last updated date.
- Manage GitHub, Swagger, and Blizzard URL lists per repo.
- Configurable Links section with Teams, iTrack, RPM, and other single-value URLs.
- List open, closed, or all todos.
- Cross-platform (Linux + Windows).
- C++ standard library only — no external dependencies.
- Fully unit-tested.

---

## Quickstart

### 1. Initialize the repository
Run once per repo to set repo-level metadata:
```
projot init --app-id MyApp --github https://github.com/org/repo
```

### 2. Start a project
Run when assigned a new RPM project:
```
projot new --rpm 12345 --name "Widget Redesign" --itrack 67890 --teams https://teams.microsoft.com/...
```

### 3. Work with todos
```
# Add a todo
projot add-todo "Validate index rebuild plan"

# List open todos
projot list

# Complete a todo
projot complete --todo 1

# Add a note to a todo
projot add-note --todo 1 "Waiting on supervisor feedback"
```

### 4. Manage URLs
```
# Add a GitHub repo URL
projot add-github --url https://github.com/org/another-repo

# Set a single-value link URL
projot set-link --key teams --url https://teams.microsoft.com/new-channel
```

---

## File Layout

```
{repo_root}/
└── .projot/
    ├── config          ← project configuration (key = value)
    └── {RPM}.md        ← project notes file
```

### Notes file example

```markdown
# Project: Widget Redesign

- RPM: 12345
- iTrack: 67890
- App ID: MyApp
- Created: 2025-11-23
- Last Updated: 2025-11-23

## Links
- Teams: https://teams.microsoft.com/...
- iTrack: https://itrack.example.com/67890
- RPM: https://rpm.example.com/12345

## GitHub
- https://github.com/org/repo

## Swagger
- https://api.example.com/swagger

## Todos

1. [ ] Validate index rebuild plan
   - Created: 2025-11-23
   - Notes:
    - 2025-11-23 - Waiting on supervisor feedback

2. [x] Initial setup complete
   - Created: 2025-11-20
   - Completed: 2025-11-21
```

---

## Configuration

`.projot/config` is a `key = value` text file with two categories of fields.

**Repo-level** (set by `init`):
```
app_id = MyApp
github = https://github.com/org/repo
swagger = https://api.example.com/swagger
blizzard = https://blizzard.example.com/project
```

**Project-level** (set by `new`):
```
rpm = 12345
name = Widget Redesign
itrack = 67890
date_format = YYYY-MM-DD
links = teams, itrack, rpm
label.teams = Teams
link.teams = https://teams.microsoft.com/...

Date format tokens (display only):

- Supported tokens: `YYYY` (4-digit year), `MM` (zero-padded month), `DD` (zero-padded day).
- Examples: `YYYY-MM-DD` → 2026-05-15, `DD/MM/YYYY` → 15/05/2026, `MM-DD-YYYY` → 05-15-2026.
- Behavior: if `date_format` is empty, projot falls back to ISO (`YYYY-MM-DD`). projot performs simple token replacement only — no locale-aware names, no time-of-day, and no timezone conversion.
- Notes: named presets (e.g. `preset:US`) are not implemented in v0.1; rely on the token patterns above for predictable output.
```

---

## Commands

| Command | Description |
|---|---|
| `init` | Initialize projot for this repo (repo-level setup) |
| `new` | Start a new RPM project in this repo |
| `close` | Close the current project (clears project-level config) |
| `add-todo` | Append a todo (`projot add-todo "message"`) |
| `list` | Display todos (`--open` default / `--closed` / `--all`) |
| `complete` | Mark a todo completed (`--todo <ID>`) |
| `add-note` | Add a note to a todo (`--todo <ID> --text "..."`) |
| `set-link` | Set a single-value link URL (`--key <key> --url <url>`) |
| `set-app-id` | Update the app ID (`--force` required if already set) |
| `add-github` | Add a GitHub URL to config |
| `add-swagger` | Add a Swagger URL to config |
| `add-blizzard` | Add a Blizzard URL to config |
| `add-azure` | Add an Azure resource URL (`--type <type> --url <url> [--name <label>]`) |
| `render` | Re-render the notes file from config |
| `set-global` | Set global defaults (`--rpm-base-url`, `--itrack-base-url`) |
| `install-hook` | Install the pre-commit git hook |
| `uninstall-hook` | Remove the pre-commit git hook |
| `install-mcp-server` | Configure the MCP server in your IDE settings |
| `uninstall-mcp-server` | Remove the MCP server from your IDE settings |

---

## Project Structure

```
projot/
├── docs/
│   └── DESIGN.md
├── src/
├── tests/
│   └── data/
├── CMakeLists.txt
└── README.md
```

---

## Installation

Choose your platform:

- **[Linux Installation](docs/INSTALL-LINUX.md)** — Debian package, binary, tarball, or build from source
- **[Windows Installation](docs/INSTALL-WINDOWS.md)** — Chocolatey, manual binary, or build from source

Both guides cover binary installation, shell completion, and building from source for your platform.

## Building

Requires C++17 and CMake. No external dependencies.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # run tests
```

Platform-specific build instructions are in the Linux and Windows installation guides.

---

## AI Integration (MCP Server)

The `mcp/` directory contains a **Model Context Protocol server** that integrates `projot` with Claude, Copilot, and other AI assistants.

With the MCP server configured, you can ask your AI assistant in your IDE to:

- **Show TODOs**: *"Show my open TODOs"* → Lists them with IDs
- **Complete TODOs**: *"Close TODO #2"* → Marks it done
- **Add TODOs**: *"Add a TODO: fix the Windows build"*
- **Set up projects**: *"Help me set up a new project"* → Creates branch, initializes projot metadata
- **Open time tracking**: *"Open iTrack to charge time"* → Opens your iTrack URL in the browser

See [mcp/README.md](mcp/README.md) for installation and usage instructions.

---

## Roadmap

See [docs/DESIGN.md](docs/DESIGN.md) for the full design specification.

Planned post-v0.1 features:
- Interactive TUI (ncurses)
- Search and filtering across todos
- Weekly/monthly reporting
- Export to HTML/PDF
- Project archiving

---

## License

MIT License — see [LICENSE](LICENSE). You are free to use, modify, and distribute this software for any purpose, provided the copyright notice is preserved.

---

## Contributing

Contributions welcome after v0.1 is released. All PRs must include unit tests and must not introduce external dependencies.
