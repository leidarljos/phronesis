/**
 * CodeMirror theme tokens aligned with playground DESIGN.md / global.css.
 * Highlighting uses stable class names so colors live in global.css (reliable
 * under Vite/Preact; tag-only styles were easy to miss next to default ink).
 */

import { HighlightStyle, syntaxHighlighting } from "@codemirror/language";
import { EditorView } from "@codemirror/view";
import { tags as t } from "@lezer/highlight";

/** Editor chrome (surfaces, caret, selection) using CSS variables. */
export const playgroundEditorTheme = EditorView.theme(
  {
    "&": {
      color: "var(--text-secondary)",
      backgroundColor: "var(--bg-input)",
      fontSize: "0.8125rem",
      fontFamily: "var(--mono)",
      width: "100%",
      maxWidth: "100%",
      height: "100%",
    },
    ".cm-content": {
      caretColor: "var(--accent-hover)",
      fontFamily: "var(--mono)",
      padding: "0.55rem 0",
      minHeight: "18rem",
      width: "100%",
      maxWidth: "100%",
      color: "var(--text-secondary)",
    },
    ".cm-cursor, .cm-dropCursor": {
      borderLeftColor: "var(--accent-hover)",
    },
    "&.cm-focused .cm-selectionBackground, .cm-selectionBackground, .cm-content ::selection":
      {
        backgroundColor: "var(--accent-soft)",
      },
    ".cm-activeLine": {
      backgroundColor: "rgba(255, 255, 255, 0.035)",
    },
    ".cm-activeLineGutter": {
      backgroundColor: "rgba(255, 255, 255, 0.035)",
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
      lineHeight: "1.5",
      width: "100%",
    },
    "&.cm-focused": {
      outline: "none",
    },
    "&.cm-editor": {
      borderRadius: "var(--radius-xs)",
      border: "1px solid var(--border)",
      width: "100%",
      maxWidth: "100%",
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

/**
 * Map Lezer tags → stable class names (styled in global.css `.j-*`).
 * Class-based is more reliable than inline HighlightStyle colors here.
 */
export const playgroundHighlightStyle = HighlightStyle.define([
  { tag: t.keyword, class: "j-kw" },
  { tag: t.atom, class: "j-atom" },
  { tag: t.number, class: "j-num" },
  { tag: t.string, class: "j-str" },
  { tag: t.comment, class: "j-cmt" },
  { tag: t.bracket, class: "j-br" },
  { tag: t.meta, class: "j-meta" },
  { tag: t.variableName, class: "j-id" },
  { tag: t.bool, class: "j-kw" },
  { tag: t.null, class: "j-kw" },
  { tag: t.operator, class: "j-op" },
  { tag: t.definition(t.variableName), class: "j-def" },
]);

export const playgroundSyntax = syntaxHighlighting(playgroundHighlightStyle);
