import { useEffect, useState } from "preact/hooks";
import {
  getModule,
  isEvaluatorReady,
  listMemfsTree,
  readMemfsText,
  reloadPackPath,
  writeMemfsText,
} from "@lib/wasm";

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
  /** Active multi-pack colon list shown to Author mode (reload target). */
  const [packSpec, setPackSpec] = useState(DEFAULT_PACK_SPEC);

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

  useEffect(() => {
    if (isEvaluatorReady()) {
      refreshTree();
      loadFile(DEFAULT_PACK);
    }
  }, [disabled]);

  // Re-scan when evaluator becomes ready (parent toggles disabled)
  useEffect(() => {
    if (!disabled && isEvaluatorReady()) {
      refreshTree();
      if (!body) loadFile(selected || DEFAULT_PACK);
    }
  }, [disabled]);

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
      // Multi-pack: reload the full colon list (entry + packs.d / extras).
      // lib/*.janet under each entry's dirname still load with that pack.
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

  return (
    <div class="pane pack-editor">
      <header class="pane-header">
        <h2>Pack editor</h2>
      </header>

      {!isEvaluatorReady() && (
        <p class="hint">Load the evaluator to edit MEMFS pack files.</p>
      )}

      <div class="pack-layout">
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
                  class={
                    e.path === selected ? "tree-item active" : "tree-item"
                  }
                  disabled={disabled}
                  onClick={() => loadFile(e.path)}
                >
                  {e.path}
                </button>
              </li>
            ))}
            {tree.length === 0 && (
              <li class="muted">No /policy files yet</li>
            )}
          </ul>
        </aside>
        <div class="pack-body">
          <label class="field">
            <span>{selected || "—"}</span>
            <textarea
              rows={18}
              value={body}
              disabled={disabled || !selected}
              spellcheck={false}
              onInput={(e) => {
                setBody((e.target as HTMLTextAreaElement).value);
                setDirty(true);
              }}
            />
          </label>
          <label class="field">
            <span>Multi-pack reload spec (colon list)</span>
            <input
              type="text"
              value={packSpec}
              disabled={disabled || busy}
              spellcheck={false}
              onInput={(e) =>
                setPackSpec((e.target as HTMLInputElement).value)
              }
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
          <p class="hint">
            Product multi-pack: colon-separated absolute{" "}
            <code>.janet</code> files and/or directories (e.g.{" "}
            <code>/policy/shell.janet:/policy/packs.d</code>). Each pack loads
            its own sealed env + sibling <code>lib/*.janet</code>. Composition
            is fail-closed: <strong>deny &gt; prompt &gt; allow</strong> across
            packs that define <code>shell-check</code> /{" "}
            <code>audio-check</code>. Default MEMFS seeds product{" "}
            <code>shell.janet</code> plus <code>packs.d/extra-canary.janet</code>{" "}
            (denies argv containing <code>multipack-demo</code>).
          </p>
        </div>
      </div>
    </div>
  );
}
