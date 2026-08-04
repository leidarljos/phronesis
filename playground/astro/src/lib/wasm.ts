/**
 * Lazy WASM loader + Cap'n check helpers for the playground island.
 * Mirrors playground/scripts/parity-node.mjs cwrap usage.
 */

import {
  encodeFixtureRequest,
  type EncodeRequest,
  type MethodName,
} from "./capnp-codec";

export type EmscriptenModule = {
  cwrap: (
    ident: string,
    returnType: string | null,
    argTypes: string[],
  ) => (...args: unknown[]) => number | void;
  _malloc: (n: number) => number;
  _free: (p: number) => void;
  HEAPU8: Uint8Array;
  setValue: (ptr: number, value: number, type: string) => void;
  getValue: (ptr: number, type: string) => number;
  UTF8ToString: (ptr: number) => string;
  FS: {
    mkdir: (path: string) => void;
    writeFile: (path: string, data: string | Uint8Array) => void;
    readFile: (path: string, opts?: { encoding?: string }) => string | Uint8Array;
    readdir: (path: string) => string[];
    stat: (path: string) => { mode: number; size: number };
    isDir: (mode: number) => boolean;
    unlink: (path: string) => void;
    analyzePath: (
      path: string,
    ) => { exists: boolean; object?: { mode: number } };
  };
  locateFile?: (path: string) => string;
};

export type TraceSpan = {
  target?: string;
  index?: number;
  role?: string;
};

export type TraceEvent = {
  phase?: string;
  name?: string;
  decision?: string | number;
  code?: number;
  shortCircuit?: boolean;
  spans?: TraceSpan[];
  reason?: string;
  [k: string]: unknown;
};

export type CheckResult = {
  decision: number;
  code: number;
  trace: TraceEvent[];
};

const METHOD_EXPORT: Record<string, string> = {
  checkShell: "pd_check_shell",
  checkPath: "pd_check_path",
  checkSeat: "pd_check_seat",
  checkRisk: "pd_check_risk",
  reloadShellPack: "pd_reload_shell_pack",
};

export type LoadState = "idle" | "loading" | "ready" | "error";

let modulePromise: Promise<EmscriptenModule> | null = null;
let supervisor: number | null = null;
let mod: EmscriptenModule | null = null;

function wasmBaseUrl(base = import.meta.env.BASE_URL): string {
  const b = base.endsWith("/") ? base : base + "/";
  return `${b}wasm/`;
}

export function isEvaluatorReady(): boolean {
  return mod !== null && supervisor !== null;
}

export function getModule(): EmscriptenModule | null {
  return mod;
}

export async function loadEvaluator(
  base = import.meta.env.BASE_URL,
): Promise<EmscriptenModule> {
  if (mod && supervisor) return mod;
  if (modulePromise) return modulePromise;

  modulePromise = (async () => {
    const root = wasmBaseUrl(base);
    const jsUrl = `${root}policyd-playground.js`;
    const imported = await import(/* @vite-ignore */ jsUrl);
    const factory =
      imported.default ?? imported.PolicydPlayground ?? imported;
    if (typeof factory !== "function") {
      throw new Error("WASM module did not export MODULARIZE factory");
    }
    const Module = (await factory({
      locateFile(path: string) {
        return root + path;
      },
    })) as EmscriptenModule;

    const open = Module.cwrap("pd_supervisor_open", "number", [
      "string",
      "string",
    ]);
    const sup = open("/pd-state", "/pd-runtime") as number;
    if (!sup) throw new Error("pd_supervisor_open returned null");

    mod = Module;
    supervisor = sup;
    return Module;
  })();

  try {
    return await modulePromise;
  } catch (e) {
    modulePromise = null;
    mod = null;
    supervisor = null;
    throw e;
  }
}

export function hasExport(Module: EmscriptenModule, name: string): boolean {
  const m = Module as unknown as Record<string, unknown>;
  const cand =
    m["_" + name] ||
    (m.asm as Record<string, unknown> | undefined)?.[name] ||
    (m.wasmExports as Record<string, unknown> | undefined)?.[name];
  return typeof cand === "function";
}

