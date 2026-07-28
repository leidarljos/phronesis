# GrokOS policyd Cap'n language (product policy plane).
#
# Callers compose these bodies and invoke grok_policyd_handle_capnp (in-process
# FFI). This is NOT the seat bus (session.capnp / nng on sessiond.sock).
# Schema SoT for this package; embedders pin a copy under vendor/schema or
# crates/grokos-schema/schema.

@0xe2859f3833215a0b;

# Outer message on every FFI call. Exactly one of request|response is set.
struct PolicyEnvelope {
  # Wire generation; handlers require 1 today.
  protocolVersion @0 :UInt32 = 1;
  # Optional correlation id (may be empty). Echoed for logs; not auth.
  traceId @1 :Text;
  body :union {
    # Caller → TCB.
    request @2 :PolicyRequest;
    # TCB → caller.
    response @3 :PolicyResponse;
  }
}

# Discriminated op; unknown arms must fail closed at the handler.
struct PolicyRequest {
  op :union {
    # Liveness / version / state+runtime paths (no agent id).
    status @0 :Void;
    # Tool/action/path evaluation (maps grok_policy_eval / path allowlist).
    check @1 :PolicyCheck;
    # Pre-work admit plane (model start, seat publish, …) via kind string.
    admit @2 :PolicyAdmit;
    # Multi-agent TCB probe (maps grok_supervisor_status for one id).
    agentStatus @3 :AgentQuery;
  }
}

# check: evaluate whether (tool, action, path) is allowed for agentId.
struct PolicyCheck {
  # Caller-chosen agent identity (not peercred). Sessiond uses seat agent id.
  agentId @0 :Text;
  # Logical tool name: "seat", "model", or path-policy tools.
  tool @1 :Text;
  # Action under tool: e.g. publish_run, read_run, start, read, write.
  action @2 :Text;
  # Optional path or run id; empty when unused. Lexical path policy only.
  path @3 :Text;
}

# admit: coarse plane before model/agent work (not a full path check).
struct PolicyAdmit {
  # Same agent identity convention as PolicyCheck.agentId.
  agentId @0 :Text;
  # Kind → tool/action map (see grok_policyd_map_admit_kind):
  #   "seat" → seat/publish_run
  #   "model"|"agent"|"" → model/start
  #   anything else → fail-closed PolicyError
  kind @1 :Text;
  # Free-form detail for logs (not used as a path).
  detail @2 :Text;
}

# agentStatus: look up one supervised agent slot.
struct AgentQuery {
  agentId @0 :Text;
}

# Response ok union; exactly one arm. error is for protocol/TCB failures.
struct PolicyResponse {
  ok :union {
    status @0 :PolicydStatus;
    # Decision for check.
    check @1 :PolicyDecision;
    # Decision for admit (same shape as check).
    admit @2 :PolicyDecision;
    agentStatus @3 :AgentStatusWire;
    # Fail-closed protocol / mapping / buffer errors (not a soft deny).
    error @4 :PolicyError;
  }
}

# Snapshot of the open supervisor (paths are process-local).
struct PolicydStatus {
  version @0 :Text;       # package version string
  apiVersion @1 :Int32;   # GROK_POLICYD_API_VERSION
  stateDir @2 :Text;
  runtimeDir @3 :Text;
  # Historical field: product has no policyd.sock. Handlers set "ffi".
  socket @4 :Text;
  # True when supervisor open + ready for handle_capnp.
  ready @5 :Bool;
}

# Policy outcome. deny/allow/prompt; seat skeleton treats prompt like allow
# at some callers — check product caller, not this enum alone.
enum Decision {
  deny @0;
  allow @1;
  prompt @2;
}

struct PolicyDecision {
  decision @0 :Decision;
  # Human-readable reason (TCB buffers are fixed-size; overlong Cap'n text fails closed).
  reason @1 :Text;
  agentId @2 :Text;
  tool @3 :Text;
  action @4 :Text;
}

enum AgentStateWire {
  stopped @0;
  running @1;
  failed @2;
}

struct AgentStatusWire {
  id @0 :Text;
  state @1 :AgentStateWire;
  pid @2 :Int32;
  pgid @3 :Int32;
  # Wait-style exit status when stopped/failed; 0 if still running.
  exitStatus @4 :Int32;
  mode @5 :Text;
  workspace @6 :Text;
  hasCgroup @7 :Bool;
}

# Protocol / TCB error (not Decision.deny). code is grok errno-style.
struct PolicyError {
  code @0 :Int32;
  message @1 :Text;
}
