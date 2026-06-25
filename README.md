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
- Track todos with stable numeric IDs and four states: **Todo**, **In Progress**, **Blocked**, **Done**.
- Append notes under each todo.
- Store project metadata: RPM number, iTrack, project name, app ID, created date.
- Manage GitHub, Swagger, and Blizzard URL lists per repo.
- Configurable Links section with Teams, iTrack, RPM, and other single-value URLs.
- List open, closed, or all todos.
- Automatic Teams Kanban sync on every commit (via legacy webhook or Workflows/Power Automate endpoint).
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

# Update status (todo / in-progress / blocked / done)
projot status --todo 1 in-progress

# Complete a todo (shorthand for status done)
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
     - Waiting on supervisor feedback

2. [>] Implement widget renderer
   - Created: 2025-11-22
   - Notes:

3. [~] Performance review
   - Created: 2025-11-21
   - Notes:
     - Blocked on load test results

4. [x] Initial setup complete
   - Created: 2025-11-20
   - Completed: 2025-11-21
   - Notes:
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
links = teams, itrack, rpm
label.teams = Teams
link.teams = https://teams.microsoft.com/...
teams_sync_url = https://prod-00.westus.logic.azure.com:443/workflows/...
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
| `status` | Set todo status (`--todo <ID> todo\|in-progress\|blocked\|done`) |
| `complete` | Mark a todo done — shorthand for `status done` (`--todo <ID>`) |
| `add-note` | Add a note to a todo (`--todo <ID> "note text"`) |
| `set-link` | Set a single-value link URL (`--key <key> --url <url>`) |
| `set-app-id` | Update the app ID (`--force` required if already set) |
| `add-github` | Add a GitHub URL to config |
| `add-swagger` | Add a Swagger URL to config |
| `add-blizzard` | Add a Blizzard URL to config |
| `add-azure` | Add an Azure resource URL (`--type <type> --url <url> [--name <label>]`) |
| `render` | Re-render the notes file from config |
| `set-global` | Set global defaults (`--rpm-base-url`, `--itrack-base-url`) |
| `set-teams-webhook` | Set the Teams sync endpoint URL for Kanban sync |
| `install-hook` | Install the pre-commit git hook |
| `uninstall-hook` | Remove the pre-commit git hook |
| `install-mcp-server` | Configure the MCP server in your IDE settings |
| `uninstall-mcp-server` | Remove the MCP server from your IDE settings |

---

## Teams sync setup

Microsoft Teams no longer exposes the old **Connectors > Incoming Webhook** flow in every channel. `projot` now supports two endpoint styles for Kanban sync:

- **Legacy Teams incoming webhook URLs** (`*.webhook.office.com`, `outlook.office.com`, etc.)
- **Power Automate / Workflows HTTP trigger URLs**

If the legacy connector UI is unavailable, create a Power Automate / Workflows flow that accepts an HTTP request and posts to the target Teams channel:

1. Open **Workflows** in Teams (or Power Automate in the browser) and create a new cloud flow.
2. Choose a trigger that exposes an HTTP POST URL such as **When an HTTP request is received**.
3. Add a Teams action such as **Post message in a chat or channel** or **Post adaptive card in a chat or channel**.
4. Map the `projot` payload fields into that action. The most useful fields are `summary`, `card`, and the `kanban.*` arrays.
5. Save the flow, then copy the generated HTTP URL from the trigger.

Store that URL with:

```sh
projot set-teams-webhook https://prod-00.westus.logic.azure.com:443/workflows/...
```

If you are creating the project for the first time, you can also pass the same URL to `projot new --teams-sync-url ...`.

For legacy Teams webhook URLs, `projot` posts the Adaptive Card payload directly. For other endpoint URLs, `projot` posts a generic JSON payload with these fields so your flow can decide how to render it:

- `projectName`
- `rpm`
- `summary`
- `card`
- `kanban.todo`
- `kanban.inProgress`
- `kanban.blocked`
- `kanban.done`

For Microsoft setup guidance, see the official Power Automate documentation at <https://learn.microsoft.com/power-automate/> and search for **"When an HTTP request is received"** and **"Post adaptive card in a chat or channel"**.

`projot` stores the sync URL, but it does not provision Teams connectors or flows automatically.

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
- **Add TODOs**: *"Add a TODO: fix the Windows build"*
- **Update status**: *"Mark TODO #3 as in-progress"* → Sets the Kanban state
- **Complete TODOs**: *"Close TODO #2"* → Marks it done
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
