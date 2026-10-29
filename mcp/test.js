#!/usr/bin/env node
// mcp/test.js — Smoke tests: verify each tool handler calls the CLI with the correct flags.
// Guards against flag-drift bugs (e.g. using --text after it was removed from add-todo/add-note).
//
// Run: node mcp/test.js
// Requires: Node.js 18+. Does not require projot to be installed.

import { spawnSync } from "child_process";
import { mkdtempSync, writeFileSync, readFileSync, mkdirSync, existsSync } from "fs";
import { join } from "path";
import { tmpdir, platform } from "os";
import { fileURLToPath } from "url";
import { buildSyncBody, buildSummaryBody, isLegacyTeamsWebhook, parseTodos, parseTodoRecords, summarizeDay, renderDaySummary } from "./teams-sync.js";

const __dirname = fileURLToPath(new URL(".", import.meta.url));
const SERVER = join(__dirname, "server.js");

// ── Fixtures ───────────────────────────────────────────────────────────────

const tmp = mkdtempSync(join(tmpdir(), "projot-mcp-test-"));
const binDir = join(tmp, "bin");
const logFile = join(tmp, "commands.log");

mkdirSync(binDir);

// Minimal .projot/config so getConfigValue() does not crash.
mkdirSync(join(tmp, ".projot"), { recursive: true });
writeFileSync(join(tmp, ".projot", "config"), [
  "config_version = 1",
  "app_id = TestApp",
  "rpm = 12345",
  "name = Test Project",
  "jira = 67890",
  "link.jira = https://jira.example.com/67890",
  "link.teams = https://teams.example.com/channel",
  "link.rpm = https://rpm.example.com/12345",
  "github = https://github.com/example/repo",
  "swagger = https://swagger.example.com",
  "blizzard = https://blizzard.example.com",
].join("\n") + "\n");

// Mock executables: log "name: <args>" to logFile then exit 0.
// projot, git, and the platform open command are all mocked.
//   git:    answers the HEAD query as branch "main"; every other call succeeds.
//   projot: fails when its subcommand equals $MOCK_PROJOT_FAIL.
const openCmd = { darwin: "open", linux: "xdg-open", win32: null }[platform()];
const mockBodies = {
  git: `[ "$1 $2" = "symbolic-ref --quiet" ] && echo main\nexit 0\n`,
  projot: `[ -n "$MOCK_PROJOT_FAIL" ] && [ "$1" = "$MOCK_PROJOT_FAIL" ] && { echo "error: simulated failure" >&2; exit 1; }\nexit 0\n`,
};
for (const name of ["projot", "git", ...(openCmd ? [openCmd] : [])]) {
  const script = join(binDir, name);
  writeFileSync(script, `#!/bin/sh\necho '${name}:' "$@" >> '${logFile}'\n${mockBodies[name] || "exit 0\n"}`);
  spawnSync("chmod", ["+x", script]);
}

const testEnv = { ...process.env, PATH: binDir + ":" + process.env.PATH };

// ── Harness ────────────────────────────────────────────────────────────────

function runTool(toolName, toolArgs, extraEnv = {}) {
  writeFileSync(logFile, "");

  const input = [
    JSON.stringify({ jsonrpc: "2.0", id: 1, method: "initialize", params: { protocolVersion: "2024-11-05" } }),
    JSON.stringify({ jsonrpc: "2.0", id: 2, method: "tools/call",
                     params: { name: toolName, arguments: toolArgs } }),
  ].join("\n") + "\n";

  const result = spawnSync("node", [SERVER], {
    input,
    cwd: tmp,
    env: { ...testEnv, ...extraEnv },
    encoding: "utf8",
    timeout: 8000,
  });

  const log = existsSync(logFile) ? readFileSync(logFile, "utf8") : "";
  // Each mock line is "<command>: <args>"; split into per-command arrays.
  const projotCalls = log.split("\n").filter(l => l.startsWith("projot:"));
  const gitCalls    = log.split("\n").filter(l => l.startsWith("git:"));
  const allLines    = log.split("\n").filter(Boolean);

  // The tools/call response (id 2), for checking isError and the message.
  let response = null;
  for (const line of (result.stdout || "").split("\n")) {
    try { const msg = JSON.parse(line); if (msg.id === 2) response = msg; } catch { /* not JSON */ }
  }
  const isError = !!(response && response.result && response.result.isError);
  const text = response && response.result ? response.result.content[0].text : "";

  return { projotCalls, gitCalls, allLines, isError, text };
}

