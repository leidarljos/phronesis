import {
  getModule,
  isEvaluatorReady,
  listMemfsTree,
  readMemfsText,
  reloadPackPath,
  writeMemfsText,
} from "@lib/wasm";
import { useEffect, useState } from "preact/hooks";
import { JanetEditor } from "./JanetEditor";

/** Product entry under MEMFS (always first in the multi-pack colon list). */
const DEFAULT_PACK = "/policy/shell.janet";
/**
 * Default multi-pack load/reload spec (matches embind PD_DEFAULT_PACK_SPEC).
 * Colon list: product entry + packs.d directory of optional extra packs.
 * Host composition: deny > prompt > allow across packs that define shell-check.
 */
const DEFAULT_PACK_SPEC = "/policy/shell.janet:/policy/packs.d";

interface Props {
  disabled?: boolean;
  onReloaded?: (rc: number) => void;
  onStatus?: (msg: string) => void;
}

export function PackEditor({ disabled, onReloaded, onStatus }: Props) {
  const [tree, setTree] = useState<{ path: string; isDir: boolean }[]>([]);
  const [selected, setSelected] = useState(DEFAULT_PACK);
  const [body, setBody] = useState("");
  const [dirty, setDirty] = useState(false);
  const [busy, setBusy] = useState(false);
  const [packSpec, setPackSpec] = useState(DEFAULT_PACK_SPEC);
  const [expanded, setExpanded] = useState(false);

  function refreshTree() {
    const m = getModule();
    if (!m) {
      setTree([]);
      return;
    }
    try {
      const entries = listMemfsTree(m, "/policy").filter((e) => !e.isDir);
      setTree(entries);
    } catch (e) {
      onStatus?.(`list pack tree failed: ${e}`);
    }
  }

  function loadFile(path: string) {
    const m = getModule();
    if (!m) return;
    try {
      const text = readMemfsText(m, path);
      setSelected(path);
      setBody(text);
      setDirty(false);
      onStatus?.(`loaded ${path}`);
    } catch (e) {
      onStatus?.(`read ${path}: ${e}`);
    }
  }

  // Seed tree/file when the evaluator becomes available (disabled flips false).
  // Only re-run on the load gate — not on every body/selection change.
  useEffect(() => {
    if (!isEvaluatorReady()) return;
    refreshTree();
    if (!body) loadFile(selected || DEFAULT_PACK);
  }, [disabled]);

  useEffect(() => {
    if (!expanded) return;
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") setExpanded(false);
    };
    const prevOverflow = document.body.style.overflow;
    document.body.style.overflow = "hidden";
    window.addEventListener("keydown", onKey);
    return () => {
      document.body.style.overflow = prevOverflow;
      window.removeEventListener("keydown", onKey);
    };
  }, [expanded]);

  async function onSaveWrite() {
    const m = getModule();
    if (!m) {
      onStatus?.("Load evaluator first");
      return;
    }
    setBusy(true);
    try {
      writeMemfsText(m, selected, body);
      setDirty(false);
      onStatus?.(`wrote ${selected}`);
      refreshTree();
    } catch (e) {
      onStatus?.(`write failed: ${e}`);
    } finally {
      setBusy(false);
    }
  }

  async function onReload() {
    if (!isEvaluatorReady()) {
      onStatus?.("Load evaluator first");
      return;
    }
    setBusy(true);
    try {
      const m = getModule();
      if (m && dirty) {
        writeMemfsText(m, selected, body);
        setDirty(false);
      }
      const spec = (packSpec || DEFAULT_PACK_SPEC).trim() || DEFAULT_PACK_SPEC;
      const rc = await reloadPackPath(spec);
      onReloaded?.(rc);
      if (rc === 0) onStatus?.(`reloaded multi-pack: ${spec}`);
      else if (rc === -1) onStatus?.(`reload path invalid: ${spec}`);
      else onStatus?.(`reload failed (rc=${rc}): ${spec}`);
    } catch (e) {
      onStatus?.(`reload error: ${e}`);
      onReloaded?.(-2);
    } finally {
      setBusy(false);
    }
  }

  const editorDisabled = disabled || !selected || !isEvaluatorReady();

  function onEditorChange(text: string) {
    setBody(text);
    setDirty(true);
  }

  function renderTree() {
    return (
      <aside class="pack-tree">
        <div class="tree-actions">
          <button
            type="button"
            class="btn"
            disabled={disabled || busy}
            onClick={refreshTree}
          >
            Refresh
          </button>
        </div>
        <ul>
          {tree.map((e) => (
            <li key={e.path}>
              <button
                type="button"
                class={e.path === selected ? "tree-item active" : "tree-item"}
                disabled={disabled}
                onClick={() => loadFile(e.path)}
                title={e.path}
              >
                {e.path}
              </button>
            </li>
          ))}
          {tree.length === 0 && <li class="muted">No /policy files yet</li>}
        </ul>
      </aside>
    );
  }

  function renderActions() {
    return (
      <>
        <label class="field">
          <span>Multi-pack reload spec (colon list)</span>
          <input
            type="text"
            value={packSpec}
            disabled={disabled || busy}
            spellcheck={false}
            onInput={(e) => setPackSpec((e.target as HTMLInputElement).value)}
          />
        </label>
        <div class="btn-row">
          <button
            type="button"
            class="btn"
            disabled={disabled || busy || !dirty}
            onClick={onSaveWrite}
          >
            Write MEMFS
          </button>
          <button
            type="button"
            class="btn primary"
            disabled={disabled || busy}
            onClick={onReload}
          >
            Write + reload multi-pack
          </button>
        </div>
      </>
    );
  }

  return (
    <div class="pane pack-editor">
      <header class="pane-header pack-editor-header">
        <h2>Pack editor</h2>
        <button
          type="button"
          class="btn"
          disabled={editorDisabled}
          onClick={() => setExpanded(true)}
          title="Open a larger editor (Esc to close)"
        >
          Expand
        </button>
      </header>

      {!isEvaluatorReady() && (
        <p class="hint">Load the evaluator to edit MEMFS pack files.</p>
      )}

      <div class="pack-layout">
        {renderTree()}
        <div class="pack-body">
          <div class="field">
            <span class="field-label">{selected || "—"}</span>
            {/* Unmount while modal owns the editor to avoid dual views. */}
            {!expanded && (
              <JanetEditor
                key={`inline-${selected}`}
                value={body}
                docKey={selected}
                variant="inline"
                disabled={editorDisabled}
                onChange={onEditorChange}
              />
            )}
            {expanded && (
              <p class="hint pack-editor-parked">Editor open in expanded view…</p>
            )}
          </div>
          {renderActions()}
          <p class="hint">
            Product multi-pack: colon-separated absolute <code>.janet</code> files and/or
            directories (e.g. <code>/policy/shell.janet:/policy/packs.d</code>). Each pack
            loads its own sealed env + sibling <code>lib/*.janet</code>. Composition is
            fail-closed: <strong>deny &gt; prompt &gt; allow</strong> across packs that
            define <code>shell-check</code> / <code>audio-check</code>. Default MEMFS
            seeds product <code>shell.janet</code> plus{" "}
            <code>packs.d/extra-canary.janet</code> (denies argv containing{" "}
            <code>multipack-demo</code>).
          </p>
        </div>
      </div>

      {expanded && (
        <div
          class="pack-modal-backdrop"
          role="presentation"
          onClick={(e) => {
            if (e.target === e.currentTarget) setExpanded(false);
          }}
        >
          <div
            class="pack-modal"
            role="dialog"
            aria-modal="true"
            aria-label="Pack editor expanded"
          >
            <header class="pack-modal-header">
              <div class="pack-modal-title">
                <h2>Pack editor</h2>
                <code class="pack-modal-path">{selected || "—"}</code>
              </div>
              <button type="button" class="btn" onClick={() => setExpanded(false)}>
                Close
              </button>
            </header>
            <div class="pack-modal-body">
              {renderTree()}
              <div class="pack-modal-editor">
                <JanetEditor
                  key={`expanded-${selected}`}
                  value={body}
                  docKey={selected}
                  variant="expanded"
                  disabled={editorDisabled}
                  onChange={onEditorChange}
                />
                <div class="pack-modal-actions">{renderActions()}</div>
              </div>
            </div>
            <p class="hint pack-modal-hint">Esc or backdrop click to close.</p>
          </div>
        </div>
      )}
    </div>
  );
}
