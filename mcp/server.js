#!/usr/bin/env node

import { execFileSync } from "child_process";
import { createInterface } from "readline";
import { cwd } from "process";
import { readFileSync } from "fs";
import { join } from "path";
import { platform } from "os";

// MCP protocol: JSON-RPC 2.0 over stdin/stdout
const rl = createInterface({ input: process.stdin });

rl.on("line", (line) => {
  try {
    const request = JSON.parse(line);

    // Notifications have no id — do not respond
    if (!("id" in request)) return;

    const response = handleRequest(request);

    if ("error" in response) {
      console.log(JSON.stringify({
        jsonrpc: "2.0",
        id: request.id,
        error: { code: -32000, message: response.error },
      }));
    } else {
      console.log(JSON.stringify({
        jsonrpc: "2.0",
        id: request.id,
        result: response.result,
      }));
    }
  } catch (err) {
    console.log(JSON.stringify({
      jsonrpc: "2.0",
      id: null,
      error: { code: -32700, message: err.message },
    }));
  }
});

function execArgs(cmd, args) {
  try {
    return execFileSync(cmd, args, { encoding: "utf8", cwd: cwd() });
  } catch (err) {
    throw new Error(`Command failed: ${err.message}`);
  }
}

function getConfigValue(key) {
  try {
    const configPath = join(cwd(), ".projot", "config");
    const config = readFileSync(configPath, "utf8");
    const match = config.match(new RegExp(`^${key}\\s*=\\s*(.+)$`, "m"));
    return match ? match[1].trim() : null;
  } catch {
    throw new Error("Could not read .projot/config");
  }
}

function getGlobalConfigValue(key) {
  try {
    let base;
    if (process.platform === "win32") {
      // Windows: prefer APPDATA, fallback to USERPROFILE
      base = process.env.APPDATA || join(process.env.USERPROFILE, ".config");
    } else {
      // Unix/Linux/macOS: prefer XDG_CONFIG_HOME, fallback to ~/.config
      base = process.env.XDG_CONFIG_HOME || join(process.env.HOME, ".config");
    }
    const configPath = join(base, "projot", "config");
    const config = readFileSync(configPath, "utf8");
    const match = config.match(new RegExp(`^${key}\\s*=\\s*(.+)$`, "m"));
    return match ? match[1].trim() : null;
  } catch {
    return null;
  }
}

function slugifyBranchName(str) {
  return str
    .toLowerCase()
    .replace(/[^\w\s-]/g, "")
    .trim()
    .replace(/\s+/g, "-")
    .replace(/-+/g, "-")
    .substring(0, 30);
}

