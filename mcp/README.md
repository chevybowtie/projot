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

The full list of tools (all prefixed `projot_`) is documented in [docs/MCP.md](../docs/MCP.md#available-tools). The authoritative definitions are in [server.js](server.js) (`tools/list` response).

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
2. It calls `projot_list_todos`, which runs `projot list --open` and returns the output
3. It displays TODOs to you
4. When you ask to close one, it calls `projot_complete_todo(id)`, which runs `projot complete --todo <id>`
5. All changes persist in `.projot/` markdown files (projot's native format)

The server is just a bridge—all the actual TODO management is done by `projot` itself.
