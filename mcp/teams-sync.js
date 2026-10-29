#!/usr/bin/env node
// teams-sync.js — post a Kanban update to a Teams sync endpoint.
// Invoked by `projot render` when teams_sync_url is configured, and with --status
// (to teams_page_url) when a status page is configured.
// Usage: node teams-sync.js <config_path> <notes_md_path> [sync_url] [--summary|--status]
// Always exits 0; sync failure must never block a git commit.

"use strict";

import https from "https";
import fs from "fs";
import { URL } from "url";

// ── config reader ─────────────────────────────────────────────────────────────

function readConfigValue(text, key) {
  // [ \t]* rather than \s*: \s also matches newlines, so an empty value would
  // otherwise capture the whole next line.
  const m = text.match(new RegExp(`^${key}[ \\t]*=[ \\t]*(.*)$`, "m"));
  return m ? m[1].trim() : "";
}

// ── todo parser ───────────────────────────────────────────────────────────────

// Matches lines like:  "1. [ ] Todo text"  or  "3. [>] In progress"
const TODO_RE = /^(\d+)\. \[(.)\] (.+)$/;

// Each todo with its dates. Detail lines sit under the todo at exactly three
// spaces; note lines are indented deeper, so they cannot be mistaken for dates.
function parseTodoRecords(mdText) {
  const records = [];
  let current = null;
  for (const line of mdText.split("\n")) {
    const m = line.match(TODO_RE);
    if (m) {
      const [, id, marker, text] = m;
      current = { id, marker, text: text.trim(), created: "", completed: "", notes: [] };
      records.push(current);
      continue;
    }
    if (!current) continue;
    const created = line.match(/^ {3}- Created: (.*)$/);
    if (created) { current.created = created[1].trim(); continue; }
    const completed = line.match(/^ {3}- Completed: (.*)$/);
    if (completed) { current.completed = completed[1].trim(); continue; }
    const note = line.match(/^ {5}- (.+)$/);
    if (note) current.notes.push(note[1].trim());
  }
  return records;
}

function bucketTodos(records) {
  const buckets = { todo: [], inProgress: [], blocked: [], done: [] };
  for (const r of records) {
    const entry = `${r.id}. ${r.text}`;
    if (r.marker === " ") buckets.todo.push(entry);
    else if (r.marker === ">") buckets.inProgress.push(entry);
    else if (r.marker === "~") buckets.blocked.push(entry);
    else if (r.marker === "x") buckets.done.push(entry);
  }
  return buckets;
}

function parseTodos(mdText) {
  return bucketTodos(parseTodoRecords(mdText));
}

