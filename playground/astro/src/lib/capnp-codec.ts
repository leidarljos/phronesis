/**
 * Cap'n Proto flat-message encoder for playground methods.
 * Wire layout matches c-capnproto (capn_write_mem, unpacked stream).
 * Port of playground/scripts/capnp-encode.mjs for browser use.
 */

const ESIZE_BYTE = 2;
const ESIZE_POINTER = 6;

function assertNonNegInt(n: number, label: string): void {
  if (!Number.isInteger(n) || n < 0) throw new Error(`${label} must be non-neg int`);
}

class CapnArena {
  private _words: BigUint64Array | null = null;
  length = 0;
  capacity = 0;

  alloc(n: number): number {
    assertNonNegInt(n, "alloc");
    if (this.length + n > this.capacity) {
      let cap = this.capacity || 16;
      while (cap < this.length + n) cap *= 2;
      const next = new BigUint64Array(cap);
      if (this._words) next.set(this._words.subarray(0, this.length));
      this._words = next;
      this.capacity = cap;
    }
    const off = this.length;
    this.length += n;
    return off;
  }

  setWord(i: number, v: bigint): void {
    if (!this._words) throw new Error("arena empty");
    this._words[i] = v;
  }

  getWord(i: number): bigint {
    if (!this._words) throw new Error("arena empty");
    return this._words[i]!;
  }

  writeStructPtr(at: number, dataOff: number, dataWords: number, ptrWords: number): void {
    const offset = dataOff - (at + 1);
    if (offset < -(1 << 29) || offset >= 1 << 29) {
      throw new Error(`struct offset out of range: ${offset}`);
    }
    const lo = (offset << 2) >>> 0;
    const hi = ((dataWords & 0xffff) | ((ptrWords & 0xffff) << 16)) >>> 0;
    this.setWord(at, BigInt(lo) | (BigInt(hi) << 32n));
  }

  writeListPtr(
    at: number,
    contentOff: number,
    elementSize: number,
    elementCount: number,
  ): void {
    const offset = contentOff - (at + 1);
    if (offset < -(1 << 29) || offset >= 1 << 29) {
      throw new Error(`list offset out of range: ${offset}`);
    }
    const lo = ((offset << 2) | 1) >>> 0;
    const hi = ((elementSize & 7) | ((elementCount & 0x1fffffff) << 3)) >>> 0;
    this.setWord(at, BigInt(lo) | (BigInt(hi) << 32n));
  }

  allocText(s: string): { contentOff: number; byteCount: number } {
    const enc = new TextEncoder().encode(s ?? "");
    const byteCount = enc.length + 1;
    const words = Math.ceil(byteCount / 8);
    const contentOff = this.alloc(words);
    const bytes = new Uint8Array(this._words!.buffer, contentOff * 8, words * 8);
    bytes.set(enc);
    bytes[enc.length] = 0;
    return { contentOff, byteCount };
  }

  writeTextPtr(at: number, s: string): void {
    const { contentOff, byteCount } = this.allocText(s);
    this.writeListPtr(at, contentOff, ESIZE_BYTE, byteCount);
  }

  allocAgentId(hi: number | bigint, lo: number | bigint): number {
    const off = this.alloc(2);
    this.setWord(off, BigInt(hi));
    this.setWord(off + 1, BigInt(lo));
    return off;
  }

  toUint8Array(): Uint8Array {
    const nWords = this.length;
    const out = new Uint8Array(8 + nWords * 8);
    const view = new DataView(out.buffer);
    view.setUint32(0, 0, true);
    view.setUint32(4, nWords, true);
    for (let i = 0; i < nWords; i++) {
      view.setBigUint64(8 + i * 8, this.getWord(i), true);
    }
    return out;
  }
}

function beginRoot(dataWords: number, ptrWords: number) {
  const a = new CapnArena();
  const rootPtr = a.alloc(1);
  const dataOff = a.alloc(dataWords + ptrWords);
  a.writeStructPtr(rootPtr, dataOff, dataWords, ptrWords);
  return {
    arena: a,
    dataOff,
    ptrOff: dataOff + dataWords,
  };
}

export type AgentId = { hi?: number; lo?: number };

export function encodeShellCheck(
  agent: AgentId | undefined,
  cwd: string,
  argv: string[],
): Uint8Array {
  const hi = agent?.hi ?? 1;
  const lo = agent?.lo ?? 2;
  const args = Array.isArray(argv) ? argv : [];
  const { arena, ptrOff } = beginRoot(0, 3);
  const idOff = arena.allocAgentId(hi, lo);
  arena.writeStructPtr(ptrOff + 0, idOff, 2, 0);
  const listOff = arena.alloc(args.length);
  arena.writeListPtr(ptrOff + 2, listOff, ESIZE_POINTER, args.length);
  for (let i = 0; i < args.length; i++) {
    arena.writeTextPtr(listOff + i, args[i] ?? "");
  }
  arena.writeTextPtr(ptrOff + 1, cwd ?? "");
  return arena.toUint8Array();
}

