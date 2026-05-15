# Projot MCP Server

This is a Model Context Protocol (MCP) server that lets AI assistants (Claude, Copilot, etc.) call `projot` CLI commands to manage TODOs.

## Why Use This?

**Problem:** You're using Copilot or Claude in your IDE, and you want to ask: *"Show me my open TODOs and close any I just worked on."*

**Solution:** This MCP server exposes `projot` commands as tools that Claude/Copilot can call directly.

## Installation

### Prerequisites

1. **Node.js 16+** — The MCP server is written in Node.js
2. **`projot` binary** — Must be built and installed in your `PATH`:

   ```bash
   cd /path/to/projot
   make
   sudo make install
   ```

The MCP server is bundled with the `projot` install (for example `/usr/local/share/projot/mcp/server.js` or `~/.local/share/projot/mcp/server.js`).

### Configure Your IDE

Use the built-in command from your repository root:

```bash
projot install-mcp-server
```

This writes:

- `.claude/settings.json`
- `.vscode/mcp.json`

Both files are configured to call the bundled MCP server path (not `./mcp/server.js`).

If you only want Claude configuration:

```bash
projot install-mcp-server --no-vscode
```

You can remove the configuration later with:

```bash
projot uninstall-mcp-server
```

#### Manual configuration (if needed)

Use the same absolute server path that `projot install-mcp-server` would write. Common examples:

- Linux/macOS system install: `/usr/local/share/projot/mcp/server.js`
- Linux/macOS user install: `~/.local/share/projot/mcp/server.js`
- Windows: `C:\Program Files\projot\share\projot\mcp\server.js`

#### Claude Code

1. Open `.claude/settings.json` in your project root
2. Add the `mcpServers` section:

```json
{
  "mcpServers": {
    "projot": {
      "command": "node",
      "args": ["/usr/local/share/projot/mcp/server.js"]
    }
  }
}
```

#### Copilot (VS Code)

1. Open `.vscode/mcp.json`
2. Add the MCP server configuration (`inputs` is part of the VS Code MCP file shape and can be left as an empty array):

```json
{
  "inputs": [],
  "servers": {
    "projot": {
      "type": "stdio",
      "command": "node",
      "args": ["/usr/local/share/projot/mcp/server.js"]
    }
  }
}
```

### Verify Setup

Test that the server works:

```bash
cd /path/to/projot
npm test
```

You should see a JSON response from the server. If it fails:

- Ensure `projot` is in your `PATH`: `which projot`
- Ensure you're in a git repo with `.projot/` initialized: `projot list` should work

## Tools

### `get_open_todos`

List all open TODOs in the current project (calls `projot list --open`).

**Example workflow:**

1. You ask: *"Show me my open TODOs"*
2. Claude/Copilot calls `get_open_todos`
3. Displays them with IDs
4. You ask: *"Close TODO #2, I just finished it"*
5. Claude/Copilot calls `complete_todo(2)`

### `complete_todo`

Mark a TODO as completed.

**Parameters:**

- `todo_id` (number): The numeric ID shown in `projot list`

### `add_todo`

Create a new TODO.

**Parameters:**

- `text` (string): The TODO description

### `add_note_to_todo`

Add a note/comment to an existing TODO.

**Parameters:**

- `todo_id` (number): The numeric ID
- `text` (string): The note

### `open_itrack`

Open the iTrack URL in your default browser to charge time to the project.

Reads the `link.itrack` URL from `.projot/config` and opens it using the OS-appropriate command (`open` on macOS, `xdg-open` on Linux, `start` on Windows).

**Example:**

1. You ask: *"Open iTrack so I can charge time"*
2. Claude/Copilot calls `open_itrack`
3. Your browser opens to the iTrack timesheet URL

### `setup_new_project`

Set up a new project: create a git branch, initialize projot metadata, and switch to the branch.

**Parameters:**

- `project_number` (string): The RANP/project number (e.g., "12345")
- `description` (string): Short project title/description
- `itrack_number` (string): The iTrack number for time tracking
- `branch_name` (string, optional): Git branch name. If not provided, auto-generates `feat/{project_number}-{slug}` (e.g., `feat/12345-widget-redesign`)

**Example workflow:**

1. You ask: *"Help me set up a new project"*
2. Claude asks for: project number, description, iTrack number
3. Claude suggests a branch name: `feat/12345-widget-redesign`
4. You confirm
5. Claude calls `setup_new_project` with all parameters
6. Tool creates the branch, switches to it, and runs `projot new`
7. You're ready to start working on the project

### Open Links

Open project resources in your default browser.

- `open_github` — Open the GitHub repository
- `open_swagger` — Open the Swagger/API documentation
- `open_blizzard` — Open the Blizzard URL
- `open_teams` — Open the Microsoft Teams channel

**Example:**

1. You ask: *"Open GitHub"*
2. Claude calls `open_github`
3. Browser opens to the project's GitHub repo

### Set Links

Configure or update project resource URLs.

- `set_github_link` — Update the GitHub repository URL
- `set_swagger_link` — Update the Swagger/API documentation URL
- `set_blizzard_link` — Update the Blizzard URL
- `set_teams_link` — Update the Microsoft Teams channel URL

**Parameters:**

- `url` (string): The resource URL to set

**Example:**

1. You ask: *"Update the Teams URL to [new URL]"*
2. Claude calls `set_teams_link` with the URL
3. The project's Teams link is updated

## Testing

```bash
# Test the server responds
npm test

# Try it interactively (requires projot in PATH)
npm start
# Then send a JSON request on stdin, e.g.:
# {"method":"initialize"}
```

## How It Works

1. When you ask Claude/Copilot to show TODOs, it discovers available tools via `tools/list`
2. It calls `get_open_todos`, which runs `projot list --open` and returns the output
3. It displays TODOs to you
4. When you ask to close one, it calls `complete_todo(id)`, which runs `projot complete --todo <id>`
5. All changes persist in `.projot/` markdown files (projot's native format)

The server is just a bridge—all the actual TODO management is done by `projot` itself.
