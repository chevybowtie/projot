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
  "itrack = 67890",
  "link.itrack = https://itrack.example.com/67890",
].join("\n") + "\n");

// Mock executables: log "name: <args>" to logFile then exit 0.
// projot, git, and the platform open command are all mocked.
const openCmd = { darwin: "open", linux: "xdg-open", win32: null }[platform()];
for (const name of ["projot", "git", ...(openCmd ? [openCmd] : [])]) {
  const script = join(binDir, name);
  writeFileSync(script, `#!/bin/sh\necho '${name}:' "$@" >> '${logFile}'\nexit 0\n`);
  spawnSync("chmod", ["+x", script]);
}

const testEnv = { ...process.env, PATH: binDir + ":" + process.env.PATH };

// ── Harness ────────────────────────────────────────────────────────────────

function runTool(toolName, toolArgs) {
  writeFileSync(logFile, "");

  const input = [
    JSON.stringify({ jsonrpc: "2.0", id: 1, method: "initialize", params: {} }),
    JSON.stringify({ jsonrpc: "2.0", id: 2, method: "tools/call",
                     params: { name: toolName, arguments: toolArgs } }),
  ].join("\n") + "\n";

  const result = spawnSync("node", [SERVER], {
    input,
    cwd: tmp,
    env: testEnv,
    encoding: "utf8",
    timeout: 8000,
  });

  const log = existsSync(logFile) ? readFileSync(logFile, "utf8") : "";
  // Each mock line is "<command>: <args>"; split into per-command arrays.
  const projotCalls = log.split("\n").filter(l => l.startsWith("projot:"));
  const allLines    = log.split("\n").filter(Boolean);

  return { projotCalls, allLines };
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
  const { projotCalls } = runTool("add_todo", { text: "validate the plan" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is add-todo", cmd.includes("add-todo"));
  assert("--text flag absent (removed from CLI in F007)", !cmd.includes("--text"));
  assert("text present in call", cmd.includes("validate the plan"));
});

// F021: add_note_to_todo must use positional arg — --text was removed from the CLI.
test("add_note_to_todo: positional arg, no --text flag", (assert) => {
  const { projotCalls } = runTool("add_note_to_todo", { todo_id: 1, text: "waiting on feedback" });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is add-note", cmd.includes("add-note"));
  assert("--text flag absent (removed from CLI in F007)", !cmd.includes("--text"));
  assert("--todo flag present", cmd.includes("--todo"));
  assert("text present in call", cmd.includes("waiting on feedback"));
});

// F021: setup_new_project must use --rpm, not --ranp (renamed flag).
test("setup_new_project: --rpm not --ranp", (assert) => {
  const { projotCalls } = runTool("setup_new_project", {
    project_number: "99999",
    description: "test setup",
    itrack_number: "11111",
  });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is new", cmd.includes("new"));
  assert("--rpm used (not --ranp)", cmd.includes("--rpm") && !cmd.includes("--ranp"));
});

// Regression guard: complete_todo must pass the todo ID via --todo.
test("complete_todo: --todo flag (regression guard)", (assert) => {
  const { projotCalls } = runTool("complete_todo", { todo_id: 3 });
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is complete", cmd.includes("complete"));
  assert("--todo flag present", cmd.includes("--todo"));
  assert("todo ID present", cmd.includes("3"));
});

// Regression guard: get_open_todos must call list --open.
test("get_open_todos: list --open (regression guard)", (assert) => {
  const { projotCalls } = runTool("get_open_todos", {});
  assert("projot called", projotCalls.length > 0);
  const cmd = projotCalls[0];
  assert("subcommand is list", cmd.includes("list"));
  assert("--open flag present", cmd.includes("--open"));
});

// F023 regression: openUrl must pass URL as argument, not interpolate into a shell string.
// Uses the link.itrack value from .projot/config ("https://itrack.example.com/67890").
if (openCmd) {
  test(`open_itrack: URL passed as argument to ${openCmd} (F023 regression)`, (assert) => {
    const { allLines } = runTool("open_itrack", {});
    const openLine = allLines.find(l => l.startsWith(openCmd + ":"));
    assert(`${openCmd} was called`, !!openLine);
    assert("URL present in args", openLine && openLine.includes("https://itrack.example.com/67890"));
    // If the URL were shell-interpolated via execSync(`xdg-open "${url}"`),
    // the surrounding quotes would appear in the log. With execFileSync they don't.
    assert("URL not wrapped in quotes (no shell interpolation)", openLine && !openLine.includes('"https://'));
  });
}

// ── Summary ────────────────────────────────────────────────────────────────

const total = pass + fail;
console.log(`\n${total} tests: ${pass} passed, ${fail} failed\n`);
process.exit(fail > 0 ? 1 : 0);
