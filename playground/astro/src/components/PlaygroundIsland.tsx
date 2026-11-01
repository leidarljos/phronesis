import type { EncodeRequest } from "@lib/capnp-codec";
import { type FixtureBody, fetchFixture, listFixtures } from "@lib/fixtures";
import { warnSecretsInArgv } from "@lib/secret-warn";
import {
  applyShareToLocation,
  encodeShareHash,
  type PlayMode,
  readShareFromLocation,
  type SharePayload,
  shareUrlAbsolute,
} from "@lib/share-state";
import { isMod, isTypingTarget } from "@lib/shortcuts";
import {
  isEvaluatorReady,
  type LoadState,
  loadEvaluator,
  runCheck,
  type TraceEvent,
} from "@lib/wasm";
import { useCallback, useEffect, useMemo, useRef, useState } from "preact/hooks";
import { collectSpans, DecisionPane, type SuiteResults } from "./DecisionPane";
import {
  DEFAULT_FORM,
  defaultActionForMethod,
  formToArgv,
  MethodForm,
  type MethodFormState,
} from "./MethodForm";
import { PackEditor, type PackEditorActions } from "./PackEditor";
import { ShortcutsHelp } from "./ShortcutsHelp";
import { TraceView } from "./TraceView";

function formFromShare(p: SharePayload): MethodFormState {
  const method = p.method || "checkShell";
  return {
    method,
    agentHi: p.agentId?.hi ?? 1,
    agentLo: p.agentId?.lo ?? 2,
    cwd: p.cwd ?? "/ws",
    argvText: (p.argv ?? DEFAULT_FORM.argvText.split("\n")).join("\n"),
    path: p.path ?? DEFAULT_FORM.path,
    action: String(p.action ?? defaultActionForMethod(method)),
    model: p.model ?? "",
    fixtureId: p.fixtureId ?? "",
  };
}

function shareFromForm(mode: PlayMode, form: MethodFormState): SharePayload {
  const argv = formToArgv(form);
  return {
    v: 1,
    mode,
    method: form.method,
    agentId: { hi: form.agentHi, lo: form.agentLo },
    cwd: form.cwd,
    argv,
    path: form.path,
    action: form.action,
    model: form.model || undefined,
    fixtureId: form.fixtureId || undefined,
  };
}

function formToRequest(form: MethodFormState): EncodeRequest {
  return {
    method: form.method,
    agentId: { hi: form.agentHi, lo: form.agentLo },
    cwd: form.cwd,
    argv: formToArgv(form),
    path: form.path,
    action: form.action,
    model: form.model,
  };
}

function fixtureToForm(fx: FixtureBody): MethodFormState {
  return {
    method: fx.method,
    agentHi: fx.agentId?.hi ?? 1,
    agentLo: fx.agentId?.lo ?? 2,
    cwd: fx.cwd ?? "/ws",
    argvText: (fx.argv ?? []).join("\n"),
    path: fx.path ?? "/etc/passwd",
    action: String(fx.action ?? defaultActionForMethod(fx.method)),
    model: "",
    fixtureId: fx.id,
  };
}

function fixtureToRequest(fx: FixtureBody): EncodeRequest {
  return {
    method: fx.method,
    agentId: fx.agentId ?? { hi: 1, lo: 2 },
    cwd: fx.cwd ?? "/ws",
    argv: fx.argv ?? [],
    path: fx.path ?? "",
    action: fx.action,
  };
}

interface Props {
  /** Optional fixture id from URL query / fixtures page */
  initialFixtureId?: string;
  baseUrl?: string;
}

