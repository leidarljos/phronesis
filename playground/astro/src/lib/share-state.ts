/**
 * Hash-based share state for /play.
 * Format: #v1.<base64url(JSON)>
 * Payload: { mode, method, request fields used by MethodForm }
 */

import type { MethodName } from "./capnp-codec";

export type PlayMode = "probe" | "author";

export interface SharePayload {
  v: 1;
  mode: PlayMode;
  method: MethodName | string;
  agentId?: { hi: number; lo: number };
  cwd?: string;
  argv?: string[];
  path?: string;
  action?: string | number;
  model?: string;
  packPath?: string;
  fixtureId?: string;
}

function b64urlEncode(bytes: Uint8Array): string {
  let bin = "";
  for (let i = 0; i < bytes.length; i++) bin += String.fromCharCode(bytes[i]!);
  const b64 = btoa(bin);
  return b64.replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

function b64urlDecode(s: string): Uint8Array {
  const pad = s.length % 4 === 0 ? "" : "=".repeat(4 - (s.length % 4));
  const b64 = s.replace(/-/g, "+").replace(/_/g, "/") + pad;
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

export function encodeShareHash(payload: SharePayload): string {
  const json = JSON.stringify({ ...payload, v: 1 as const });
  const bytes = new TextEncoder().encode(json);
  return `#v1.${b64urlEncode(bytes)}`;
}

export function decodeShareHash(hash: string): SharePayload | null {
  if (!hash) return null;
  const raw = hash.startsWith("#") ? hash.slice(1) : hash;
  if (!raw.startsWith("v1.")) return null;
  try {
    const bytes = b64urlDecode(raw.slice(3));
    const obj = JSON.parse(new TextDecoder().decode(bytes)) as SharePayload;
    if (!obj || obj.v !== 1 || !obj.method) return null;
    if (obj.mode !== "probe" && obj.mode !== "author") obj.mode = "probe";
    return obj;
  } catch {
    return null;
  }
}

export function applyShareToLocation(payload: SharePayload): void {
  const next = encodeShareHash(payload);
  if (history?.replaceState) {
    history.replaceState(null, "", next);
  } else if (typeof location !== "undefined") {
    location.hash = next.slice(1);
  }
}

export function readShareFromLocation(): SharePayload | null {
  if (typeof location === "undefined") return null;
  return decodeShareHash(location.hash);
}

export function shareUrlAbsolute(payload: SharePayload): string {
  if (typeof location === "undefined") return encodeShareHash(payload);
  return `${location.origin}${location.pathname}${encodeShareHash(payload)}`;
}
