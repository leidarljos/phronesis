/**
 * Lightweight Janet stream language for the pack editor.
 * Covers comments, strings (incl. long `` / ``` forms), keywords, numbers,
 * and pack-relevant specials — not a full Janet grammar.
 */
import { LanguageSupport, StreamLanguage, type StreamParser } from "@codemirror/language";
import { tags as t } from "@lezer/highlight";

/** Core specials + pack helpers seen in policy/*.janet */
const KEYWORDS = new Set([
  "def",
  "def-",
  "var",
  "var-",
  "defn",
  "defn-",
  "defmacro",
  "defmacro-",
  "defglobal",
  "varglobal",
  "set",
  "setdyn",
  "put",
  "put-in",
  "if",
  "when",
  "unless",
  "cond",
  "case",
  "match",
  "do",
  "upscope",
  "break",
  "return",
  "yield",
  "resume",
  "with",
  "with-syms",
  "with-dyns",
  "defer",
  "edefer",
  "try",
  "catch",
  "while",
  "for",
  "each",
  "eachk",
  "eachp",
  "loop",
  "seq",
  "tabseq",
  "generate",
  "coro",
  "fiber",
  "fn",
  "quote",
  "quasiquote",
  "unquote",
  "splice",
  "true",
  "false",
  "nil",
  "and",
  "or",
  "not",
  "in",
  "as",
  "as?",
  "as-macro",
  "import",
  "use",
  "require",
]);

/** Common pack-side helpers — tint as “builtin” for scannability. */
const BUILTINS = new Set([
  "decide",
  "shell-check",
  "audio-check",
  "shell-danger-deny",
  "shell-secret-deny",
  "touches-python?",
  "uv-run?",
  "pep723?",
  "any-pep723?",
  "read-argv",
]);

type ModeState = {
  commentDepth: number;
  longStringDelim: string | null;
};

function isSymbolStart(ch: string): boolean {
  return /[A-Za-z_!$%&*+\-./<=>?@^~]/.test(ch);
}

function isSymbolCont(ch: string): boolean {
  return /[0-9A-Za-z_!$%&*+\-./:<=>?@^~]/.test(ch);
}

function tokenString(
  stream: {
    next: () => string | void;
    eol: () => boolean;
  },
  quote: string,
): string {
  let escaped = false;
  while (!stream.eol()) {
    const ch = stream.next();
    if (ch == null || ch === "") break;
    if (escaped) {
      escaped = false;
      continue;
    }
    if (ch === "\\") {
      escaped = true;
      continue;
    }
    if (ch === quote) return "string";
  }
  return "string";
}

const janetParser: StreamParser<ModeState> = {
  name: "janet",
  startState(): ModeState {
    return { commentDepth: 0, longStringDelim: null };
  },
  token(stream, state) {
    if (state.commentDepth > 0) {
      while (!stream.eol()) {
        if (stream.match("#|")) {
          state.commentDepth++;
          continue;
        }
        if (stream.match("|#")) {
          state.commentDepth--;
          if (state.commentDepth === 0) return "comment";
          continue;
        }
        stream.next();
      }
      return "comment";
    }

    if (state.longStringDelim) {
      const delim = state.longStringDelim;
      while (!stream.eol()) {
        if (stream.match(delim)) {
          state.longStringDelim = null;
          return "string";
        }
        stream.next();
      }
      return "string";
    }

    if (stream.eatSpace()) return null;

    if (stream.peek() === "#") {
      if (stream.match("#|")) {
        state.commentDepth = 1;
        while (!stream.eol()) {
          if (stream.match("#|")) {
            state.commentDepth++;
            continue;
          }
          if (stream.match("|#")) {
            state.commentDepth--;
            if (state.commentDepth === 0) return "comment";
            continue;
          }
          stream.next();
        }
        return "comment";
      }
      stream.skipToEnd();
      return "comment";
    }

    if (stream.peek() === "`") {
      let ticks = "";
      while (stream.peek() === "`") ticks += stream.next();
      state.longStringDelim = ticks;
      while (!stream.eol()) {
        if (stream.match(ticks)) {
          state.longStringDelim = null;
          return "string";
        }
        stream.next();
      }
      return "string";
    }

    if (stream.peek() === '"') {
      stream.next();
      return tokenString(stream, '"');
    }

    // Keyword atoms :foo
    if (stream.peek() === ":") {
      stream.next();
      stream.eat(":");
      stream.eatWhile(isSymbolCont);
      return "atom";
    }

    if (stream.match(/^-?(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)/)) {
      return "number";
    }

    if (stream.match(/^[()[\]{}]/)) return "bracket";
    if (stream.match(/^['~,|]/)) return "meta";

    if (stream.peek() && isSymbolStart(stream.peek()!)) {
      let word = "";
      while (stream.peek() && isSymbolCont(stream.peek()!)) {
        word += stream.next();
      }
      if (KEYWORDS.has(word)) return "keyword";
      // Pack helpers + mod/name calls — use "atom" (stable highlight tag).
      if (BUILTINS.has(word) || word.includes("/")) return "atom";
      return "variable";
    }

    stream.next();
    return null;
  },
  languageData: {
    commentTokens: { line: "#", block: { open: "#|", close: "|#" } },
    closeBrackets: { brackets: ["(", "[", "{", '"', "`"] },
    indentOnInput: /^\s*[\])}]/,
  },
  tokenTable: {
    keyword: t.keyword,
    atom: t.atom,
    number: t.number,
    string: t.string,
    comment: t.comment,
    bracket: t.bracket,
    meta: t.meta,
    variable: t.variableName,
  },
};

const janetStream = StreamLanguage.define(janetParser);

/** LanguageSupport wrapper so highlighting facets attach reliably. */
export function janet(): LanguageSupport {
  return new LanguageSupport(janetStream);
}
