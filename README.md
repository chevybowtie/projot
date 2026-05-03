# projot

projot is a cross-platform C++ command-line tool (Linux + Windows) that replaces a physical desk notepad for tracking project work, todos, notes, and URLs.

projot is **repo-centric** — it runs inside a git repository and stores all project configuration and notes in a `.projot/` directory at the repo root, versioned alongside your code.

---

## Features (v0.1)

- Initialize a repo with app-level metadata (`init`) then start a project (`new`).
- Track todos with created/completed dates and stable numeric IDs.
- Append notes under each todo.
- Store project metadata: RANP number, iTrack, project name, app ID, created date.
- Manage GitHub, Swagger, and Blizzard URL lists per repo.
- Configurable Links section with Teams, iTrack, RANP, and other single-value URLs.
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
Run when assigned a new RANP project:
```
projot new --ranp 12345 --name "Widget Redesign" --itrack 67890 --teams https://teams.microsoft.com/...
```

### 3. Work with todos
```
# Add a todo
projot add-todo --text "Validate index rebuild plan"

# List open todos
projot list

# Complete a todo
projot complete --todo 1

# Add a note to a todo
projot add-note --todo 1 --text "Waiting on supervisor feedback"
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
    └── {RANP}.md       ← project notes file
```

### Notes file example

```markdown
# Project: Widget Redesign

- RANP: 12345
- iTrack: 67890
- App ID: MyApp
- Created: 2025-11-23

## Links
- Teams: https://teams.microsoft.com/...
- iTrack: https://itrack.example.com/67890
- RANP: https://ranp.example.com/12345

## GitHub
- https://github.com/org/repo

## Swagger
- https://api.example.com/swagger

## Todos

1. [ ] Validate index rebuild plan
   - Created: 2025-11-23
   - Notes:
     - Waiting on supervisor feedback

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
ranp = 12345
name = Widget Redesign
itrack = 67890
date_format = YYYY-MM-DD
links = teams, itrack, ranp
label.teams = Teams
link.teams = https://teams.microsoft.com/...
```

---

## Commands

| Command | Description |
|---|---|
| `init` | Initialize projot for this repo (repo-level setup) |
| `new` | Start a new RANP project in this repo |
| `add-todo` | Append a todo |
| `list` | Display todos (`--open` default / `--closed` / `--all`) |
| `complete` | Mark a todo completed (`--todo <ID>`) |
| `add-note` | Add a note to a todo (`--todo <ID> --text "..."`) |
| `set-link` | Set a single-value link URL (`--key <key> --url <url>`) |
| `set-app-id` | Update the app ID (`--force` required if already set) |
| `add-github` | Add a GitHub URL to config |
| `add-swagger` | Add a Swagger URL to config |
| `add-blizzard` | Add a Blizzard URL to config |

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

## Building

Requires C++17 and CMake. No external dependencies.

```sh
make          # Linux (release build)
make test     # build and run all tests
```

Or directly with CMake (all platforms):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

See [docs/DEVELOPER.md](docs/DEVELOPER.md) for full build, install, and shell completion instructions.

---

## Installation

### From a pre-built release (recommended)

Download the latest release from the [GitHub Releases](../../releases/latest) page.

#### Linux

```sh
# Download and install the binary
curl -Lo projot https://github.com/ORG/projot/releases/latest/download/projot-linux-x86_64
chmod +x projot
sudo mv projot /usr/local/bin/

# (Optional) verify the SHA-256 checksum printed on the release page
sha256sum projot
```

#### Windows

1. Download `projot-windows-x86_64.exe` from the release page.
2. Rename it to `projot.exe` and move it to a directory on your `PATH`  
   (e.g. `C:\Users\<you>\AppData\Local\Programs\projot\`).
3. Add that directory to `PATH` via **System Properties → Environment Variables**.

### Shell completion (optional)

Download the completion script for your shell from the same release page:

| Shell | File to download | Install path |
|---|---|---|
| Bash | `projot.bash` | `~/.local/share/bash-completion/completions/projot` |
| Zsh | `_projot` | `~/.zsh/completions/_projot` (must be on `$fpath`) |
| Fish | `projot.fish` | `~/.config/fish/completions/projot.fish` |
| PowerShell | `projot.ps1` | Dot-source from `$PROFILE` |

Restart your shell after installing.

### From source

```sh
git clone https://github.com/ORG/projot
cd projot
make
sudo make install               # installs to /usr/local/bin
make install-completion         # installs completion for current shell
```

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
