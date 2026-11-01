import {
  APP_SHORTCUTS,
  chordLabel,
  EDITOR_SHORTCUTS,
  SHORTCUT_CHIPS,
  type ShortcutDef,
} from "@lib/shortcuts";

interface Props {
  open: boolean;
  onToggle: () => void;
}

function Row({ def }: { def: ShortcutDef }) {
  return (
    <tr>
      <td>
        <kbd class="kbd">{chordLabel(def)}</kbd>
      </td>
      <td>{def.action}</td>
      <td class="muted">{def.when ?? def.scope}</td>
    </tr>
  );
}

export function ShortcutsHelp({ open, onToggle }: Props) {
  return (
    <div class="shortcuts-help">
      <div class="shortcut-chips">
        {SHORTCUT_CHIPS.map((chip) => {
          const def = APP_SHORTCUTS.find((s) => s.id === chip.id);
          if (!def) return null;
          return (
            <span class="shortcut-chip" key={chip.id}>
              <kbd class="kbd">{chordLabel(def)}</kbd>
              <span class="chip-label">{chip.label}</span>
            </span>
          );
        })}
        <button
          type="button"
          class="btn shortcuts-toggle"
          aria-expanded={open}
          onClick={onToggle}
        >
          {open ? "Hide shortcuts" : "All shortcuts"}
        </button>
      </div>

      {open && (
        <section class="shortcuts-panel pane" aria-label="Keyboard shortcuts">
          <header class="pane-header">
            <h2>Keyboard shortcuts</h2>
            <button type="button" class="btn" onClick={onToggle}>
              Close
            </button>
          </header>
          <p class="hint">
            App shortcuts work on <code>/play</code>. Pack editor also uses CodeMirror
            defaults (undo, indent, fold). Press <kbd class="kbd">?</kbd> when not typing
            to toggle this panel.
          </p>
          <h3 class="shortcuts-section">Playground</h3>
          <table class="shortcuts-table">
            <thead>
              <tr>
                <th>Keys</th>
                <th>Action</th>
                <th>When</th>
              </tr>
            </thead>
            <tbody>
              {APP_SHORTCUTS.map((def) => (
                <Row key={def.id} def={def} />
              ))}
            </tbody>
          </table>
          <h3 class="shortcuts-section">Pack editor (CodeMirror)</h3>
          <table class="shortcuts-table">
            <thead>
              <tr>
                <th>Keys</th>
                <th>Action</th>
                <th>When</th>
              </tr>
            </thead>
            <tbody>
              {EDITOR_SHORTCUTS.map((def) => (
                <Row key={def.id} def={def} />
              ))}
            </tbody>
          </table>
        </section>
      )}
    </div>
  );
}
