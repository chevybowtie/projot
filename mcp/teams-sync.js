#!/usr/bin/env node
// teams-sync.js — post a Kanban Adaptive Card to a Teams incoming webhook.
// Invoked by `projot render` when teams_webhook is configured.
// Usage: node teams-sync.js <config_path> <notes_md_path> <webhook_url>
// Always exits 0; sync failure must never block a git commit.

"use strict";

const https = require("https");
const fs = require("fs");
const url = require("url");

// ── arg parsing ───────────────────────────────────────────────────────────────

const [configPath, notesPath, webhookUrl] = process.argv.slice(2);

if (!configPath || !notesPath || !webhookUrl) {
  process.stderr.write("teams-sync: usage: node teams-sync.js <config_path> <notes_path> <webhook_url>\n");
  process.exit(0);
}

// ── config reader ─────────────────────────────────────────────────────────────

function readConfigValue(text, key) {
  const m = text.match(new RegExp(`^${key}\\s*=\\s*(.+)$`, "m"));
  return m ? m[1].trim() : "";
}

// ── todo parser ───────────────────────────────────────────────────────────────

// Matches lines like:  "1. [ ] Todo text"  or  "3. [>] In progress"
const TODO_RE = /^(\d+)\. \[(.)\] (.+)$/;

function parseTodos(mdText) {
  const buckets = { todo: [], inProgress: [], blocked: [], done: [] };
  for (const line of mdText.split("\n")) {
    const m = line.match(TODO_RE);
    if (!m) continue;
    const [, id, marker, text] = m;
    const entry = `${id}. ${text.trim()}`;
    if (marker === " ") buckets.todo.push(entry);
    else if (marker === ">") buckets.inProgress.push(entry);
    else if (marker === "~") buckets.blocked.push(entry);
    else if (marker === "x") buckets.done.push(entry);
  }
  return buckets;
}

// ── adaptive card builder ─────────────────────────────────────────────────────

function textBlock(text, options) {
  return Object.assign({ type: "TextBlock", text, wrap: true }, options || {});
}

function kanbanColumn(title, items, color) {
  const cells = [textBlock(title, { weight: "Bolder", color: color || "Default" })];
  if (items.length === 0) {
    cells.push(textBlock("—", { isSubtle: true, size: "Small" }));
  } else {
    for (const item of items) {
      cells.push(textBlock("• " + item, { size: "Small" }));
    }
  }
  return { type: "Column", width: "stretch", items: cells };
}

function buildCard(projectName, rpm, buckets) {
  const card = {
    $schema: "http://adaptivecards.io/schemas/adaptive-card.json",
    type: "AdaptiveCard",
    version: "1.4",
    body: [
      textBlock(`Kanban: ${projectName} (RPM ${rpm})`, { weight: "Bolder", size: "Medium" }),
      {
        type: "ColumnSet",
        columns: [
          kanbanColumn("TODO",        buckets.todo,       "Default"),
          kanbanColumn("In Progress", buckets.inProgress, "Accent"),
          kanbanColumn("Blocked",     buckets.blocked,    "Attention"),
          kanbanColumn("Done",        buckets.done,       "Good"),
        ],
      },
    ],
  };

  return {
    type: "message",
    attachments: [
      {
        contentType: "application/vnd.microsoft.card.adaptive",
        content: card,
      },
    ],
  };
}

// ── http post ─────────────────────────────────────────────────────────────────

function postJson(webhookUrl, body) {
  return new Promise((resolve, reject) => {
    const parsed = new url.URL(webhookUrl);
    const payload = JSON.stringify(body);
    const options = {
      hostname: parsed.hostname,
      port: parsed.port || 443,
      path: parsed.pathname + parsed.search,
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "Content-Length": Buffer.byteLength(payload),
      },
    };

    const req = https.request(options, (res) => {
      let data = "";
      res.on("data", (chunk) => (data += chunk));
      res.on("end", () => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(data);
        } else {
          reject(new Error(`HTTP ${res.statusCode}: ${data.slice(0, 200)}`));
        }
      });
    });
    req.on("error", reject);
    req.setTimeout(8000, () => {
      req.destroy(new Error("request timed out"));
    });
    req.write(payload);
    req.end();
  });
}

// ── main ──────────────────────────────────────────────────────────────────────

async function main() {
  let configText, notesText;
  try {
    configText = fs.readFileSync(configPath, "utf8");
    notesText  = fs.readFileSync(notesPath,  "utf8");
  } catch (e) {
    process.stderr.write(`teams-sync: warning: cannot read files: ${e.message}\n`);
    return;
  }

  const projectName = readConfigValue(configText, "name") || "Unknown project";
  const rpm         = readConfigValue(configText, "rpm")  || "?";
  const buckets     = parseTodos(notesText);
  const card        = buildCard(projectName, rpm, buckets);

  try {
    await postJson(webhookUrl, card);
  } catch (e) {
    process.stderr.write(`teams-sync: warning: Teams sync failed: ${e.message}\n`);
  }
}

main().catch((e) => {
  process.stderr.write(`teams-sync: warning: unexpected error: ${e.message}\n`);
});
