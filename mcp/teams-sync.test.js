#!/usr/bin/env node
"use strict";

import * as sync from "./teams-sync.js";

function fail(message) {
  process.stderr.write(`FAIL: ${message}\n`);
  process.exit(1);
}

function assert(name, condition) {
  if (!condition) fail(name);
}

const buckets = {
  todo: ["1. First todo"],
  inProgress: ["2. Ongoing"],
  blocked: [],
  done: ["3. Finished"],
};

assert(
  "legacy webhook detection",
  sync.isLegacyTeamsWebhook("https://example.webhook.office.com/webhookb2/abc@def/IncomingWebhook/ghi/jkl")
);
assert(
  "workflow endpoint detection",
  !sync.isLegacyTeamsWebhook("https://prod-00.westus.logic.azure.com:443/workflows/abc/triggers/manual/paths/invoke")
);

const legacyBody = sync.buildSyncBody(
  "https://example.webhook.office.com/webhookb2/abc@def/IncomingWebhook/ghi/jkl",
  "Widget",
  "12345",
  buckets
);
assert("legacy payload type", legacyBody.type === "message");
assert(
  "legacy payload attachment content type",
  legacyBody.attachments &&
    legacyBody.attachments[0] &&
    legacyBody.attachments[0].contentType === "application/vnd.microsoft.card.adaptive"
);

const workflowBody = sync.buildSyncBody(
  "https://prod-00.westus.logic.azure.com:443/workflows/abc/triggers/manual/paths/invoke",
  "Widget",
  "12345",
  buckets
);
assert("workflow payload source", workflowBody.source === "projot");
assert("workflow payload version", workflowBody.version === 1);
assert("workflow payload project name", workflowBody.projectName === "Widget");
assert("workflow payload rpm", workflowBody.rpm === "12345");
assert("workflow payload kanban", workflowBody.kanban && workflowBody.kanban.inProgress[0] === "2. Ongoing");
assert(
  "workflow payload summary",
  workflowBody.summary.includes("Kanban: Widget (RPM 12345)") && workflowBody.summary.includes("- 1. First todo")
);
assert("workflow payload card", workflowBody.card && workflowBody.card.type === "AdaptiveCard");

// ── Done column cap ──
const md = [1, 2, 3, 4, 5, 6, 7].map((i) =>
  `${i}. [x] item ${i}\n   - Created: 2026-11-01\n   - Completed: 2026-11-0${i}`
).join("\n") + "\n8. [ ] open\n";
const capped = sync.parseTodos(md);
assert("done capped at 5 by default", capped.done.length === 5);
assert("done keeps newest first", capped.done[0] === "7. item 7" && capped.done[4] === "3. item 3");
assert("doneTotal is uncapped", capped.doneTotal === 7);
assert("limit 0 shows all", sync.parseTodos(md, 0).done.length === 7);
assert("custom limit", sync.parseTodos(md, 2).done.length === 2);
assert("limit above count", sync.parseTodos(md, 50).done.length === 7);
const tie = sync.parseTodos("1. [x] a\n   - Completed: 2026-11-01\n2. [x] b\n   - Completed: 2026-11-01\n", 1);
assert("same-day tie goes to higher id", tie.done[0] === "2. b");
assert("parseDoneLimit valid", sync.parseDoneLimit("10") === 10 && sync.parseDoneLimit("0") === 0);
assert("parseDoneLimit falls back", sync.parseDoneLimit("") === 5 && sync.parseDoneLimit("-1") === 5 && sync.parseDoneLimit("x") === 5);

const cappedBody = sync.buildSyncBody(
  "https://prod-00.westus.logic.azure.com:443/workflows/abc/triggers/manual/paths/invoke",
  "Widget", "12345", capped
);
assert("card ends with total line",
  cappedBody.card.body[cappedBody.card.body.length - 1].text === "7 todos complete (showing the 5 most recent).");
assert("summary states total", cappedBody.summary.endsWith("7 todos complete (showing the 5 most recent)."));
assert("payload carries doneTotal", cappedBody.kanban.doneTotal === 7);
const uncapped = sync.buildAdaptiveCard("W", "1", sync.parseTodos(md, 0));
assert("no 'showing' when nothing hidden", uncapped.body[uncapped.body.length - 1].text === "7 todos complete.");
const one = sync.buildAdaptiveCard("W", "1", sync.parseTodos("1. [x] a\n"));
assert("singular", one.body[one.body.length - 1].text === "1 todo complete.");

// The status page must list every todo, regardless of the Done cap.
const status = sync.buildStatusBody("Widget", "12345", "2026-11-11", sync.parseTodoRecords(md), []);
assert("status page uncapped", status.todos.done.length === 7);

process.stdout.write("teams-sync tests passed\n");
