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

process.stdout.write("teams-sync tests passed\n");