export function encodePathCheck(
  agent: AgentId | undefined,
  action: number,
  path: string,
): Uint8Array {
  const hi = agent?.hi ?? 1;
  const lo = agent?.lo ?? 2;
  const { arena, dataOff, ptrOff } = beginRoot(1, 2);
  arena.setWord(dataOff, BigInt(action & 0xffff));
  const idOff = arena.allocAgentId(hi, lo);
  arena.writeStructPtr(ptrOff + 0, idOff, 2, 0);
  arena.writeTextPtr(ptrOff + 1, path ?? "");
  return arena.toUint8Array();
}

export function encodeSeatCheck(agent: AgentId | undefined, action: number): Uint8Array {
  const hi = agent?.hi ?? 1;
  const lo = agent?.lo ?? 2;
  const { arena, dataOff, ptrOff } = beginRoot(1, 1);
  arena.setWord(dataOff, BigInt(action & 0xffff));
  const idOff = arena.allocAgentId(hi, lo);
  arena.writeStructPtr(ptrOff + 0, idOff, 2, 0);
  return arena.toUint8Array();
}

export function encodeRiskCheck(
  agent: AgentId | undefined,
  action: number,
  path = "",
): Uint8Array {
  const hi = agent?.hi ?? 1;
  const lo = agent?.lo ?? 2;
  const { arena, dataOff, ptrOff } = beginRoot(1, 2);
  arena.setWord(dataOff, BigInt(action & 0xffff));
  const idOff = arena.allocAgentId(hi, lo);
  arena.writeStructPtr(ptrOff + 0, idOff, 2, 0);
  arena.writeTextPtr(ptrOff + 1, path ?? "");
  return arena.toUint8Array();
}

export function encodeReloadShellPack(path: string): Uint8Array {
  const { arena, ptrOff } = beginRoot(0, 1);
  arena.writeTextPtr(ptrOff + 0, path ?? "");
  return arena.toUint8Array();
}

export const PATH_ACTIONS: Record<string, number> = {
  read: 0,
  write: 1,
  delete: 2,
};

export const SEAT_ACTIONS: Record<string, number> = {
  publishRun: 0,
  readRun: 1,
  listRuns: 2,
  listEvents: 3,
};

export const RISK_ACTIONS: Record<string, number> = {
  network: 0,
  secretExport: 1,
  sudo: 2,
  pay: 3,
  auth: 4,
  osChange: 5,
  privilege: 6,
};

function actionOrdinal(
  action: number | string | undefined,
  kind: "path" | "seat" | "risk",
): number {
  if (typeof action === "number" && Number.isInteger(action)) return action;
  const map =
    kind === "path" ? PATH_ACTIONS : kind === "seat" ? SEAT_ACTIONS : RISK_ACTIONS;
  if (typeof action === "string" && action in map) return map[action]!;
  return 0;
}

export type MethodName =
  | "checkShell"
  | "checkPath"
  | "checkSeat"
  | "checkRisk"
  | "checkModel"
  | "admitModel"
  | "reloadShellPack";

export interface EncodeRequest {
  method: MethodName | string;
  agentId?: AgentId;
  cwd?: string;
  argv?: string[];
  path?: string;
  action?: number | string;
  model?: string;
}

export function encodeFixtureRequest(req: EncodeRequest): Uint8Array {
  const method = req.method;
  const agent = req.agentId ?? { hi: 1, lo: 2 };
  switch (method) {
    case "checkShell":
      return encodeShellCheck(agent, req.cwd ?? "/ws", req.argv ?? []);
    case "checkPath":
      return encodePathCheck(agent, actionOrdinal(req.action, "path"), req.path ?? "");
    case "checkSeat":
      return encodeSeatCheck(agent, actionOrdinal(req.action, "seat"));
    case "checkRisk":
      return encodeRiskCheck(agent, actionOrdinal(req.action, "risk"), req.path ?? "");
    case "reloadShellPack":
      return encodeReloadShellPack(req.path ?? "");
    case "checkModel":
    case "admitModel":
      throw new Error(`${method} is a stub in the playground (no Cap'n encode yet)`);
    default:
      throw new Error(`unsupported method: ${method}`);
  }
}