// What was added and closed on `today`, plus how many todos are still open.
function summarizeDay(records, today) {
  const label = (r) => `${r.id}. ${r.text}`;
  return {
    added:  records.filter((r) => r.created === today).map(label),
    closed: records.filter((r) => r.marker === "x" && r.completed === today).map(label),
    open:   records.filter((r) => r.marker !== "x").length,
  };
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

// Non-legacy endpoints get the generic fields for custom flows, plus the same
// message/attachments shape the legacy webhook uses, so the default Teams
// webhook flow can post the card without any edits.
function buildWorkflowBody(projectName, rpm, buckets, card) {
  return {
    type: "message",
    attachments: [
      {
        contentType: "application/vnd.microsoft.card.adaptive",
        content: card,
      },
    ],
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

// ── daily summary ─────────────────────────────────────────────────────────────

function listSection(title, items, color) {
  const cells = [textBlock(title, { weight: "Bolder", color: color || "Default" })];
  if (items.length === 0) {
    cells.push(textBlock("—", { isSubtle: true, size: "Small" }));
  } else {
    for (const item of items) cells.push(textBlock("• " + item, { size: "Small" }));
  }
  return cells;
}

function buildSummaryCard(projectName, rpm, today, day) {
  return {
    $schema: "http://adaptivecards.io/schemas/adaptive-card.json",
    type: "AdaptiveCard",
    version: "1.4",
    body: [
      textBlock(`Daily summary: ${projectName} (RPM ${rpm}), ${today}`, { weight: "Bolder", size: "Medium" }),
      ...listSection(`Added today (${day.added.length})`, day.added, "Default"),
      ...listSection(`Closed today (${day.closed.length})`, day.closed, "Good"),
      textBlock(`Open todos: ${day.open}`, { isSubtle: true, size: "Small" }),
    ],
  };
}

function renderDaySummary(projectName, rpm, today, day) {
  const list = (items) => (items.length === 0 ? ["- —"] : items.map((i) => `- ${i}`));
  return [
    `Daily summary: ${projectName} (RPM ${rpm}), ${today}`,
    "",
    `Added today (${day.added.length}):`,
    ...list(day.added),
    "",
    `Closed today (${day.closed.length}):`,
    ...list(day.closed),
    "",
    `Open todos: ${day.open}`,
  ].join("\n");
}

// Same message/attachments shape as the Kanban post, so the default flow posts it
// unchanged. Non-legacy endpoints also get the generic fields, with kind "summary".
function buildSummaryBody(syncUrl, projectName, rpm, today, records) {
  const day = summarizeDay(records, today);
  const card = buildSummaryCard(projectName, rpm, today, day);
  const message = buildLegacyWebhookBody(card);
  if (isLegacyTeamsWebhook(syncUrl)) return message;
  return {
    ...message,
    source: "projot",
    version: 1,
    kind: "summary",
    date: today,
    projectName,
    rpm,
    summary: renderDaySummary(projectName, rpm, today, day),
    card,
    added: day.added,
    closed: day.closed,
    openCount: day.open,
  };
}

// ── status page ───────────────────────────────────────────────────────────────

const STATUS_WINDOW_DAYS = 14;
const ISO_DATE_RE = /^\d{4}-\d{2}-\d{2}$/;

// Link entries from the rendered notes file: "- Label: url" under "## Links" (skipping
// unset "N/A" ones), and "- url" or "- [name](url)" in the other link sections.
function parseLinks(mdText) {
  const links = [];
  let section = "";
  for (const line of mdText.split("\n")) {
    const heading = line.match(/^#{2,3} (.+)$/);
    if (heading) { section = heading[1].trim(); continue; }
    if (section === "" || section === "Todos") continue;
    const item = line.match(/^- (.+)$/);
    if (!item) continue;
    const text = item[1].trim();
    const md = text.match(/^\[(.*)\]\((.+)\)$/);
    if (md) { links.push({ section, label: md[1], url: md[2] }); continue; }
    if (section === "Links") {
      const kv = text.match(/^(.+?): (\S.*)$/);
      if (kv && kv[2] !== "N/A") links.push({ section, label: kv[1], url: kv[2] });
      continue;
    }
    if (/^https?:\/\//.test(text)) links.push({ section, label: text, url: text });
  }
  return links;
}

// First day of a window of `days` days ending on `today` (inclusive), as ISO.
function windowStart(today, days) {
  const [y, m, d] = today.split("-").map(Number);
  const start = new Date(Date.UTC(y, m - 1, d - (days - 1)));
  return start.toISOString().slice(0, 10);
}

function inWindow(date, start, today) {
  return ISO_DATE_RE.test(date) && date >= start && date <= today;
}

// Overall state of the project: every todo by status, how much finished recently, and
// progress. Derived only from the notes file, so history reaches back as far as its
// Created/Completed dates do.
function summarizeStatus(records, today, days) {
  const start = windowStart(today, days);
  const entry = (r) => ({ id: r.id, text: r.text, created: r.created, completed: r.completed, notes: r.notes });
  const group = (marker) => records.filter((r) => r.marker === marker).map(entry);
  const todos = { todo: group(" "), inProgress: group(">"), blocked: group("~"), done: group("x") };
  const recentlyCompleted = todos.done
    .filter((r) => inWindow(r.completed, start, today))
    .sort((a, b) => b.completed.localeCompare(a.completed) || Number(b.id) - Number(a.id));
  const addedInWindow = records.filter((r) => inWindow(r.created, start, today)).length;
  const total = records.length;
  return {
    todos,
    recentlyCompleted,
    counts: {
      todo: todos.todo.length,
      inProgress: todos.inProgress.length,
      blocked: todos.blocked.length,
      done: todos.done.length,
      total,
    },
    percentDone: total === 0 ? 0 : Math.round((todos.done.length / total) * 100),
    windowDays: days,
    addedInWindow,
    completedInWindow: recentlyCompleted.length,
  };
}

function renderStatusMarkdown(projectName, rpm, today, status, links) {
  const c = status.counts;
  const lines = [
    `# ${projectName} (RPM ${rpm})`,
    "",
    `Updated ${today}. ${status.percentDone}% done (${c.done} of ${c.total} todos).`,
    "",
    `Open: ${c.todo} todo, ${c.inProgress} in progress, ${c.blocked} blocked.`,
    `Last ${status.windowDays} days: ${status.addedInWindow} added, ${status.completedInWindow} completed.`,
    "",
  ];
  const section = (title, items, line) => {
    lines.push(`## ${title} (${items.length})`, "");
    if (items.length === 0) lines.push("- —");
    for (const item of items) {
      lines.push(line(item));
      for (const note of item.notes || []) lines.push(`  - ${note}`);
    }
    lines.push("");
  };
  const plain = (r) => `- ${r.id}. ${r.text}`;
  section("In progress", status.todos.inProgress, plain);
  section("Blocked", status.todos.blocked, plain);
  section("Todo", status.todos.todo, plain);
  section(`Completed in the last ${status.windowDays} days`, status.recentlyCompleted,
    (r) => `- ${r.id}. ${r.text} (${r.completed})`);
  if (links.length > 0) {
    lines.push("## Links", "");
    for (const l of links) lines.push(`- ${l.section}: [${l.label}](${l.url})`);
    lines.push("");
  }
  return lines.join("\n").trim();
}

// The page endpoint's flow does the create-or-update, so the body is generic JSON
// rather than a card: `key` is stable for the project (find the page by it), and
// `markdown` is a ready-to-write body. `kind: "status"` tells it apart from the posts.
function buildStatusBody(projectName, rpm, today, records, links, days = STATUS_WINDOW_DAYS) {
  const status = summarizeStatus(records, today, days);
  return {
    source: "projot",
    version: 1,
    kind: "status",
    key: `projot-${rpm}`,
    title: `${projectName} (RPM ${rpm}): project status`,
    date: today,
    projectName,
    rpm,
    markdown: renderStatusMarkdown(projectName, rpm, today, status, links),
    counts: status.counts,
    percentDone: status.percentDone,
    windowDays: status.windowDays,
    addedInWindow: status.addedInWindow,
    completedInWindow: status.completedInWindow,
    todos: status.todos,
    recentlyCompleted: status.recentlyCompleted,
    links,
  };
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

function localIsoDate() {
  const d = new Date();
  const pad = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
}

// ── main ──────────────────────────────────────────────────────────────────────

async function main(argv = process.argv.slice(2)) {
  // The sync URL is normally read from the config, since command-line arguments are
  // visible to other local users; an explicit third argument is still accepted.
  // Flags (--summary, --today=DATE) come from projot itself and are kept apart from
  // the positional arguments, so they cannot be taken for the sync URL.
  const flags = argv.filter((a) => a.startsWith("--"));
  const [configPath, notesPath, urlArg] = argv.filter((a) => !a.startsWith("--"));
  if (!configPath || !notesPath) {
    process.stderr.write("teams-sync: usage: node teams-sync.js <config_path> <notes_path> [sync_url] [--summary] [--today=YYYY-MM-DD]\n");
    return;
  }
  const summaryMode = flags.includes("--summary");
  const statusMode = flags.includes("--status");
  const todayFlag = flags.find((f) => f.startsWith("--today="));
  const today = todayFlag ? todayFlag.slice("--today=".length) : localIsoDate();

  let configText, notesText;
  try {
    configText = fs.readFileSync(configPath, "utf8");
    notesText  = fs.readFileSync(notesPath,  "utf8");
  } catch (e) {
    process.stderr.write(`teams-sync: warning: cannot read files: ${e.message}\n`);
    return;
  }

  // The status page has its own endpoint; it never falls back to the message one,
  // since that flow would post a new message on every render.
  const webhookUrl = statusMode
    ? urlArg || readConfigValue(configText, "teams_page_url")
    : urlArg
      || readConfigValue(configText, "teams_sync_url")
      || readConfigValue(configText, "teams_webhook");
  if (!webhookUrl) {
    process.stderr.write(`teams-sync: warning: no ${statusMode ? "teams_page_url" : "teams_sync_url"} in config; skipping\n`);
    return;
  }

  const projectName = readConfigValue(configText, "name") || "Unknown project";
  const rpm         = readConfigValue(configText, "rpm")  || "?";
  let body;
  if (statusMode) {
    body = buildStatusBody(projectName, rpm, today, parseTodoRecords(notesText), parseLinks(notesText));
  } else if (summaryMode) {
    body = buildSummaryBody(webhookUrl, projectName, rpm, today, parseTodoRecords(notesText));
  } else {
    body = buildSyncBody(webhookUrl, projectName, rpm, parseTodos(notesText));
  }

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
  buildSummaryBody,
  buildSummaryCard,
  buildSyncBody,
  buildStatusBody,
  buildWorkflowBody,
  isLegacyTeamsWebhook,
  parseLinks,
  parseTodoRecords,
  parseTodos,
  renderDaySummary,
  renderSummary,
  summarizeDay,
};
