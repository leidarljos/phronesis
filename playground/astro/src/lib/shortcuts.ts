/**
 * Playground keyboard shortcuts — single catalog for the handler + help UI.
 * Zero deps: app-level only (CodeMirror keeps its own defaultKeymap).
 */

export type ShortcutScope = "global" | "probe" | "author" | "editor";

export type ShortcutDef = {
  id: string;
  /** Display chord on Apple platforms */
  mac: string;
  /** Display chord elsewhere */
  other: string;
  /** What it does */
  action: string;
  scope: ShortcutScope;
  /** Extra context for the help table */
  when?: string;
};

/** App-level bindings we implement in PlaygroundIsland / PackEditor. */
export const APP_SHORTCUTS: ShortcutDef[] = [
  {
    id: "load",
    mac: "⌘L",
    other: "Ctrl+L",
    action: "Load evaluator",
    scope: "global",
    when: "when not loaded",
  },
  {
    id: "eval",
    mac: "⌘↵",
    other: "Ctrl+Enter",
    action: "Evaluate current check",
    scope: "probe",
  },
  {
    id: "suite",
    mac: "⌘⇧↵",
    other: "Ctrl+Shift+Enter",
    action: "Run shell fixture suite",
    scope: "global",
    when: "evaluator loaded",
  },
  {
    id: "probe",
    mac: "⌘1",
    other: "Ctrl+1",
    action: "Switch to Probe",
    scope: "global",
  },
  {
    id: "author",
    mac: "⌘2",
    other: "Ctrl+2",
    action: "Switch to Author",
    scope: "global",
  },
  {
    id: "expand",
    mac: "⌘.",
    other: "Ctrl+.",
    action: "Expand / focus pack editor",
    scope: "author",
  },
  {
    id: "write",
    mac: "⌘S",
    other: "Ctrl+S",
    action: "Write MEMFS",
    scope: "author",
  },
  {
    id: "reload",
    mac: "⌘⇧S",
    other: "Ctrl+Shift+S",
    action: "Write + reload multi-pack",
    scope: "author",
  },
  {
    id: "share",
    mac: "⌘⇧U",
    other: "Ctrl+Shift+U",
    action: "Share URL",
    scope: "global",
  },
  {
    id: "help",
    mac: "?",
    other: "?",
    action: "Toggle this shortcuts list",
    scope: "global",
    when: "not typing in a field",
  },
  {
    id: "esc",
    mac: "Esc",
    other: "Esc",
    action: "Close expand modal / shortcuts",
    scope: "global",
  },
];

/** Document-only: already provided by CodeMirror defaultKeymap / history. */
export const EDITOR_SHORTCUTS: ShortcutDef[] = [
  {
    id: "undo",
    mac: "⌘Z",
    other: "Ctrl+Z",
    action: "Undo",
    scope: "editor",
  },
  {
    id: "redo",
    mac: "⌘⇧Z",
    other: "Ctrl+Y",
    action: "Redo",
    scope: "editor",
  },
  {
    id: "indent",
    mac: "Tab",
    other: "Tab",
    action: "Indent (Shift+Tab outdent)",
    scope: "editor",
  },
  {
    id: "fold",
    mac: "⌘⌥[",
    other: "Ctrl+Alt+[",
    action: "Fold (unfold ⌘⌥])",
    scope: "editor",
  },
];

export function isApplePlatform(): boolean {
  if (typeof navigator === "undefined") return false;
  return /Mac|iPhone|iPad|iPod/i.test(navigator.platform || navigator.userAgent);
}

export function chordLabel(def: ShortcutDef): string {
  return isApplePlatform() ? def.mac : def.other;
}

export function isMod(e: KeyboardEvent): boolean {
  return e.metaKey || e.ctrlKey;
}

/** True when the event target is an editable control (incl. CodeMirror). */
export function isTypingTarget(t: EventTarget | null): boolean {
  if (!(t instanceof HTMLElement)) return false;
  const tag = t.tagName;
  if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") return true;
  if (t.isContentEditable) return true;
  if (t.closest(".cm-content, .cm-editor")) return true;
  return false;
}

/** Compact chips shown under the toolbar (always visible). */
export const SHORTCUT_CHIPS: { id: string; label: string }[] = [
  { id: "eval", label: "Evaluate" },
  { id: "suite", label: "Suite" },
  { id: "write", label: "Write" },
  { id: "reload", label: "Reload pack" },
  { id: "help", label: "Help" },
];
