#!/usr/bin/env node
// teams-sync.js — post a Kanban update to a Teams sync endpoint.
// Invoked by `projot render` when teams_sync_url is configured.
// Usage: node teams-sync.js <config_path> <notes_md_path> <sync_url>
// Always exits 0; sync failure must never block a git commit.

"use strict";

import https from "https";
import fs from "fs";
import { URL } from "url";

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

function buildAdaptiveCard(projectName, rpm, buckets) {
  return {
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
}

function buildLegacyWebhookBody(card) {
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

function renderSummary(projectName, rpm, buckets) {
  const lines = [`Kanban: ${projectName} (RPM ${rpm})`, ""];
  for (const [title, items] of [
    ["TODO", buckets.todo],
    ["In Progress", buckets.inProgress],
    ["Blocked", buckets.blocked],
    ["Done", buckets.done],
  ]) {
    lines.push(`${title}:`);
    if (items.length === 0) {
      lines.push("- —");
    } else {
      for (const item of items) lines.push(`- ${item}`);
    }
    lines.push("");
  }
  return lines.join("\n").trim();
}

function buildWorkflowBody(projectName, rpm, buckets, card) {
  return {
    source: "projot",
    version: 1,
    projectName,
    rpm,
    summary: renderSummary(projectName, rpm, buckets),
    card,
    kanban: {
      todo: buckets.todo,
      inProgress: buckets.inProgress,
      blocked: buckets.blocked,
      done: buckets.done,
    },
  };
}

function isLegacyTeamsWebhook(syncUrl) {
  const parsed = new URL(syncUrl);
  const host = parsed.hostname.toLowerCase();
  return (
    host.endsWith(".webhook.office.com") ||
    host === "outlook.office.com" ||
    host === "outlook.office365.com" ||
    parsed.pathname.includes("/webhookb2/")
  );
}

function buildSyncBody(syncUrl, projectName, rpm, buckets) {
  const card = buildAdaptiveCard(projectName, rpm, buckets);
  return isLegacyTeamsWebhook(syncUrl)
    ? buildLegacyWebhookBody(card)
    : buildWorkflowBody(projectName, rpm, buckets, card);
}

// ── http post ─────────────────────────────────────────────────────────────────

function postJson(webhookUrl, body) {
  return new Promise((resolve, reject) => {
    const parsed = new URL(webhookUrl);
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

async function main(argv = process.argv.slice(2)) {
  const [configPath, notesPath, webhookUrl] = argv;
  if (!configPath || !notesPath || !webhookUrl) {
    process.stderr.write("teams-sync: usage: node teams-sync.js <config_path> <notes_path> <sync_url>\n");
    return;
  }

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
  const body        = buildSyncBody(webhookUrl, projectName, rpm, buckets);

  try {
    await postJson(webhookUrl, body);
  } catch (e) {
    process.stderr.write(`teams-sync: warning: Teams sync failed: ${e.message}\n`);
  }
}

const isMainModule = process.argv[1] && new URL(`file://${process.argv[1]}`).href === import.meta.url;
if (isMainModule) {
  main().catch((e) => {
    process.stderr.write(`teams-sync: warning: unexpected error: ${e.message}\n`);
  });
}

export {
  buildAdaptiveCard,
  buildSyncBody,
  buildWorkflowBody,
  isLegacyTeamsWebhook,
  parseTodos,
  renderSummary,
};
