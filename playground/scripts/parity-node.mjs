#!/usr/bin/env node
/**
 * Fixture catalog parity: load playground WASM, run every JSON fixture under
 * playground/fixtures/{shell,path,seat,risk}/, compare Decision + PolicyReason.
 *
 * Exit 0: "parity OK N fixtures"
 * Exit 1: any mismatch, missing wasm, or encode/check failure
 *
 * Requires playground/dist-wasm built with TRACE (playground/wasm/build.sh).
 * Cap'n request bytes: JS encode (capnp-encode.mjs) or optional golden `bin`.
 */
import { readdirSync, readFileSync, existsSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { encodeFixtureRequest } from "./capnp-encode.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const fixturesRoot = join(root, "playground", "fixtures");
const dist = join(root, "playground", "dist-wasm");
const DOMAINS = ["shell", "path", "seat", "risk"];

const METHOD_EXPORT = {
  checkShell: "pd_check_shell",
  checkPath: "pd_check_path",
  checkSeat: "pd_check_seat",
  checkRisk: "pd_check_risk",
};

function fail(msg) {
  console.error("FAIL:", msg);
  process.exit(1);
}

function listFixtures() {
  const out = [];
  for (const domain of DOMAINS) {
    const dir = join(fixturesRoot, domain);
    if (!existsSync(dir)) continue;
    for (const name of readdirSync(dir).sort()) {
      if (!name.endsWith(".json")) continue;
      out.push(join(dir, name));
    }
  }
  return out;
}

function loadFixture(path) {
  let raw;
  try {
    raw = JSON.parse(readFileSync(path, "utf8"));
  } catch (e) {
    fail(`parse ${path}: ${e}`);
  }
  if (!raw || typeof raw !== "object") fail(`fixture not object: ${path}`);
  if (!raw.id || !raw.method || !raw.expect) {
    fail(`fixture missing id/method/expect: ${path}`);
  }
  if (
    typeof raw.expect.decision !== "number" ||
    typeof raw.expect.code !== "number"
  ) {
    fail(`fixture expect.decision/code must be numbers: ${path}`);
  }
  if (!METHOD_EXPORT[raw.method]) {
    fail(`unsupported method ${raw.method} in ${path}`);
  }
  return raw;
}

function encodeRequest(fx, fixturePath) {
  if (fx.bin) {
    const binPath = join(dirname(fixturePath), fx.bin);
    if (!existsSync(binPath)) fail(`golden bin missing: ${binPath}`);
    return readFileSync(binPath);
  }
  try {
    return encodeFixtureRequest(fx);
  } catch (e) {
    fail(`encode ${fx.id}: ${e}`);
  }
}

function seedMemfs(Module, memfs) {
  if (!memfs || typeof memfs !== "object") return;
  for (const [path, body] of Object.entries(memfs)) {
    if (!path.startsWith("/")) fail(`memfs path must be absolute: ${path}`);
    const parts = path.split("/").filter(Boolean);
    let cur = "";
    for (let i = 0; i < parts.length - 1; i++) {
      cur += "/" + parts[i];
      try {
        Module.FS.mkdir(cur);
      } catch {
        /* exists */
      }
    }
    Module.FS.writeFile(path, body);
  }
}

function callCheck(Module, fnName, sup, inBytes) {
  const check = Module.cwrap(fnName, "number", [
    "number",
    "number",
    "number",
    "number",
  ]);
  const inPtr = Module._malloc(inBytes.length);
  Module.HEAPU8.set(inBytes, inPtr);
  const outHolder = Module._malloc(4);
  Module.setValue(outHolder, 0, "i32");
  const outLen = check(sup, inPtr, inBytes.length, outHolder);
  Module._free(inPtr);
  if (outLen < 0) {
    Module._free(outHolder);
    return { error: `${fnName} returned ${outLen}` };
  }
  const outPtr = Module.getValue(outHolder, "i32");
  Module._free(outHolder);
  if (!outPtr) return { error: `${fnName} out pointer null` };
  return { outPtr, outLen };
}

function readDecision(Module, outPtr, outLen) {
  const read = Module.cwrap("pd_read_decision", "number", [
    "number",
    "number",
    "number",
    "number",
  ]);
  const pdFree = Module.cwrap("pd_free", null, ["number"]);
  const decPtr = Module._malloc(4);
  const codePtr = Module._malloc(4);
  const rc = read(outPtr, outLen, decPtr, codePtr);
  const decision = Module.getValue(decPtr, "i32");
  const code = Module.getValue(codePtr, "i32");
  Module._free(decPtr);
  Module._free(codePtr);
  pdFree(outPtr);
  if (rc !== 0) return { error: `pd_read_decision rc=${rc}` };
  return { decision, code };
}

const jsPath = join(dist, "policyd-playground.js");
if (!existsSync(jsPath)) {
  fail(
    `missing ${jsPath}\n` +
      "  Build on rg.terra: bash playground/wasm/build.sh\n" +
      "  (requires emcc + host capnpc-c; see playground/README.md)",
  );
}

let createModule;
try {
  const mod = await import(pathToFileURL(jsPath).href);
  createModule = mod.default ?? mod.PolicydPlayground ?? mod;
} catch (e) {
  fail(`import ${jsPath}: ${e}`);
}
if (typeof createModule !== "function") {
  fail("WASM module did not export MODULARIZE factory");
}

const Module = await createModule({
  locateFile(path) {
    return join(dist, path);
  },
});

const open = Module.cwrap("pd_supervisor_open", "number", ["string", "string"]);
const close = Module.cwrap("pd_supervisor_close", null, ["number"]);
const clearTrace = Module.cwrap("pd_clear_trace", null, []);

const sup = open("/pd-state", "/pd-runtime");
if (!sup) fail("pd_supervisor_open returned null");

const paths = listFixtures();
if (paths.length === 0) {
  close(sup);
  fail(`no fixtures under ${fixturesRoot}/{${DOMAINS.join(",")}}`);
}

function hasExport(name) {
  const cand =
    Module["_" + name] ||
    Module.asm?.[name] ||
    Module.wasmExports?.[name] ||
    Module.asm?.["_" + name];
  return typeof cand === "function";
}

let failed = 0;
let passed = 0;
const missingExports = new Set();

for (const fpath of paths) {
  const fx = loadFixture(fpath);
  const rel = relative(root, fpath);
  const exportName = METHOD_EXPORT[fx.method];

  if (!hasExport(exportName)) {
    missingExports.add(exportName);
    console.error(
      `FAIL ${rel}: export ${exportName} not in WASM (rebuild: playground/wasm/build.sh on rg.terra)`,
    );
    failed++;
    continue;
  }

  seedMemfs(Module, fx.memfs);
  clearTrace();

  let inBytes;
  try {
    inBytes = encodeRequest(fx, fpath);
  } catch (e) {
    console.error(`FAIL ${rel}: encode ${e}`);
    failed++;
    continue;
  }

  let checked;
  try {
    checked = callCheck(Module, exportName, sup, inBytes);
  } catch (e) {
    console.error(`FAIL ${rel}: ${exportName} threw ${e}`);
    failed++;
    continue;
  }
  if (checked.error) {
    console.error(`FAIL ${rel}: ${checked.error}`);
    failed++;
    continue;
  }
  const got = readDecision(Module, checked.outPtr, checked.outLen);
  if (got.error) {
    console.error(`FAIL ${rel}: ${got.error}`);
    failed++;
    continue;
  }

  const wantD = fx.expect.decision;
  const wantC = fx.expect.code;
  if (got.decision !== wantD || got.code !== wantC) {
    console.error(
      `FAIL ${rel} (${fx.id}): decision=${got.decision} code=${got.code} ` +
        `(want decision=${wantD} code=${wantC})`,
    );
    failed++;
    continue;
  }
  console.log(`OK ${rel} decision=${got.decision} code=${got.code}`);
  passed++;
}

close(sup);

if (failed > 0) {
  if (missingExports.size) {
    console.error(
      `missing WASM exports: ${[...missingExports].join(", ")} — run playground/wasm/build.sh on rg.terra`,
    );
  }
  console.error(
    `parity FAILED: ${passed} ok, ${failed} failed (${paths.length} fixtures)`,
  );
  process.exit(1);
}

console.log(`parity OK ${passed} fixtures`);
process.exit(0);