export function seedMemfs(
  Module: EmscriptenModule,
  memfs: Record<string, string> | undefined,
): void {
  if (!memfs) return;
  for (const [path, body] of Object.entries(memfs)) {
    if (!path.startsWith("/")) throw new Error(`memfs path must be absolute: ${path}`);
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

function callCheck(
  Module: EmscriptenModule,
  fnName: string,
  sup: number,
  inBytes: Uint8Array,
): { outPtr: number; outLen: number } {
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
  const outLen = check(sup, inPtr, inBytes.length, outHolder) as number;
  Module._free(inPtr);
  if (outLen < 0) {
    Module._free(outHolder);
    throw new Error(`${fnName} returned ${outLen}`);
  }
  const outPtr = Module.getValue(outHolder, "i32");
  Module._free(outHolder);
  if (!outPtr) throw new Error(`${fnName} out pointer null`);
  return { outPtr, outLen };
}

function readDecision(
  Module: EmscriptenModule,
  outPtr: number,
  outLen: number,
): { decision: number; code: number } {
  const read = Module.cwrap("pd_read_decision", "number", [
    "number",
    "number",
    "number",
    "number",
  ]);
  const pdFree = Module.cwrap("pd_free", null, ["number"]);
  const decPtr = Module._malloc(4);
  const codePtr = Module._malloc(4);
  const rc = read(outPtr, outLen, decPtr, codePtr) as number;
  const decision = Module.getValue(decPtr, "i32");
  const code = Module.getValue(codePtr, "i32");
  Module._free(decPtr);
  Module._free(codePtr);
  pdFree(outPtr);
  if (rc !== 0) throw new Error(`pd_read_decision rc=${rc}`);
  return { decision, code };
}

function takeTrace(Module: EmscriptenModule): TraceEvent[] {
  if (!hasExport(Module, "pd_take_trace_json")) return [];
  const take = Module.cwrap("pd_take_trace_json", "number", []);
  const pdFree = Module.cwrap("pd_free", null, ["number"]);
  const jsonPtr = take() as number;
  if (!jsonPtr) return [];
  const jsonStr = Module.UTF8ToString(jsonPtr);
  pdFree(jsonPtr);
  try {
    const events = JSON.parse(jsonStr);
    return Array.isArray(events) ? (events as TraceEvent[]) : [];
  } catch {
    return [];
  }
}

export async function runCheck(
  method: MethodName | string,
  req: EncodeRequest,
  memfs?: Record<string, string>,
): Promise<CheckResult> {
  const Module = await loadEvaluator();
  if (supervisor === null) throw new Error("supervisor not open");

  const exportName = METHOD_EXPORT[method];
  if (!exportName) {
    throw new Error(`method ${method} has no WASM export (stub?)`);
  }
  if (!hasExport(Module, exportName)) {
    throw new Error(
      `export ${exportName} missing — rebuild playground WASM on rg.terra`,
    );
  }

  seedMemfs(Module, memfs);
  if (hasExport(Module, "pd_clear_trace")) {
    Module.cwrap("pd_clear_trace", null, [])();
  }

  const inBytes = encodeFixtureRequest({ ...req, method });
  const { outPtr, outLen } = callCheck(Module, exportName, supervisor, inBytes);
  const { decision, code } = readDecision(Module, outPtr, outLen);
  const trace = takeTrace(Module);
  return { decision, code, trace };
}

/** Path-based pack reload (Author mode). Returns 0 / -1 / -2. */
export async function reloadPackPath(path: string): Promise<number> {
  const Module = await loadEvaluator();
  if (!hasExport(Module, "pd_reload_pack_path")) {
    // Fall back to Cap'n reload if path helper missing
    const r = await runCheck("reloadShellPack", {
      method: "reloadShellPack",
      path,
    });
    if (r.decision === 1 && r.code === 21) return 0;
    if (r.code === 22) return -1;
    return -2;
  }
  const fn = Module.cwrap("pd_reload_pack_path", "number", ["string"]);
  return fn(path) as number;
}

export function listMemfsTree(
  Module: EmscriptenModule,
  root = "/policy",
): { path: string; isDir: boolean; size?: number }[] {
  const out: { path: string; isDir: boolean; size?: number }[] = [];
  function walk(dir: string) {
    let names: string[];
    try {
      names = Module.FS.readdir(dir);
    } catch {
      return;
    }
    for (const name of names) {
      if (name === "." || name === "..") continue;
      const p = dir === "/" ? `/${name}` : `${dir}/${name}`;
      let st: { mode: number; size: number };
      try {
        st = Module.FS.stat(p);
      } catch {
        continue;
      }
      const isDir = Module.FS.isDir(st.mode);
      out.push({ path: p, isDir, size: isDir ? undefined : st.size });
      if (isDir) walk(p);
    }
  }
  walk(root);
  return out;
}

export function readMemfsText(Module: EmscriptenModule, path: string): string {
  const data = Module.FS.readFile(path, { encoding: "utf8" });
  return typeof data === "string" ? data : new TextDecoder().decode(data);
}

export function writeMemfsText(
  Module: EmscriptenModule,
  path: string,
  body: string,
): void {
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
