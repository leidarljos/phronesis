import type { TraceEvent } from "@lib/wasm";

interface Props {
  events: TraceEvent[];
  emptyHint?: string;
  /** Site base (e.g. /phronesis/) for encyclopedia deep-links */
  baseUrl?: string;
}

function shortCircuitWhy(events: TraceEvent[]): string | null {
  const sc = events.find(
    (e) =>
      e &&
      e.shortCircuit === true &&
      (e.decision === "deny" || e.decision === 0 || typeof e.code === "number"),
  );
  if (!sc) return null;
  const parts: string[] = ["shortCircuit"];
  if (sc.name) parts.push(String(sc.name));
  if (sc.phase) parts.push(String(sc.phase));
  if (typeof sc.code === "number") parts.push(`code=${sc.code}`);
  if (sc.reason) parts.push(String(sc.reason));
  return parts.join(" · ");
}

export function TraceView({ events, emptyHint, baseUrl }: Props) {
  const why = shortCircuitWhy(events);
  const encBase = (baseUrl ?? "/").endsWith("/") ? (baseUrl ?? "/") : `${baseUrl ?? ""}/`;

  return (
    <div class="pane trace-view">
      <header class="pane-header">
        <h2>Forensic trace</h2>
      </header>

      {why && (
        <div class="why-not" data-testid="why-not">
          <strong>why-not</strong>
          <span>{why}</span>
        </div>
      )}

      {events.length === 0 ? (
        <p class="muted">{emptyHint ?? "No TRACE events yet."}</p>
      ) : (
        <ol class="trace-steps">
          {events.map((e, i) => (
            <li
              key={`trace-${typeof e.seq === "number" ? e.seq : i}-${e.phase ?? ""}-${e.name ?? ""}`}
              class="trace-step"
              data-phase={e.phase ?? undefined}
            >
              <div class="trace-head">
                <span class="phase">{e.phase ?? "—"}</span>
                <span class="name">{e.name ?? ""}</span>
                {typeof e.code === "number" && (
                  <span class="code">
                    code=
                    <a href={`${encBase}encyclopedia/${e.code}`}>{e.code}</a>
                  </span>
                )}
                {e.shortCircuit === true && <span class="sc-tag">shortCircuit</span>}
              </div>
              {e.decision !== undefined && (
                <div class="muted">decision={String(e.decision)}</div>
              )}
              {Array.isArray(e.spans) && e.spans.length > 0 && (
                <ul class="span-list">
                  {e.spans.map((s, j) => (
                    <li key={`span-${s.target ?? ""}-${s.index ?? j}-${s.role ?? j}`}>
                      {s.target}
                      {typeof s.index === "number" ? `[${s.index}]` : ""}
                      {s.role ? `:${s.role}` : ""}
                    </li>
                  ))}
                </ul>
              )}
              <details class="raw">
                <summary>raw</summary>
                <pre>{JSON.stringify(e, null, 2)}</pre>
              </details>
            </li>
          ))}
        </ol>
      )}
    </div>
  );
}
