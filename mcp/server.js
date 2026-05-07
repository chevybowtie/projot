#!/usr/bin/env node

import { execSync } from "child_process";
import { createInterface } from "readline";
import { cwd } from "process";
import { readFileSync } from "fs";
import { join } from "path";
import { platform } from "os";

// MCP protocol: read stdin, write stdout
const rl = createInterface({ input: process.stdin });

rl.on("line", (line) => {
  try {
    const request = JSON.parse(line);
    const response = handleRequest(request);
    console.log(JSON.stringify(response));
  } catch (err) {
    console.error(JSON.stringify({ error: err.message }));
  }
});

function execCommand(cmd) {
  try {
    return execSync(cmd, { encoding: "utf8", cwd: cwd() });
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
    return {
      tools: [
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
          description: "Mark a TODO as completed",
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
                description: "The RANP/project number (e.g., 12345)",
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
      ],
    };
  }

  // Tool execution
  if (method === "tools/call") {
    const { name, arguments: args } = params;

    try {
      if (name === "get_open_todos") {
        const output = execCommand("projot list --open");
        return { result: output };
      }

      if (name === "complete_todo") {
        const { todo_id } = args;
        execCommand(`projot complete --todo ${todo_id}`);
        return { result: `TODO #${todo_id} marked as completed` };
      }

      if (name === "add_todo") {
        const { text } = args;
        execCommand(`projot add-todo --text "${text.replace(/"/g, '\\"')}"`);
        return { result: `TODO added: "${text}"` };
      }

      if (name === "add_note_to_todo") {
        const { todo_id, text } = args;
        execCommand(
          `projot add-note --todo ${todo_id} --text "${text.replace(/"/g, '\\"')}"`
        );
        return {
          result: `Note added to TODO #${todo_id}`,
        };
      }

      if (name === "open_itrack") {
        let itrackUrl = getConfigValue("link.itrack");
        if (!itrackUrl) {
          const baseUrl = getConfigValue("itrack_base_url") || getGlobalConfigValue("itrack_base_url");
          const number = getConfigValue("itrack");
          if (baseUrl && number) {
            itrackUrl = baseUrl + number;
          }
        }
        if (!itrackUrl) {
          return { error: "No iTrack URL configured. Set link.itrack or run 'projot set-global --itrack-base-url <url>'" };
        }

        const openCmd = {
          darwin: "open",
          linux: "xdg-open",
          win32: "start",
        }[platform()];

        if (!openCmd) {
          return { error: `Unsupported platform: ${platform()}` };
        }

        execCommand(`${openCmd} "${itrackUrl}"`);
        return { result: `Opening iTrack: ${itrackUrl}` };
      }

      if (name === "setup_new_project") {
        const { project_number, description, itrack_number, branch_name } =
          args;

        const suggestedBranch =
          branch_name ||
          `feat/${project_number}-${slugifyBranchName(description)}`;

        try {
          execCommand(`git checkout -b ${suggestedBranch}`);
          const teamsUrl = getConfigValue("link.teams");
          const projotCmd = `projot new --ranp ${project_number} --name "${description.replace(/"/g, '\\"')}" --itrack ${itrack_number}${teamsUrl ? ` --teams "${teamsUrl}"` : ""}`;
          execCommand(projotCmd);

          return {
            result: `Project setup complete:\n- Branch: ${suggestedBranch}\n- Project: ${project_number} - ${description}\n- iTrack: ${itrack_number}`,
          };
        } catch (err) {
          return { error: err.message };
        }
      }

      if (name === "open_github") {
        const url = getConfigValue("github") || getConfigValue("link.github");
        if (!url)
          return { error: "No GitHub URL configured in .projot/config" };
        const openCmd = {
          darwin: "open",
          linux: "xdg-open",
          win32: "start",
        }[platform()];
        if (!openCmd)
          return { error: `Unsupported platform: ${platform()}` };
        execCommand(`${openCmd} "${url}"`);
        return { result: `Opening GitHub: ${url}` };
      }

      if (name === "open_swagger") {
        const url = getConfigValue("swagger") || getConfigValue("link.swagger");
        if (!url)
          return { error: "No Swagger URL configured in .projot/config" };
        const openCmd = {
          darwin: "open",
          linux: "xdg-open",
          win32: "start",
        }[platform()];
        if (!openCmd)
          return { error: `Unsupported platform: ${platform()}` };
        execCommand(`${openCmd} "${url}"`);
        return { result: `Opening Swagger: ${url}` };
      }

      if (name === "open_blizzard") {
        const url =
          getConfigValue("blizzard") || getConfigValue("link.blizzard");
        if (!url)
          return { error: "No Blizzard URL configured in .projot/config" };
        const openCmd = {
          darwin: "open",
          linux: "xdg-open",
          win32: "start",
        }[platform()];
        if (!openCmd)
          return { error: `Unsupported platform: ${platform()}` };
        execCommand(`${openCmd} "${url}"`);
        return { result: `Opening Blizzard: ${url}` };
      }

      if (name === "open_teams") {
        const url = getConfigValue("link.teams");
        if (!url) return { error: "No Teams URL configured in .projot/config" };
        const openCmd = {
          darwin: "open",
          linux: "xdg-open",
          win32: "start",
        }[platform()];
        if (!openCmd)
          return { error: `Unsupported platform: ${platform()}` };
        execCommand(`${openCmd} "${url}"`);
        return { result: `Opening Teams: ${url}` };
      }

      if (name === "open_rpm") {
        let rpmUrl = getConfigValue("link.rpm");
        if (!rpmUrl) {
          const baseUrl = getConfigValue("rpm_base_url") || getGlobalConfigValue("rpm_base_url");
          const number = getConfigValue("rpm");
          if (baseUrl && number) {
            rpmUrl = baseUrl + number;
          }
        }
        if (!rpmUrl) {
          return { error: "No RPM URL configured. Set link.rpm or run 'projot set-global --rpm-base-url <url>'" };
        }
        const openCmd = {
          darwin: "open",
          linux: "xdg-open",
          win32: "start",
        }[platform()];
        if (!openCmd)
          return { error: `Unsupported platform: ${platform()}` };
        execCommand(`${openCmd} "${rpmUrl}"`);
        return { result: `Opening RPM: ${rpmUrl}` };
      }

      if (name === "set_teams_link") {
        const { url } = args;
        execCommand(`projot set-link --key teams --url "${url}"`);
        return { result: `Teams URL updated: ${url}` };
      }

      if (name === "set_github_link") {
        const { url } = args;
        execCommand(`projot add-github --url "${url}"`);
        return { result: `GitHub URL updated: ${url}` };
      }

      if (name === "set_swagger_link") {
        const { url } = args;
        execCommand(`projot add-swagger --url "${url}"`);
        return { result: `Swagger URL updated: ${url}` };
      }

      if (name === "set_blizzard_link") {
        const { url } = args;
        execCommand(`projot add-blizzard --url "${url}"`);
        return { result: `Blizzard URL updated: ${url}` };
      }

      return { error: `Unknown tool: ${name}` };
    } catch (err) {
      return { error: err.message };
    }
  }

  // MCP initialization
  if (method === "initialize") {
    return {
      protocolVersion: "2024-11-05",
      capabilities: {},
      serverInfo: {
        name: "projot-mcp",
        version: "1.0.0",
      },
    };
  }

  return { error: `Unknown method: ${method}` };
}
