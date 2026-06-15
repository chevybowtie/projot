# MCP Server

The projot MCP server lets AI assistants (Claude Code, GitHub Copilot) call projot commands directly. Instead of typing `projot add-todo "..."` yourself, you can ask your AI assistant and it will call projot on your behalf — with changes persisting in `.projot/` as usual.

---

## Prerequisites

- **Node.js 16+** — the MCP server is a Node.js process
- **`projot` in your PATH** — must be installed system-wide or per-user

---

## Installation

From any git repo that has been initialized with `projot init`:

```sh
projot install-mcp-server
```

This writes two files:

| File | Used by |
|------|---------|
| `.claude/settings.json` | Claude Code |
| `.vscode/mcp.json` | GitHub Copilot (VS Code) |

Both point to the bundled server at the path where projot is installed (e.g. `/usr/local/share/projot/mcp/server.js`). You do not need to run it manually.

To remove the configuration:

```sh
projot uninstall-mcp-server
```

To skip the VS Code file:

```sh
projot install-mcp-server --no-vscode
```

---

## Using with Claude Code

Once installed, Claude will automatically call projot tools when you ask about project work items. To be explicit and bypass Claude's built-in task tracking, use the `/projot` slash command:

```
/projot show my open todos
/projot add a todo to write unit tests for the auth module
/projot complete todo 3
/projot set todo 2 to in-progress
```

The `/projot` command is available globally in Claude Code (not per-project). It ensures Claude uses the projot MCP tools rather than its own ephemeral session task list.

---

## Using with GitHub Copilot (VS Code)

After running `projot install-mcp-server`, Copilot Chat can call projot tools automatically. Be specific in your requests to avoid Copilot using its own task handling:

```
Show me my open projot work items
Add a projot todo: refactor the config parser
Complete projot work item #2
Open iTrack for this project
```

The word **projot** in your request is enough to steer Copilot toward the right tools.

---

## Available Tools

All tools are prefixed with `projot_` to avoid conflicts with built-in AI task management.

### Work items

| Tool | What it does |
|------|--------------|
| `projot_list_todos` | List open work items in the current project |
| `projot_add_todo` | Create a new work item |
| `projot_complete_todo` | Mark a work item done by ID |
| `projot_set_status` | Set status: `todo`, `in-progress`, `blocked`, or `done` |
| `projot_add_note` | Add a note to an existing work item |

### Project setup

| Tool | What it does |
|------|--------------|
| `projot_setup_project` | Create a branch, run `projot new`, and switch to it |

### Open links in browser

| Tool | What it does |
|------|--------------|
| `projot_open_itrack` | Open the iTrack timesheet URL |
| `projot_open_rpm` | Open the RPM project page |
| `projot_open_github` | Open the GitHub repository |
| `projot_open_swagger` | Open the Swagger/API docs |
| `projot_open_blizzard` | Open the Blizzard URL |
| `projot_open_teams` | Open the Teams channel |

### Update stored URLs

| Tool | What it does |
|------|--------------|
| `projot_set_github_link` | Update the GitHub URL in `.projot/config` |
| `projot_set_swagger_link` | Update the Swagger URL |
| `projot_set_blizzard_link` | Update the Blizzard URL |
| `projot_set_teams_link` | Update the Teams channel URL |

---

## Troubleshooting

**"No such file or directory" when starting the server**
The server path written by `install-mcp-server` is based on where projot was installed at build time. If you move or reinstall projot, re-run `projot install-mcp-server` to update the path.

**"projot: command not found" from the MCP server**
The server inherits the PATH from your IDE, which may differ from your shell. Ensure projot is installed in a standard location (`/usr/local/bin`, `~/.local/bin`) rather than a custom path only in your `.bashrc`.

**Copilot uses its own task list instead of projot**
Include the word "projot" in your request (e.g. "add a projot todo") to steer tool selection.

**Claude uses TodoWrite instead of projot**
Use the `/projot` slash command to route explicitly to projot tools.
