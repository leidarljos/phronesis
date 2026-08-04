/**
 * Minimal Cap'n Proto flat-message encoder for playground fixtures.
 * Wire layout matches c-capnproto (capn_write_mem, unpacked stream).
 *
 * Supported roots: ShellCheck, PathCheck, SeatCheck, RiskCheck
 * with Util.AgentId {hi, lo} as nested struct.
 */
// Element sizes for Cap'n list pointers
const ESIZE_BYTE = 2;
const ESIZE_POINTER = 6;

/** @param {number} n */
function assertNonNegInt(n, label) {
  if (!Number.isInteger(n) || n < 0) throw new Error(`${label} must be non-neg int`);
}

/**
 * Arena of 64-bit little-endian words.
 */
class CapnArena {
  constructor() {
    /** @type {BigUint64Array | null} */
    this._words = null;
    this.length = 0;
    this.capacity = 0;
  }

  /** @param {number} n */
  alloc(n) {
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

  /** @param {number} i @param {bigint} v */
  setWord(i, v) {
    this._words[i] = v;
  }

  /** @param {number} i */
  getWord(i) {
    return this._words[i];
  }

  /**
   * Struct pointer at `at` → data section at `dataOff`.
   * @param {number} at
   * @param {number} dataOff
   * @param {number} dataWords
   * @param {number} ptrWords
   */
  writeStructPtr(at, dataOff, dataWords, ptrWords) {
    const offset = dataOff - (at + 1); // signed words from end of ptr
    if (offset < -(1 << 29) || offset >= 1 << 29) {
      throw new Error(`struct offset out of range: ${offset}`);
    }
    // type=00 | offset<<2 | dataWords<<32 | ptrWords<<48
    const lo = (offset << 2) >>> 0;
    const hi = ((dataWords & 0xffff) | ((ptrWords & 0xffff) << 16)) >>> 0;
    this.setWord(at, BigInt(lo) | (BigInt(hi) << 32n));
  }

  /**
   * List pointer (non-composite) at `at` → content at `contentOff`.
   * @param {number} at
   * @param {number} contentOff
   * @param {number} elementSize
   * @param {number} elementCount
   */
  writeListPtr(at, contentOff, elementSize, elementCount) {
    const offset = contentOff - (at + 1);
    if (offset < -(1 << 29) || offset >= 1 << 29) {
      throw new Error(`list offset out of range: ${offset}`);
    }
    // type=01 | offset<<2 | elementSize<<32 | elementCount<<35
    const lo = ((offset << 2) | 1) >>> 0;
    const hi =
      ((elementSize & 7) | ((elementCount & 0x1fffffff) << 3)) >>> 0;
    this.setWord(at, BigInt(lo) | (BigInt(hi) << 32n));
  }

  /**
   * Allocate zero-padded text (NUL-terminated list of bytes) and return content off.
   * @param {string} s
   * @returns {{ contentOff: number, byteCount: number }}
   */
  allocText(s) {
    const enc = new TextEncoder().encode(s ?? "");
    const byteCount = enc.length + 1; // include NUL
    const words = Math.ceil(byteCount / 8);
    const contentOff = this.alloc(words);
    const bytes = new Uint8Array(this._words.buffer, contentOff * 8, words * 8);
    bytes.set(enc);
    bytes[enc.length] = 0;
    return { contentOff, byteCount };
  }

  /**
   * Write a text pointer at `at`.
   * @param {number} at
   * @param {string} s
   */
  writeTextPtr(at, s) {
    const { contentOff, byteCount } = this.allocText(s);
    this.writeListPtr(at, contentOff, ESIZE_BYTE, byteCount);
  }

  /**
   * Allocate AgentId struct (2 data words), return data off.
   * @param {bigint|number} hi
   * @param {bigint|number} lo
   */
  allocAgentId(hi, lo) {
    const off = this.alloc(2);
    this.setWord(off, BigInt(hi));
    this.setWord(off + 1, BigInt(lo));
    return off;
  }

  /** Flat stream: [nsegs-1][seg0_words][segment words…] */
  toUint8Array() {
    const nWords = this.length;
    // header: 2 × u32 when single segment
    const out = new Uint8Array(8 + nWords * 8);
    const view = new DataView(out.buffer);
    view.setUint32(0, 0, true); // nsegs - 1
    view.setUint32(4, nWords, true);
    for (let i = 0; i < nWords; i++) {
      const w = this.getWord(i);
      view.setBigUint64(8 + i * 8, w, true);
    }
    return out;
  }
}

/**
 * Build message with root struct of (dataWords, ptrWords).
 * Root pointer lives at word 0; struct is allocated after.
 * Returns { arena, dataOff, ptrOff }.
 * @param {number} dataWords
 * @param {number} ptrWords
 */
function beginRoot(dataWords, ptrWords) {
  const a = new CapnArena();
  const rootPtr = a.alloc(1); // word 0
  const dataOff = a.alloc(dataWords + ptrWords);
  a.writeStructPtr(rootPtr, dataOff, dataWords, ptrWords);
  return {
    arena: a,
    dataOff,
    ptrOff: dataOff + dataWords,
  };
}

/**
 * @param {{ hi?: number|bigint, lo?: number|bigint }} agent
 * @param {string} cwd
 * @param {string[]} argv
 */
export function encodeShellCheck(agent, cwd, argv) {
  const hi = agent?.hi ?? 1;
  const lo = agent?.lo ?? 2;
  const args = Array.isArray(argv) ? argv : [];
  // ShellCheck: 0 data, 3 pointers (agentId, cwd, argv)
  const { arena, ptrOff } = beginRoot(0, 3);

  // agentId @0
  const idOff = arena.allocAgentId(hi, lo);
  arena.writeStructPtr(ptrOff + 0, idOff, 2, 0);

  // cwd @1 — text content after remaining root slots / lists
  // argv @2 — pointer list of text
  // Allocate pointer-list slots first so list header is contiguous.
  const listOff = arena.alloc(args.length);
  arena.writeListPtr(ptrOff + 2, listOff, ESIZE_POINTER, args.length);
  for (let i = 0; i < args.length; i++) {
    arena.writeTextPtr(listOff + i, args[i]);
  }

  arena.writeTextPtr(ptrOff + 1, cwd ?? "");

  return arena.toUint8Array();
}

/**
 * @param {{ hi?: number|bigint, lo?: number|bigint }} agent
 * @param {number} action PathAction ordinal
 * @param {string} path
 */
export function encodePathCheck(agent, action, path) {
  const hi = agent?.hi ?? 1;
  const lo = agent?.lo ?? 2;
  // PathCheck: 1 data word (action u16), 2 pointers (agentId, path)
  const { arena, dataOff, ptrOff } = beginRoot(1, 2);
  arena.setWord(dataOff, BigInt(action & 0xffff));
  const idOff = arena.allocAgentId(hi, lo);
  arena.writeStructPtr(ptrOff + 0, idOff, 2, 0);
  arena.writeTextPtr(ptrOff + 1, path ?? "");
  return arena.toUint8Array();
}

/**
 * @param {{ hi?: number|bigint, lo?: number|bigint }} agent
 * @param {number} action SeatAction ordinal
 */
export function encodeSeatCheck(agent, action) {
  const hi = agent?.hi ?? 1;
  const lo = agent?.lo ?? 2;
  // SeatCheck: 1 data word, 1 pointer
  const { arena, dataOff, ptrOff } = beginRoot(1, 1);
  arena.setWord(dataOff, BigInt(action & 0xffff));
  const idOff = arena.allocAgentId(hi, lo);
  arena.writeStructPtr(ptrOff + 0, idOff, 2, 0);
  return arena.toUint8Array();
}

/**
 * @param {{ hi?: number|bigint, lo?: number|bigint }} agent
 * @param {number} action RiskAction ordinal
 * @param {string} [path]
 */
export function encodeRiskCheck(agent, action, path = "") {
  const hi = agent?.hi ?? 1;
  const lo = agent?.lo ?? 2;
  // RiskCheck: 1 data word, 2 pointers (agentId, path)
  const { arena, dataOff, ptrOff } = beginRoot(1, 2);
  arena.setWord(dataOff, BigInt(action & 0xffff));
  const idOff = arena.allocAgentId(hi, lo);
  arena.writeStructPtr(ptrOff + 0, idOff, 2, 0);
  arena.writeTextPtr(ptrOff + 1, path ?? "");
  return arena.toUint8Array();
}

/**
 * ReloadShellPack: 0 data, 1 pointer (path text).
 * @param {string} path Absolute MEMFS / host path to .janet pack
 */
export function encodeReloadShellPack(path) {
  const { arena, ptrOff } = beginRoot(0, 1);
  arena.writeTextPtr(ptrOff + 0, path ?? "");
  return arena.toUint8Array();
}

/**
 * Encode a fixture request object into Cap'n bytes.
 * @param {{ method: string, agentId?: object, cwd?: string, argv?: string[],
 *           path?: string, action?: number|string }} req
 */
export function encodeFixtureRequest(req) {
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
      return encodeRiskCheck(
        agent,
        actionOrdinal(req.action, "risk"),
        req.path ?? "",
      );
    case "reloadShellPack":
      return encodeReloadShellPack(req.path ?? "");
    default:
      throw new Error(`unsupported method: ${method}`);
  }
}

const PATH_ACTIONS = { read: 0, write: 1, delete: 2 };
const SEAT_ACTIONS = {
  publishRun: 0,
  readRun: 1,
  listRuns: 2,
  listEvents: 3,
};
const RISK_ACTIONS = {
  network: 0,
  secretExport: 1,
  sudo: 2,
  pay: 3,
  auth: 4,
  osChange: 5,
  privilege: 6,
};

/**
 * @param {number|string|undefined} action
 * @param {"path"|"seat"|"risk"} kind
 */
function actionOrdinal(action, kind) {
  if (typeof action === "number" && Number.isInteger(action)) return action;
  const map =
    kind === "path" ? PATH_ACTIONS : kind === "seat" ? SEAT_ACTIONS : RISK_ACTIONS;
  if (typeof action === "string" && action in map) return map[action];
  if (action === undefined || action === null) {
    if (kind === "path") return 0;
    if (kind === "seat") return 0;
    return 0;
  }
  throw new Error(`unknown ${kind} action: ${action}`);
}
