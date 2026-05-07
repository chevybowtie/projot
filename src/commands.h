#pragma once

#include "cli.h"

// Each function returns an exit code (0 = success, non-zero = error).

// Initialize projot for the current git repository.
int cmd_init(const Args& args);

// Start a new RPM project in this repository.
int cmd_new(const Args& args);

// Append a new todo to the project notes file.
int cmd_add_todo(const Args& args);

// Display project summary and todos (filtered by status).
int cmd_list(const Args& args);

// Mark a todo as completed.
int cmd_complete(const Args& args);

// Add a note under a todo.
int cmd_add_note(const Args& args);

// Set or update a single-value link URL (teams, itrack, rpm, other).
int cmd_set_link(const Args& args);

// Set or update the application ID in config.
int cmd_set_app_id(const Args& args);

// Add a GitHub repository URL.
int cmd_add_github(const Args& args);

// Add a Swagger/OpenAPI endpoint URL.
int cmd_add_swagger(const Args& args);

// Add a Blizzard project URL.
int cmd_add_blizzard(const Args& args);

// Add an Azure resource entry (subscription, key-vault, AKS, etc.).
int cmd_add_azure(const Args& args);

// Re-render the notes file and stage it for commit.
int cmd_render(const Args& args);

// Install or re-install the pre-commit git hook.
int cmd_install_hook(const Args& args);

// Configure MCP server integration for Claude Code and VS Code.
int cmd_install_mcp_server(const Args& args);
int cmd_set_global(const Args& args);