function handleRequest(request) {
  const { method, params } = request;

  // Tool discovery
  if (method === "tools/list") {
    return { result: { tools: [
        {
          name: "get_open_todos",
          description: "List all open TODOs in the current project",
          inputSchema: {
            type: "object",
            properties: {},
          },
        },
        {
          name: "complete_todo",
          description: "Mark a TODO as done (sets status to 'done')",
          inputSchema: {
            type: "object",
            properties: {
              todo_id: {
                type: "number",
                description: "The numeric ID of the TODO to complete",
              },
            },
            required: ["todo_id"],
          },
        },
        {
          name: "add_todo",
          description: "Create a new TODO",
          inputSchema: {
            type: "object",
            properties: {
              text: {
                type: "string",
                description: "The TODO text",
              },
            },
            required: ["text"],
          },
        },
        {
          name: "add_note_to_todo",
          description: "Add a note/comment to an existing TODO",
          inputSchema: {
            type: "object",
            properties: {
              todo_id: {
                type: "number",
                description: "The numeric ID of the TODO",
              },
              text: {
                type: "string",
                description: "The note text",
              },
            },
            required: ["todo_id", "text"],
          },
        },
        {
          name: "open_itrack",
          description:
            "Open the iTrack URL in the default browser to charge time to the project",
          inputSchema: {
            type: "object",
            properties: {},
          },
        },
        {
          name: "setup_new_project",
          description:
            "Set up a new project: create a branch, initialize projot metadata, and switch to the branch",
          inputSchema: {
            type: "object",
            properties: {
              project_number: {
                type: "string",
                description: "The RPM project number (e.g., 12345)",
              },
              description: {
                type: "string",
                description: "Short project description/title",
              },
              itrack_number: {
                type: "string",
                description: "The iTrack number for time tracking",
              },
              branch_name: {
                type: "string",
                description:
                  'Optional: git branch name. If not provided, generates feat/{project_number}-{slugified_description}',
              },
            },
            required: ["project_number", "description", "itrack_number"],
          },
        },
        {
          name: "open_github",
          description: "Open the GitHub repository URL in the default browser",
          inputSchema: { type: "object", properties: {} },
        },
        {
          name: "open_swagger",
          description:
            "Open the Swagger/API documentation URL in the default browser",
          inputSchema: { type: "object", properties: {} },
        },
        {
          name: "open_blizzard",
          description: "Open the Blizzard URL in the default browser",
          inputSchema: { type: "object", properties: {} },
        },
        {
          name: "open_teams",
          description:
            "Open the Microsoft Teams channel URL in the default browser",
          inputSchema: { type: "object", properties: {} },
        },
        {
          name: "set_teams_link",
          description: "Set or update the Teams channel URL for the project",
          inputSchema: {
            type: "object",
            properties: {
              url: {
                type: "string",
                description: "The Teams channel URL",
              },
            },
            required: ["url"],
          },
        },
        {
          name: "set_github_link",
          description: "Set or update the GitHub repository URL",
          inputSchema: {
            type: "object",
            properties: {
              url: {
                type: "string",
                description: "The GitHub repository URL",
              },
            },
            required: ["url"],
          },
        },
        {
          name: "set_swagger_link",
          description: "Set or update the Swagger/API documentation URL",
          inputSchema: {
            type: "object",
            properties: {
              url: {
                type: "string",
                description: "The Swagger/API documentation URL",
              },
            },
            required: ["url"],
          },
        },
        {
          name: "set_blizzard_link",
          description: "Set or update the Blizzard URL",
          inputSchema: {
            type: "object",
            properties: {
              url: {
                type: "string",
                description: "The Blizzard URL",
              },
            },
            required: ["url"],
          },
        },
        {
          name: "open_rpm",
          description: "Open the RPM project page in the default browser",
          inputSchema: { type: "object", properties: {} },
        },
        {
          name: "set_status",
          description: "Set the status of a todo: todo, in-progress, blocked, or done",
          inputSchema: {
            type: "object",
            properties: {
              todo_id: {
                type: "number",
                description: "The numeric ID of the TODO",
              },
              status: {
                type: "string",
                enum: ["todo", "in-progress", "blocked", "done"],
                description: "New status for the TODO",
              },
            },
            required: ["todo_id", "status"],
          },
        },
      ] } };
  }

  // Tool execution
  if (method === "tools/call") {
    const { name, arguments: args } = params;

    const ok  = (text) => ({ result: { content: [{ type: "text", text: String(text) }], isError: false } });
    const err = (text) => ({ result: { content: [{ type: "text", text: String(text) }], isError: true  } });
    const openUrl = (url) => {
      const p = platform();
      if (p === "darwin")       execFileSync("open",    [url]);
      else if (p === "linux")   execFileSync("xdg-open", [url]);
      else if (p === "win32")   execFileSync("cmd.exe", ["/c", "start", "", url]);
      else return err(`Unsupported platform: ${p}`);
      return null; // caller returns ok()
    };

    try {
      if (name === "get_open_todos") {
        return ok(execArgs("projot", ["list", "--open"]));
      }

      if (name === "complete_todo") {
        const { todo_id } = args;
        execArgs("projot", ["complete", "--todo", String(todo_id)]);
        return ok(`TODO #${todo_id} marked as completed`);
      }

      if (name === "add_todo") {
        const { text } = args;
        execArgs("projot", ["add-todo", text]);
        return ok(`TODO added: "${text}"`);
      }

      if (name === "add_note_to_todo") {
        const { todo_id, text } = args;
        execArgs("projot", ["add-note", "--todo", String(todo_id), text]);
        return ok(`Note added to TODO #${todo_id}`);
      }

      if (name === "open_itrack") {
        let itrackUrl = getConfigValue("link.itrack");
        if (!itrackUrl) {
          const baseUrl = getConfigValue("itrack_base_url") || getGlobalConfigValue("itrack_base_url");
          const number = getConfigValue("itrack");
          if (baseUrl && number) itrackUrl = baseUrl + number;
        }
        if (!itrackUrl) return err("No iTrack URL configured. Set link.itrack or run 'projot set-global --itrack-base-url <url>'");
        return openUrl(itrackUrl) || ok(`Opening iTrack: ${itrackUrl}`);
      }

      if (name === "open_rpm") {
        let rpmUrl = getConfigValue("link.rpm");
        if (!rpmUrl) {
          const baseUrl = getConfigValue("rpm_base_url") || getGlobalConfigValue("rpm_base_url");
          const number = getConfigValue("rpm");
          if (baseUrl && number) rpmUrl = baseUrl + number;
        }
        if (!rpmUrl) return err("No RPM URL configured. Set link.rpm or run 'projot set-global --rpm-base-url <url>'");
        return openUrl(rpmUrl) || ok(`Opening RPM: ${rpmUrl}`);
      }

      if (name === "open_github") {
        const url = getConfigValue("github") || getConfigValue("link.github");
        if (!url) return err("No GitHub URL configured in .projot/config");
        return openUrl(url) || ok(`Opening GitHub: ${url}`);
      }

      if (name === "open_swagger") {
        const url = getConfigValue("swagger") || getConfigValue("link.swagger");
        if (!url) return err("No Swagger URL configured in .projot/config");
        return openUrl(url) || ok(`Opening Swagger: ${url}`);
      }

      if (name === "open_blizzard") {
        const url = getConfigValue("blizzard") || getConfigValue("link.blizzard");
        if (!url) return err("No Blizzard URL configured in .projot/config");
        return openUrl(url) || ok(`Opening Blizzard: ${url}`);
      }

      if (name === "open_teams") {
        const url = getConfigValue("link.teams");
        if (!url) return err("No Teams URL configured in .projot/config");
        return openUrl(url) || ok(`Opening Teams: ${url}`);
      }

      if (name === "setup_new_project") {
        const { project_number, description, itrack_number, branch_name } = args;
        const suggestedBranch = branch_name || `feat/${project_number}-${slugifyBranchName(description)}`;
        execArgs("git", ["checkout", "-b", suggestedBranch]);
        const teamsUrl = getConfigValue("link.teams");
        const newArgs = ["new", "--rpm", project_number, "--name", description, "--itrack", itrack_number];
        if (teamsUrl) newArgs.push("--teams", teamsUrl);
        execArgs("projot", newArgs);
        return ok(`Project setup complete:\n- Branch: ${suggestedBranch}\n- Project: ${project_number} - ${description}\n- iTrack: ${itrack_number}`);
      }

      if (name === "set_teams_link") {
        execArgs("projot", ["set-link", "--key", "teams", "--url", args.url]);
        return ok(`Teams URL updated: ${args.url}`);
      }

      if (name === "set_github_link") {
        execArgs("projot", ["add-github", "--url", args.url]);
        return ok(`GitHub URL updated: ${args.url}`);
      }

      if (name === "set_swagger_link") {
        execArgs("projot", ["add-swagger", "--url", args.url]);
        return ok(`Swagger URL updated: ${args.url}`);
      }

      if (name === "set_blizzard_link") {
        execArgs("projot", ["add-blizzard", "--url", args.url]);
        return ok(`Blizzard URL updated: ${args.url}`);
      }

      if (name === "set_status") {
        const { todo_id, status } = args;
        execArgs("projot", ["status", "--todo", String(todo_id), status]);
        return ok(`TODO #${todo_id} status set to: ${status}`);
      }

      return err(`Unknown tool: ${name}`);
    } catch (e) {
      return err(e.message);
    }
  }

  // MCP initialization
  if (method === "initialize") {
    return { result: {
      protocolVersion: "2024-11-05",
      capabilities: {},
      serverInfo: { name: "projot-mcp", version: "1.0.0" },
    } };
  }

  return { error: `Unknown method: ${method}` };
}
