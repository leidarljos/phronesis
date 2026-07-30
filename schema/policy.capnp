# GrokOS policyd Cap'n language.
# Product API: grok_policyd_handle_capnp(sup, request_body, …) with PolicyEnvelope
# messages. Schema SoT; embedders pin a copy for Rust/C codegen.

@0xe2859f3833215a0b;

# Top-level FFI message. Set body to request (caller→TCB) or response (TCB→caller).
struct PolicyEnvelope {
  # Envelope version. Handler requires 1.
  protocolVersion @0 :UInt32 = 1;
  # Optional correlation id for logs (may be empty). Not authorization.
  traceId @1 :Text;
  body :union {
    # Inbound op (status | check | admit | agentStatus).
    request @2 :PolicyRequest;
    # Outbound result (status | check | admit | agentStatus | error).
    response @3 :PolicyResponse;
  }
}

# Inbound op. Exactly one arm of op.
struct PolicyRequest {
  op :union {
    # Supervisor open snapshot: version, apiVersion, paths, ready.
    status @0 :Void;
    # Path/tool policy evaluation → PolicyDecision on response.check.
    check @1 :PolicyCheck;
    # Coarse admit by kind string → PolicyDecision on response.admit.
    # kind map: "seat"→seat/publish_run; "model"|"agent"|""→model/start;
    # other kind → response.error fail-closed.
    admit @2 :PolicyAdmit;
    # Look up one agent slot → response.agentStatus (or error if missing).
    agentStatus @3 :AgentQuery;
  }
}

# check arm payload.
struct PolicyCheck {
  # Agent identity string chosen by the caller (e.g. seat run id).
  agentId @0 :Text;
  # Tool namespace: "seat", "model", "fs", "shell", or other path-policy tools.
  tool @1 :Text;
  # Action under tool: publish_run|read_run|list_runs|list_events|start|read|write|
  # exec|…  High-risk action names (delete, network, sudo, …) → Decision.prompt.
  action @2 :Text;
  # Optional absolute path; empty when unused. Path checks are lexical only
  # (no realpath). For tool=shell action=exec: absolute cwd or target root of the
  # proposed command — must sit under the agent workspace for Decision.allow.
  # Argv is not on this wire in Track 1 (meta #88).
  path @3 :Text;
}

# admit arm payload.
struct PolicyAdmit {
  # Same identity convention as PolicyCheck.agentId.
  agentId @0 :Text;
  # Admit kind (see PolicyRequest.op.admit). Unknown kinds fail closed.
  kind @1 :Text;
  # Free-form log detail; not used as a filesystem path.
  detail @2 :Text;
}

# agentStatus arm payload.
struct AgentQuery {
  # Agent id to query via grok_supervisor_status.
  agentId @0 :Text;
}

# Outbound result. Exactly one arm of ok.
struct PolicyResponse {
  ok :union {
    # Filled for status.
    status @0 :PolicydStatus;
    # Filled for check.
    check @1 :PolicyDecision;
    # Filled for admit (same shape as check).
    admit @2 :PolicyDecision;
    # Filled for agentStatus.
    agentStatus @3 :AgentStatusWire;
    # Protocol/TCB failure (not Decision.deny). Unknown op, bad fields, etc.
    error @4 :PolicyError;
  }
}

# status arm payload: open supervisor snapshot.
struct PolicydStatus {
  # Package version string (GROK_POLICYD_VERSION).
  version @0 :Text;
  # ABI generation (GROK_POLICYD_API_VERSION).
  apiVersion @1 :Int32;
  # Resolved state directory.
  stateDir @2 :Text;
  # Resolved runtime directory.
  runtimeDir @3 :Text;
  # Legacy field name. Handler sets the literal "ffi" (in-process Cap'n).
  socket @4 :Text;
  # True when the supervisor handle is open and ready for further ops.
  ready @5 :Bool;
}

# Decision enum for PolicyDecision.decision.
enum Decision {
  # Hard deny.
  deny @0;
  # Allowed.
  allow @1;
  # Needs confirm. Some seat callers treat prompt as allow; check the caller.
  prompt @2;
}

# Shared decision body for check and admit responses.
struct PolicyDecision {
  # Outcome enum.
  decision @0 :Decision;
  # Human-readable reason. Cap'n text longer than TCB buffers fails closed.
  reason @1 :Text;
  # Echo of agent id used for the decision.
  agentId @2 :Text;
  # Echo of tool (after admit.kind mapping when from admit).
  tool @3 :Text;
  # Echo of action (after admit.kind mapping when from admit).
  action @4 :Text;
}

# Process state for AgentStatusWire.state.
enum AgentStateWire {
  stopped @0;
  running @1;
  failed @2;
}

# agentStatus arm payload.
struct AgentStatusWire {
  # Agent id.
  id @0 :Text;
  # Running / stopped / failed.
  state @1 :AgentStateWire;
  # Process id when running; 0 otherwise.
  pid @2 :Int32;
  # Process group id when running; 0 otherwise.
  pgid @3 :Int32;
  # Wait-style exit status when stopped/failed; 0 if still running.
  exitStatus @4 :Int32;
  # Supervisor mode string (e.g. develop).
  mode @5 :Text;
  # Workspace path if set.
  workspace @6 :Text;
  # True when a cgroup path is attached for the agent.
  hasCgroup @7 :Bool;
}

# error arm payload (protocol / TCB errors, not policy deny).
struct PolicyError {
  # grok errno-style code (GROK_ERR_*).
  code @0 :Int32;
  # Short message for logs.
  message @1 :Text;
}