export default function PlaygroundIsland({ initialFixtureId, baseUrl }: Props) {
  const base = baseUrl ?? import.meta.env.BASE_URL ?? "/";
  const [mode, setMode] = useState<PlayMode>("probe");
  const [loadState, setLoadState] = useState<LoadState>("idle");
  const [loadError, setLoadError] = useState<string | null>(null);
  const [form, setForm] = useState<MethodFormState>(DEFAULT_FORM);
  const [running, setRunning] = useState(false);
  const [decision, setDecision] = useState<number | null>(null);
  const [code, setCode] = useState<number | null>(null);
  const [trace, setTrace] = useState<TraceEvent[]>([]);
  const [evalError, setEvalError] = useState<string | null>(null);
  const [status, setStatus] = useState<string | null>(null);
  const [lastMemfs, setLastMemfs] = useState<Record<string, string> | undefined>(
    undefined,
  );
  const [shareMsg, setShareMsg] = useState<string | null>(null);
  const [hydrated, setHydrated] = useState(false);
  const [suiteResults, setSuiteResults] = useState<SuiteResults>({});
  const [suiteRunning, setSuiteRunning] = useState(false);
  const [shortcutsOpen, setShortcutsOpen] = useState(false);
  const packActionsRef = useRef<PackEditorActions | null>(null);
  /** Expand requested before Author/PackEditor mounted (e.g. ⌘. from Probe). */
  const pendingExpandRef = useRef(false);

  const fixtures = useMemo(() => listFixtures(), []);
  const ready = loadState === "ready" || isEvaluatorReady();

  const onPackActions = useCallback((actions: PackEditorActions | null) => {
    packActionsRef.current = actions;
    if (actions && pendingExpandRef.current) {
      pendingExpandRef.current = false;
      actions.expand();
    }
  }, []);

  // Hydrate from hash + optional fixture id
  useEffect(() => {
    const shared = readShareFromLocation();
    if (shared) {
      setMode(shared.mode);
      setForm(formFromShare(shared));
      setHydrated(true);
      return;
    }
    if (initialFixtureId) {
      void (async () => {
        try {
          const fx = await fetchFixture(initialFixtureId, base);
          setForm(fixtureToForm(fx));
          setLastMemfs(fx.memfs);
        } catch (e) {
          setEvalError(`fixture ${initialFixtureId}: ${e}`);
        } finally {
          setHydrated(true);
        }
      })();
      return;
    }
    setHydrated(true);
  }, [initialFixtureId, base]);

  const onLoadEvaluator = useCallback(async () => {
    setLoadState("loading");
    setLoadError(null);
    try {
      await loadEvaluator(base);
      setLoadState("ready");
      setStatus("Evaluator ready (supervisor open)");
    } catch (e) {
      setLoadState("error");
      setLoadError(String(e));
    }
  }, [base]);

  const onLoadFixture = useCallback(
    async (id: string) => {
      try {
        const fx = await fetchFixture(id, base);
        setForm(fixtureToForm(fx));
        setLastMemfs(fx.memfs);
        setStatus(`Loaded fixture ${id}`);
        setEvalError(null);
      } catch (e) {
        setEvalError(`load fixture: ${e}`);
      }
    },
    [base],
  );

  const onRun = useCallback(async () => {
    if (!ready) {
      setEvalError('Click "Load evaluator" first');
      return;
    }
    setRunning(true);
    setEvalError(null);
    try {
      const req = formToRequest(form);
      const result = await runCheck(form.method, req, lastMemfs);
      setDecision(result.decision);
      setCode(result.code);
      setTrace(result.trace);
      setStatus(
        `${form.method} → ${result.decision}/${result.code} (${result.trace.length} trace events)`,
      );
    } catch (e) {
      setEvalError(String(e));
      setDecision(null);
      setCode(null);
      setTrace([]);
    } finally {
      setRunning(false);
    }
  }, [ready, form, lastMemfs]);

  /** Evaluate all shell fixtures against the current pack; color suite list. */
  const onRunSuite = useCallback(async () => {
    if (!ready) {
      setEvalError('Click "Load evaluator" first');
      return;
    }
    setSuiteRunning(true);
    setEvalError(null);
    const shell = fixtures.filter((f) => f.method === "checkShell");
    const next: SuiteResults = {};
    let pass = 0;
    let fail = 0;
    let err = 0;
    for (const meta of shell) {
      try {
        const fx = await fetchFixture(meta.id, base);
        const result = await runCheck(fx.method, fixtureToRequest(fx), fx.memfs);
        const expect = meta.expect;
        const ok =
          expect != null &&
          result.decision === expect.decision &&
          result.code === expect.code;
        if (ok) {
          next[meta.id] = {
            status: "pass",
            got: { decision: result.decision, code: result.code },
          };
          pass++;
        } else {
          next[meta.id] = {
            status: "fail",
            got: { decision: result.decision, code: result.code },
          };
          fail++;
        }
      } catch (e) {
        next[meta.id] = { status: "error", error: String(e) };
        err++;
      }
    }
    setSuiteResults(next);
    setStatus(
      `Suite: ${pass} pass · ${fail} fail${err > 0 ? ` · ${err} error` : ""} (${shell.length} shell fixtures)`,
    );
    setSuiteRunning(false);
  }, [ready, fixtures, base]);

  const onShare = useCallback(() => {
    const argv = formToArgv(form);
    const warn = warnSecretsInArgv(argv);
    if (warn.risky) {
      const ok = window.confirm(
        `${warn.message}\n\nTokens: ${warn.tokens.map((t) => `${t.slice(0, 16)}…`).join(", ")}\n\nShare anyway?`,
      );
      if (!ok) {
        setShareMsg("Share cancelled (secret-looking argv)");
        return;
      }
    }
    const payload = shareFromForm(mode, form);
    applyShareToLocation(payload);
    const url = shareUrlAbsolute(payload);
    void navigator.clipboard?.writeText(url).then(
      () => setShareMsg("Share URL copied to clipboard"),
      () => setShareMsg(`Share hash: ${encodeShareHash(payload)}`),
    );
  }, [form, mode]);

  const argv = formToArgv(form);
  const spans = collectSpans(trace);

  // App-level keybindings (catalog in @lib/shortcuts + ShortcutsHelp).
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const typing = isTypingTarget(e.target);
      const mod = isMod(e);

      if (e.key === "Escape") {
        const pack = packActionsRef.current;
        if (pack?.isExpanded()) {
          e.preventDefault();
          pack.collapse();
          return;
        }
        if (shortcutsOpen) {
          e.preventDefault();
          setShortcutsOpen(false);
        }
        return;
      }

      // "?" toggles help when not typing (Shift+/ on US layouts).
      if (!typing && (e.key === "?" || (e.key === "/" && e.shiftKey))) {
        e.preventDefault();
        setShortcutsOpen((o) => !o);
        return;
      }

      if (!mod) return;

      const key = e.key.toLowerCase();

      if (key === "l" && !e.shiftKey) {
        if (loadState === "ready" || isEvaluatorReady()) return;
        e.preventDefault();
        void onLoadEvaluator();
        return;
      }

      if (key === "1" && !e.shiftKey) {
        e.preventDefault();
        setMode("probe");
        return;
      }
      if (key === "2" && !e.shiftKey) {
        e.preventDefault();
        setMode("author");
        return;
      }

      if (key === "enter") {
        e.preventDefault();
        if (e.shiftKey) void onRunSuite();
        else if (mode === "probe") void onRun();
        else void onRunSuite();
        return;
      }

      if (key === "s") {
        e.preventDefault();
        const pack = packActionsRef.current;
        if (!pack) {
          setMode("author");
          setStatus("Author mode — press ⌘S / Ctrl+S again to write MEMFS");
          return;
        }
        if (e.shiftKey) void pack.writeReload();
        else void pack.writeMemfs();
        return;
      }

      if (key === "." || e.code === "Period") {
        e.preventDefault();
        if (packActionsRef.current) {
          packActionsRef.current.expand();
        } else {
          pendingExpandRef.current = true;
          setMode("author");
        }
        return;
      }

      if (key === "u" && e.shiftKey) {
        e.preventDefault();
        onShare();
      }
    };

    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [shortcutsOpen, loadState, mode, onLoadEvaluator, onRun, onRunSuite, onShare]);

  if (!hydrated) {
    return <div class="playground loading-shell">Loading…</div>;
  }

  return (
    <div class="playground" data-mode={mode}>
      <div class="toolbar">
        <div class="mode-toggle" role="tablist">
          <button
            type="button"
            role="tab"
            class={mode === "probe" ? "active" : ""}
            aria-selected={mode === "probe"}
            onClick={() => setMode("probe")}
            title="⌘1 / Ctrl+1"
          >
            Probe
          </button>
          <button
            type="button"
            role="tab"
            class={mode === "author" ? "active" : ""}
            aria-selected={mode === "author"}
            onClick={() => setMode("author")}
            title="⌘2 / Ctrl+2"
          >
            Author
          </button>
        </div>

        <div class="toolbar-actions">
          {loadState !== "ready" && !isEvaluatorReady() ? (
            <button
              type="button"
              class="btn primary"
              disabled={loadState === "loading"}
              onClick={onLoadEvaluator}
              data-testid="load-evaluator"
              title="⌘L / Ctrl+L"
            >
              {loadState === "loading" ? "Loading WASM…" : "Load evaluator"}
            </button>
          ) : (
            <span class="status ok">Evaluator loaded</span>
          )}
          <button type="button" class="btn" onClick={onShare} title="⌘⇧U / Ctrl+Shift+U">
            Share URL
          </button>
        </div>
      </div>

      <ShortcutsHelp open={shortcutsOpen} onToggle={() => setShortcutsOpen((o) => !o)} />

      {loadError && <p class="error-banner">{loadError}</p>}
      {shareMsg && <p class="hint">{shareMsg}</p>}
      {status && <p class="hint muted">{status}</p>}

      <div class="three-pane">
        <div class="col left">
          {mode === "probe" ? (
            <MethodForm
              form={form}
              onChange={setForm}
              onRun={onRun}
              onLoadFixture={onLoadFixture}
              disabled={!ready}
              running={running}
            />
          ) : (
            <PackEditor
              disabled={!ready}
              onStatus={setStatus}
              onActions={onPackActions}
              onReloaded={(rc) => {
                if (rc === 0) {
                  setStatus("Pack reloaded — re-evaluating shell suite…");
                  void onRunSuite();
                }
              }}
            />
          )}
        </div>
        <div class="col mid">
          <DecisionPane
            decision={decision}
            code={code}
            argv={mode === "probe" ? argv : []}
            spans={spans}
            error={evalError}
            fixtures={fixtures}
            activeFixtureId={form.fixtureId}
            baseUrl={base}
            suiteResults={suiteResults}
            suiteRunning={suiteRunning}
            onRunSuite={onRunSuite}
            showRunSuite={ready}
          />
          {mode === "author" && (
            <div class="author-probe-hint pane">
              <p class="hint">
                After a successful pack reload the shell fixture suite re-runs
                automatically (green/red in the list). Use Run suite any time, or Probe
                with the current form for a single check.
              </p>
              <button
                type="button"
                class="btn"
                disabled={!ready || running}
                onClick={() => {
                  setMode("probe");
                  void onRun();
                }}
              >
                Probe with current form
              </button>
            </div>
          )}
        </div>
        <div class="col right">
          <TraceView
            events={trace}
            baseUrl={base}
            emptyHint={
              ready
                ? "Run Evaluate to capture TRACE events."
                : "Load evaluator, then Evaluate (e.g. curl_sh fixture)."
            }
          />
        </div>
      </div>
    </div>
  );
}