let pass = 0, fail = 0;

function test(name, fn) {
  const failures = [];
  fn((label, cond) => { if (!cond) failures.push(label); });
  if (failures.length === 0) {
    console.log(`  pass  ${name}`);
    pass++;
  } else {
    console.error(`  FAIL  ${name}`);
    failures.forEach(f => console.error(`         x ${f}`));
    fail++;
  }
}

// ── Tests ──────────────────────────────────────────────────────────────────

console.log("\nMCP server CLI flag tests\n");

// F021: add_todo must use positional arg — --text was removed from the CLI.
test("add_todo: positional arg, no --text flag", (assert) => {
  const { projotCalls } = runTool("projot_add_todo", { text: "validate the plan" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is add-todo", cmd.includes("add-todo"));
  assert("--text flag absent (removed from CLI in F007)", !cmd.includes("--text"));
  assert("text present in call", cmd.includes("validate the plan"));
});

// F021: add_note_to_todo must use positional arg — --text was removed from the CLI.
test("add_note_to_todo: positional arg, no --text flag", (assert) => {
  const { projotCalls } = runTool("projot_add_note", { todo_id: 1, text: "waiting on feedback" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is add-note", cmd.includes("add-note"));
  assert("--text flag absent (removed from CLI in F007)", !cmd.includes("--text"));
  assert("--todo flag present", cmd.includes("--todo"));
  assert("text present in call", cmd.includes("waiting on feedback"));
});

// F021: setup_new_project must use --rpm, not --ranp (renamed flag).
test("setup_new_project: --rpm not --ranp", (assert) => {
  const { projotCalls } = runTool("projot_setup_project", {
    project_number: "99999",
    description: "test setup",
    jira_number: "11111",
  });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is new", cmd.includes("new"));
  assert("--rpm used (not --ranp)", cmd.includes("--rpm") && !cmd.includes("--ranp"));
  assert("--jira used", cmd.includes("--jira"));
});

test("setup_new_project: legacy itrack_number still accepted", (assert) => {
  const { projotCalls } = runTool("projot_setup_project", {
    project_number: "99999",
    description: "test setup",
    itrack_number: "22222",
  });
  assert("projot called", projotCalls.length > 0);
  assert("--jira 22222 passed", projotCalls[0].includes("--jira") && projotCalls[0].includes("22222"));
});

const setupArgs = { project_number: "99999", description: "test setup", jira_number: "11111" };
const setupBranch = "feat/99999-test-setup";

test("setup_project: success creates the branch and does not roll back", (assert) => {
  const { gitCalls, isError } = runTool("projot_setup_project", setupArgs);
  assert("not an error", !isError);
  assert("branch created", gitCalls.some(l => l.includes(`checkout -b ${setupBranch}`)));
  assert("no branch deletion", !gitCalls.some(l => l.includes("branch -d")));
});

test("setup_project: failed projot new removes the branch and restores HEAD", (assert) => {
  const { gitCalls, isError, text } =
    runTool("projot_setup_project", setupArgs, { MOCK_PROJOT_FAIL: "new" });
  assert("reported as error", isError);
  assert("projot's error surfaced", text.includes("simulated failure"));
  assert("rollback reported", text.includes("Rolled back"));
  const checkout = gitCalls.findIndex(l => l.includes(`checkout -b ${setupBranch}`));
  const restore  = gitCalls.findIndex(l => l.includes("symbolic-ref HEAD refs/heads/main"));
  const remove   = gitCalls.findIndex(l => l.includes(`branch -d ${setupBranch}`));
  assert("HEAD restored after branching", checkout >= 0 && restore > checkout);
  assert("branch deleted after HEAD restored", remove > restore);
});

test("setup_project: missing arguments fail before any git call", (assert) => {
  for (const missing of ["project_number", "description", "jira_number"]) {
    const partial = { ...setupArgs };
    delete partial[missing];
    const { gitCalls, isError, text } = runTool("projot_setup_project", partial);
    assert(`${missing}: reported as error`, isError && text.includes(missing));
    assert(`${missing}: git untouched`, gitCalls.length === 0);
  }
});

// Regression guard: complete_todo must pass the todo ID via --todo.
test("complete_todo: --todo flag (regression guard)", (assert) => {
  const { projotCalls } = runTool("projot_complete_todo", { todo_id: 3 });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is complete", cmd.includes("complete"));
  assert("--todo flag present", cmd.includes("--todo"));
  assert("todo ID present", cmd.includes("3"));
});

// Regression guard: get_open_todos must call list --open.
test("get_open_todos: list --open (regression guard)", (assert) => {
  const { projotCalls } = runTool("projot_list_todos", {});
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is list", cmd.includes("list"));
  assert("--open flag present", cmd.includes("--open"));
});

// F023 regression: openUrl must pass URL as argument, not interpolate into a shell string.
// Uses the link.jira value from .projot/config ("https://jira.example.com/67890").
if (openCmd) {
  test(`open_jira: URL passed as argument to ${openCmd} (F023 regression)`, (assert) => {
    const { allLines } = runTool("projot_open_jira", {});
    const openLine = allLines.find(l => l.startsWith(openCmd + ":"));
    assert(`${openCmd} was called`, !!openLine);
    assert("URL present in args", openLine && openLine.includes("https://jira.example.com/67890"));
    // If the URL were shell-interpolated via execSync(`xdg-open "${url}"`),
    // the surrounding quotes would appear in the log. With execFileSync they don't.
    assert("URL not wrapped in quotes (no shell interpolation)", openLine && !openLine.includes('"https://'));
  });

  test(`open_itrack: legacy tool name still works`, (assert) => {
    const { allLines } = runTool("projot_open_itrack", {});
    const openLine = allLines.find(l => l.startsWith(openCmd + ":"));
    assert(`${openCmd} was called`, !!openLine);
  });
}

// set_*_link handlers: verify correct CLI subcommand and flags.
test("set_teams_link: projot set-link with correct key and url", (assert) => {
  const { projotCalls } = runTool("projot_set_teams_link", { url: "https://teams.new.com/channel" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is set-link", cmd.includes("set-link"));
  assert("--key teams present", cmd.includes("--key") && cmd.includes("teams"));
  assert("--url present", cmd.includes("--url"));
  assert("url in call", cmd.includes("https://teams.new.com/channel"));
});

test("set_github_link: projot add-github called with url", (assert) => {
  const { projotCalls } = runTool("projot_set_github_link", { url: "https://github.com/example/new" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is add-github", cmd.includes("add-github"));
  assert("--url present", cmd.includes("--url"));
  assert("url in call", cmd.includes("https://github.com/example/new"));
});

test("set_swagger_link: projot add-swagger called with url", (assert) => {
  const { projotCalls } = runTool("projot_set_swagger_link", { url: "https://swagger.new.com" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is add-swagger", cmd.includes("add-swagger"));
  assert("--url present", cmd.includes("--url"));
  assert("url in call", cmd.includes("https://swagger.new.com"));
});

test("set_blizzard_link: projot add-blizzard called with url", (assert) => {
  const { projotCalls } = runTool("projot_set_blizzard_link", { url: "https://blizzard.new.com" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is add-blizzard", cmd.includes("add-blizzard"));
  assert("--url present", cmd.includes("--url"));
  assert("url in call", cmd.includes("https://blizzard.new.com"));
});

// open_* handlers: verify URL is passed as an argument (not shell-interpolated).
if (openCmd) {
  test(`open_github: URL passed as argument to ${openCmd}`, (assert) => {
    const { allLines } = runTool("projot_open_github", {});
    const openLine = allLines.find(l => l.startsWith(openCmd + ":"));
    assert(`${openCmd} was called`, !!openLine);
    assert("GitHub URL in args", openLine && openLine.includes("https://github.com/example/repo"));
  });

  test(`open_swagger: URL passed as argument to ${openCmd}`, (assert) => {
    const { allLines } = runTool("projot_open_swagger", {});
    const openLine = allLines.find(l => l.startsWith(openCmd + ":"));
    assert(`${openCmd} was called`, !!openLine);
    assert("Swagger URL in args", openLine && openLine.includes("https://swagger.example.com"));
  });

  test(`open_blizzard: URL passed as argument to ${openCmd}`, (assert) => {
    const { allLines } = runTool("projot_open_blizzard", {});
    const openLine = allLines.find(l => l.startsWith(openCmd + ":"));
    assert(`${openCmd} was called`, !!openLine);
    assert("Blizzard URL in args", openLine && openLine.includes("https://blizzard.example.com"));
  });

  test(`open_teams: URL passed as argument to ${openCmd}`, (assert) => {
    const { allLines } = runTool("projot_open_teams", {});
    const openLine = allLines.find(l => l.startsWith(openCmd + ":"));
    assert(`${openCmd} was called`, !!openLine);
    assert("Teams URL in args", openLine && openLine.includes("https://teams.example.com/channel"));
  });

  test(`open_rpm: URL passed as argument to ${openCmd}`, (assert) => {
    const { allLines } = runTool("projot_open_rpm", {});
    const openLine = allLines.find(l => l.startsWith(openCmd + ":"));
    assert(`${openCmd} was called`, !!openLine);
    assert("RPM URL in args", openLine && openLine.includes("https://rpm.example.com/12345"));
  });
}

test("set_status: projot status called with todo id and status string", (assert) => {
  const { projotCalls } = runTool("projot_set_status", { todo_id: 3, status: "in-progress" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is status", cmd.includes("status"));
  assert("--todo present", cmd.includes("--todo"));
  assert("todo_id in call", cmd.includes("3"));
  assert("status in call", cmd.includes("in-progress"));
});

// Protocol negotiation tests
function runInitialize(protocolVersion) {
  const params = protocolVersion !== undefined ? { protocolVersion } : {};
  const input = JSON.stringify({ jsonrpc: "2.0", id: 1, method: "initialize", params }) + "\n";
  const result = spawnSync("node", [SERVER], {
    input,
    cwd: tmp,
    env: testEnv,
    encoding: "utf8",
    timeout: 8000,
  });
  try {
    return JSON.parse(result.stdout.trim());
  } catch {
    return null;
  }
}

test("initialize: supported version 2025-11-25 is accepted and echoed back", (assert) => {
  const resp = runInitialize("2025-11-25");
  assert("response received", !!resp);
  assert("no error", !resp.error);
  assert("protocolVersion echoed", resp.result && resp.result.protocolVersion === "2025-11-25");
  assert("tools capability declared", resp.result && resp.result.capabilities && "tools" in resp.result.capabilities);
});

test("initialize: supported legacy version 2024-11-05 is accepted and echoed back", (assert) => {
  const resp = runInitialize("2024-11-05");
  assert("response received", !!resp);
  assert("no error", !resp.error);
  assert("protocolVersion echoed", resp.result && resp.result.protocolVersion === "2024-11-05");
  assert("tools capability declared", resp.result && resp.result.capabilities && "tools" in resp.result.capabilities);
});

test("initialize: unsupported version returns error", (assert) => {
  const resp = runInitialize("2099-01-01");
  assert("response received", !!resp);
  assert("error returned", !!resp.error);
});

test("initialize: no protocolVersion defaults to supported version", (assert) => {
  const resp = runInitialize(undefined);
  assert("response received", !!resp);
  assert("no error", !resp.error);
  assert("protocolVersion defaults to latest", resp.result && resp.result.protocolVersion === "2025-11-25");
  assert("tools capability declared", resp.result && resp.result.capabilities && "tools" in resp.result.capabilities);
});

// ── Teams sync endpoint routing (#50) ──────────────────────────────────────

console.log("\nTeams sync endpoint tests\n");

// Power Automate / Workflows URLs carry SAS auth in the query string. They must
// be treated as generic endpoints (workflow body), never as legacy connectors,
// and the body must not add any auth field of its own.
const PA_URL = "https://prod-00.westus.logic.azure.com:443/workflows/abc/triggers/manual/paths/invoke?api-version=2016-06-01&sp=%2Ftriggers%2Fmanual%2Frun&sv=1.0&sig=XYZ";
const LEGACY_URL = "https://outlook.office.com/webhookb2/abc@def/IncomingWebhook/ghi/jkl";
const SAMPLE_TODOS = parseTodos("1. [ ] first\n2. [x] second\n");

test("teams_sync: Power Automate URL is not treated as a legacy webhook", (assert) => {
  assert("not legacy", isLegacyTeamsWebhook(PA_URL) === false);
});

test("teams_sync: legacy Teams webhook URL is still detected", (assert) => {
  assert("legacy", isLegacyTeamsWebhook(LEGACY_URL) === true);
});

test("teams_sync: Power Automate URL gets workflow body with kanban data", (assert) => {
  const body = buildSyncBody(PA_URL, "Demo", "123", SAMPLE_TODOS);
  assert("source is projot", body.source === "projot");
  assert("kanban present", !!body.kanban);
  assert("todo bucket kept", body.kanban && body.kanban.todo.length === 1);
  assert("message type for default flow", body.type === "message");
  assert("attachments present", Array.isArray(body.attachments) && body.attachments.length === 1);
  assert("attachment is adaptive card", body.attachments[0].content.type === "AdaptiveCard");
});

test("teams_sync: legacy URL gets message/attachments body", (assert) => {
  const body = buildSyncBody(LEGACY_URL, "Demo", "123", SAMPLE_TODOS);
  assert("type message", body.type === "message");
  assert("attachments present", Array.isArray(body.attachments));
});

test("teams_sync: request body carries no auth field", (assert) => {
  const json = JSON.stringify(buildSyncBody(PA_URL, "Demo", "123", SAMPLE_TODOS)).toLowerCase();
  assert("no authorization key", !json.includes("authorization"));
  assert("no sig key", !json.includes('"sig"'));
});

// ── Daily summary (summarize --today) ─────────────────────────────────────

console.log("\nTeams daily summary tests\n");

const NOTES = [
  "# Test",
  "",
  "## Todos",
  "",
  "1. [x] shipped today",
  "   - Created: 2026-10-27",
  "   - Completed: 2026-10-28",
  "2. [ ] added today",
  "   - Created: 2026-10-28",
  "   - Notes:",
  "     - Created: 2026-01-01 (a note, not a date)",
  "3. [>] old and open",
  "   - Created: 2026-10-01",
  "4. [x] closed earlier",
  "   - Created: 2026-10-20",
  "   - Completed: 2026-10-21",
].join("\n");

const RECORDS = parseTodoRecords(NOTES);
const DAY = summarizeDay(RECORDS, "2026-10-28");

test("summary: parseTodoRecords reads created and completed dates", (assert) => {
  assert("four todos", RECORDS.length === 4);
  assert("completed date kept", RECORDS[0].completed === "2026-10-28");
  assert("created date kept", RECORDS[2].created === "2026-10-01");
});

test("summary: note lines are not mistaken for dates", (assert) => {
  assert("note does not overwrite created", RECORDS[1].created === "2026-10-28");
});

test("summary: added today lists todos created on the day", (assert) => {
  assert("added count", DAY.added.length === 1);
  assert("added text", DAY.added[0] === "2. added today");
});

test("summary: closed today lists only todos completed on the day", (assert) => {
  assert("closed count", DAY.closed.length === 1);
  assert("closed text", DAY.closed[0] === "1. shipped today");
});

test("summary: open count excludes done todos", (assert) => {
  assert("open count", DAY.open === 2);
});

test("summary: plain text lists the day's activity", (assert) => {
  const text = renderDaySummary("Demo", "123", "2026-10-28", DAY);
  assert("header has date", text.startsWith("Daily summary: Demo (RPM 123), 2026-10-28"));
  assert("added listed", text.includes("- 2. added today"));
  assert("closed listed", text.includes("- 1. shipped today"));
});

test("summary: Power Automate body uses the default-flow shape and kind", (assert) => {
  const url = "https://prod-00.westus.logic.azure.com:443/workflows/abc/triggers/manual/paths/invoke?sig=XYZ";
  const body = buildSummaryBody(url, "Demo", "123", "2026-10-28", RECORDS);
  assert("message type", body.type === "message");
  assert("adaptive card attachment", body.attachments[0].content.type === "AdaptiveCard");
  assert("kind is summary", body.kind === "summary");
  assert("date carried", body.date === "2026-10-28");
  assert("generic summary present", typeof body.summary === "string");
});

test("summary: legacy webhook body is message-only", (assert) => {
  const url = "https://outlook.office.com/webhookb2/abc@def/IncomingWebhook/ghi/jkl";
  const body = buildSummaryBody(url, "Demo", "123", "2026-10-28", RECORDS);
  assert("type message", body.type === "message");
  assert("no generic fields", body.kind === undefined && body.summary === undefined);
});

// ── Summary ────────────────────────────────────────────────────────────────

const total = pass + fail;
console.log(`\n${total} tests: ${pass} passed, ${fail} failed\n`);
process.exit(fail > 0 ? 1 : 0);
