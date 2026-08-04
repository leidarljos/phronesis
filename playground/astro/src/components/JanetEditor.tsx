import { useEffect, useRef } from "preact/hooks";
import { Compartment, EditorState } from "@codemirror/state";
import {
  EditorView,
  keymap,
  lineNumbers,
  highlightActiveLine,
  highlightActiveLineGutter,
  drawSelection,
} from "@codemirror/view";
import {
  defaultKeymap,
  history,
  historyKeymap,
  indentWithTab,
} from "@codemirror/commands";
import {
  bracketMatching,
  foldGutter,
  foldKeymap,
  indentOnInput,
} from "@codemirror/language";
import { janetLanguage } from "@lib/janet-lang";
import { playgroundEditorTheme, playgroundSyntax } from "@lib/cm-theme";

interface Props {
  value: string;
  onChange: (value: string) => void;
  disabled?: boolean;
  /** Bumps when the edited file path changes so the doc is replaced. */
  docKey?: string;
}

function editableExtensions(disabled: boolean) {
  return [
    EditorView.editable.of(!disabled),
    EditorState.readOnly.of(!!disabled),
  ];
}

export function JanetEditor({ value, onChange, disabled, docKey }: Props) {
  const hostRef = useRef<HTMLDivElement>(null);
  const viewRef = useRef<EditorView | null>(null);
  const editableComp = useRef(new Compartment());
  const onChangeRef = useRef(onChange);
  onChangeRef.current = onChange;

  useEffect(() => {
    const parent = hostRef.current;
    if (!parent) return;

    const updateListener = EditorView.updateListener.of((update) => {
      if (update.docChanged) {
        onChangeRef.current(update.state.doc.toString());
      }
    });

    const state = EditorState.create({
      doc: value,
      extensions: [
        lineNumbers(),
        highlightActiveLine(),
        highlightActiveLineGutter(),
        drawSelection(),
        history(),
        foldGutter(),
        indentOnInput(),
        bracketMatching(),
        EditorView.lineWrapping,
        janetLanguage,
        playgroundEditorTheme,
        playgroundSyntax,
        keymap.of([
          indentWithTab,
          ...defaultKeymap,
          ...historyKeymap,
          ...foldKeymap,
        ]),
        updateListener,
        editableComp.current.of(editableExtensions(!!disabled)),
      ],
    });

    const view = new EditorView({ state, parent });
    viewRef.current = view;
    return () => {
      view.destroy();
      viewRef.current = null;
    };
    // Mount once; value / disabled synced in effects below.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // External doc replace (file switch). Own edits already match `value`.
  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const cur = view.state.doc.toString();
    if (cur === value) return;
    view.dispatch({
      changes: { from: 0, to: cur.length, insert: value },
    });
  }, [docKey, value]);

  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    view.dispatch({
      effects: editableComp.current.reconfigure(
        editableExtensions(!!disabled),
      ),
    });
  }, [disabled]);

  return (
    <div
      ref={hostRef}
      class={
        disabled ? "janet-editor janet-editor--disabled" : "janet-editor"
      }
      data-doc-key={docKey ?? ""}
      aria-disabled={disabled ? "true" : undefined}
    />
  );
}
