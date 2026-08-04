import { DECISION_LABEL, type FixtureCatalogEntry, reasonLabel } from "@lib/fixtures";
import type { TraceEvent, TraceSpan } from "@lib/wasm";

/** Per-fixture suite evaluation outcome (Author re-eval / Run suite). */
export type SuiteResultStatus = "pass" | "fail" | "error";

export interface SuiteResult {
  status: SuiteResultStatus;
  got?: { decision: number; code: number };
  error?: string;
}

export type SuiteResults = Record<string, SuiteResult>;

interface Props {
  decision: number | null;
  code: number | null;
  argv: string[];
  spans: TraceSpan[];
  error: string | null;
  fixtures: FixtureCatalogEntry[];
  activeFixtureId: string;
  baseUrl: string;
  /** Colored pass/fail from Run suite / post-reload re-eval */
  suiteResults?: SuiteResults;
  suiteRunning?: boolean;
  onRunSuite?: () => void;
  showRunSuite?: boolean;
}

function spansForArgv(spans: TraceSpan[]): Map<number, string[]> {
  const m = new Map<number, string[]>();
  for (const s of spans) {
    if (s.target !== "argv" || typeof s.index !== "number") continue;
    const roles = m.get(s.index) ?? [];
    if (s.role) roles.push(s.role);
    m.set(s.index, roles);
  }
  return m;
}

function collectSpans(events: TraceEvent[]): TraceSpan[] {
  const out: TraceSpan[] = [];
  for (const e of events) {
    if (Array.isArray(e.spans)) out.push(...e.spans);
  }
  return out;
}

export { collectSpans };

export function DecisionPane({
  decision,
  code,
  argv,
  spans,
  error,
  fixtures,
  activeFixtureId,
  baseUrl,
  suiteResults,
  suiteRunning,
  onRunSuite,
  showRunSuite,
}: Props) {
  const label = decision === null ? "—" : (DECISION_LABEL[decision] ?? `D${decision}`);
  const badgeClass =
    decision === 0
      ? "badge deny"
      : decision === 1
        ? "badge allow"
        : decision === 2
          ? "badge prompt"
          : "badge idle";
  const roleMap = spansForArgv(spans);
  const encBase = baseUrl.endsWith("/") ? baseUrl : `${baseUrl}/`;

  let passCount = 0;
  let failCount = 0;
  let errCount = 0;
  if (suiteResults) {
    for (const r of Object.values(suiteResults)) {
      if (r.status === "pass") passCount++;
      else if (r.status === "fail") failCount++;
      else if (r.status === "error") errCount++;
    }
  }
  const hasSuiteResults =
    suiteResults !== undefined && Object.keys(suiteResults).length > 0;

  return (
    <div class="pane decision-pane">
      <header class="pane-header">
        <h2>Decision</h2>
      </header>

      {error && <p class="error-banner">{error}</p>}

      <div class={badgeClass} data-testid="decision-badge">
        {label}
      </div>

      <dl class="kv">
        <div>
          <dt>code</dt>
          <dd data-testid="decision-code">
            {code === null ? "—" : code}{" "}
            {code !== null && <span class="muted">({reasonLabel(code)})</span>}
          </dd>
        </div>
        <div>
          <dt>encyclopedia</dt>
          <dd>
            {code === null ? (
              "—"
            ) : (
              <a href={`${encBase}encyclopedia/${code}`}>/encyclopedia/{code}</a>
            )}
          </dd>
        </div>
      </dl>

      {argv.length > 0 && (
        <section class="match-map">
          <h3>argv match map</h3>
          <ol class="token-list">
            {argv.map((tok, i) => {
              const roles = roleMap.get(i) ?? [];
              const hit = roles.length > 0;
              return (
                <li
                  key={`argv-${i}-${tok}`}
                  class={hit ? "token hit" : "token"}
                  title={roles.join(", ") || undefined}
                >
                  <span class="idx">{i}</span>
                  <code>{tok}</code>
                  {hit && <span class="roles">{roles.join(" · ")}</span>}
                </li>
              );
            })}
          </ol>
        </section>
      )}

      <section class="suite-list">
        <div class="suite-header">
          <h3>Fixture suite</h3>
          {showRunSuite && onRunSuite && (
            <button
              type="button"
              class="btn"
              data-testid="run-suite"
              disabled={suiteRunning}
              onClick={onRunSuite}
            >
              {suiteRunning ? "Running suite…" : "Run suite"}
            </button>
          )}
        </div>
        {hasSuiteResults && (
          <p class="suite-summary muted" data-testid="suite-summary">
            {passCount} pass · {failCount} fail
            {errCount > 0 ? ` · ${errCount} error` : ""}
          </p>
        )}
        <ul>
          {fixtures.map((f) => {
            const result = suiteResults?.[f.id];
            const classes = [
              f.id === activeFixtureId ? "active" : "",
              result ? `suite-${result.status}` : "",
            ]
              .filter(Boolean)
              .join(" ");
            let gotHint = "";
            if (result?.status === "fail" && result.got) {
              gotHint = ` got ${DECISION_LABEL[result.got.decision] ?? result.got.decision}/${result.got.code}`;
            } else if (result?.status === "error" && result.error) {
              gotHint = ` ${result.error}`;
            }
            return (
              <li key={f.id} class={classes || undefined} title={gotHint || undefined}>
                <span class="suite-id">{f.id}</span>
                {f.expect && (
                  <span class="muted">
                    {" "}
                    → {DECISION_LABEL[f.expect.decision] ?? f.expect.decision}/
                    {f.expect.code}
                  </span>
                )}
                {result && (
                  <span class={`suite-mark suite-mark-${result.status}`}>
                    {result.status === "pass"
                      ? " ✓"
                      : result.status === "fail"
                        ? " ✗"
                        : " !"}
                  </span>
                )}
              </li>
            );
          })}
        </ul>
      </section>
    </div>
  );
}
