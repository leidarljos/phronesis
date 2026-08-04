/**
 * CodeMirror theme tokens aligned with playground DESIGN.md / global.css.
 */
import { EditorView } from "@codemirror/view";
import { HighlightStyle, syntaxHighlighting } from "@codemirror/language";
import { tags as t } from "@lezer/highlight";

/** Editor chrome (surfaces, caret, selection) using CSS variables. */
export const playgroundEditorTheme = EditorView.theme(
  {
    "&": {
      color: "var(--text)",
      backgroundColor: "var(--bg-input)",
      fontSize: "0.8125rem",
      fontFamily: "var(--mono)",
    },
    ".cm-content": {
      caretColor: "var(--accent)",
      fontFamily: "var(--mono)",
      padding: "0.55rem 0",
      minHeight: "18rem",
    },
    ".cm-cursor, .cm-dropCursor": {
      borderLeftColor: "var(--accent)",
    },
    "&.cm-focused .cm-selectionBackground, .cm-selectionBackground, .cm-content ::selection":
      {
        backgroundColor: "var(--accent-soft)",
      },
    ".cm-activeLine": {
      backgroundColor: "rgba(255, 255, 255, 0.03)",
    },
    ".cm-activeLineGutter": {
      backgroundColor: "rgba(255, 255, 255, 0.03)",
    },
    ".cm-gutters": {
      backgroundColor: "var(--bg-elev)",
      color: "var(--faint)",
      border: "none",
      borderRight: "1px solid var(--border)",
      fontFamily: "var(--mono)",
      fontSize: "0.7rem",
    },
    ".cm-lineNumbers .cm-gutterElement": {
      padding: "0 0.5rem 0 0.35rem",
      minWidth: "2rem",
    },
    ".cm-scroller": {
      overflow: "auto",
      maxHeight: "28rem",
      fontFamily: "var(--mono)",
      lineHeight: "1.45",
    },
    "&.cm-focused": {
      outline: "none",
    },
    "&.cm-editor": {
      borderRadius: "var(--radius-xs)",
      border: "1px solid var(--border)",
    },
    "&.cm-editor.cm-focused": {
      borderColor: "var(--accent)",
      boxShadow: "var(--ring)",
    },
    ".cm-placeholder": {
      color: "var(--faint)",
    },
  },
  { dark: true },
);

/** Syntax colors — semantic, scarce accent (DESIGN.md). */
export const playgroundHighlightStyle = HighlightStyle.define([
  { tag: t.keyword, color: "#828fff" },
  { tag: t.atom, color: "#c4b5fd" },
  { tag: t.number, color: "#f0abfc" },
  { tag: t.string, color: "#59d499" },
  { tag: t.comment, color: "#5c6370", fontStyle: "italic" },
  { tag: t.bracket, color: "#8b919c" },
  { tag: t.meta, color: "#ffc533" },
  { tag: t.variableName, color: "#f4f5f7" },
  { tag: t.bool, color: "#828fff" },
  { tag: t.null, color: "#828fff" },
]);

export const playgroundSyntax = syntaxHighlighting(playgroundHighlightStyle);
