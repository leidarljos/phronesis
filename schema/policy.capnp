# GrokOS policyd Cap'n API (TCB FFI language).
# Source of truth: trace-analysis/grokos-packages/grokos-schema
#
# Role
# ----
# Product API is grok_policyd_handle_capnp(sup, request_body, …) with
# PolicyEnvelope messages. In-process only (static link / FFI). There is no
# policyd.sock product peer for seat/model admit.
#
# Speakers: grok-policyd tests/CLI; sessiond / grokos-agent / grokos-shell
# embed the same schema for Cap'n check/admit.
#
# Trust
# -----
# agentId on this FFI is Text holding the 32-char lowercase hex of
# Util.AgentId (xxh3-128). It is NOT free-form open content / labels.
# The TCB does not authenticate agentId; seat plane joins it to GROKOS_RUN_ID
# by convention. Path checks are lexical only (no symlink resolution).
# Fail closed on overlong Cap'n text relative to internal C buffers.
#
# Evolution
# ---------
# protocolVersion must be 1. Prefer new union arms over reusing ordinals.
# deprecated fields stay until all embedders migrate (Sandstorm style).

@0xe2859f3833215a0b;

using Util = import "util.capnp";

# ------------------------------------------------------------------------------
# Envelope
# ------------------------------------------------------------------------------

struct PolicyEnvelope {
  # Top-level FFI message. Set body to request (caller→TCB) or response (TCB→caller).

  protocolVersion @0 :UInt32 = 1;
  # Handler requires 1.

  traceId @1 :Text;
  # Optional correlation for logs. Not authorization. May be empty.

  body :union {
    request @2 :PolicyRequest;
    response @3 :PolicyResponse;
  }
}

# ------------------------------------------------------------------------------
# Request
# ------------------------------------------------------------------------------

struct PolicyRequest {
  # Exactly one op.

  op :union {
    status @0 :Void;
    # Supervisor open snapshot → PolicydStatus.

    check @1 :PolicyCheck;
    # Path/tool policy evaluation → PolicyDecision on response.check.

    admit @2 :PolicyAdmit;
    # Coarse admit by kind → PolicyDecision on response.admit.
    # Mapping (fail-closed on unknown kind):
    #   seat  → tool=seat,  action=publish_run (or SeatAction via check)
    #   model → tool=model, action=start
    #   agent → tool=model, action=start (alias)
    #   ""    → tool=model, action=start (legacy empty)

    agentStatus @3 :AgentQuery;
    # Look up one agent slot → response.agentStatus (or error if missing).
  }
}

struct PolicyCheck {
  # Full tool/action/path check (seat, model, or filesystem tools).

  agentId @0 :Text;
  # 32-hex of Util.AgentId (or empty). Not a free label.

  tool @1 :Text;
  # Tool namespace. Canonical closed values used by product:
  #   "seat"  — seat board ops (see seatAction)
  #   "model" — model process admit
  #   "fs"|"shell"|… — path policy (open set for tools)
  # Prefer exact tokens above; unknown tools fail closed or deny per TCB.

  action @2 :Text;
  # Action under tool. Canonical seat actions: publish_run, list_runs, read_run,
  # list_events. Canonical model action: start. Filesystem: read, write, exec, …

  path @3 :Text;
  # Optional path or run id; empty when unused. Path checks are lexical only.
}

struct PolicyAdmit {
  # Coarse admit. Prefer check() when tool/action are known.

  agentId @0 :Text;
  # 32-hex of Util.AgentId (or empty). Not a free label.
  kind @1 :Text;
  # Admit kind string (see PolicyRequest.op.admit mapping). Unknown → error.
  detail @2 :Text;
  # Free-form log detail; not used as a filesystem path.
}

struct AgentQuery {
  agentId @0 :Text;
  # 32-hex of Util.AgentId to query via grok_supervisor_status.
}

# ------------------------------------------------------------------------------
# Response
# ------------------------------------------------------------------------------

struct PolicyResponse {
  # Exactly one ok arm.

  ok :union {
    status @0 :PolicydStatus;
    check @1 :PolicyDecision;
    admit @2 :PolicyDecision;
    # Same shape as check.
    agentStatus @3 :AgentStatusWire;
    error @4 :PolicyError;
    # Protocol/TCB failure (not Decision.deny). Unknown op, bad fields, etc.
  }
}

struct PolicydStatus {
  version @0 :Text;
  # Package version string (GROK_POLICYD_VERSION).
  apiVersion @1 :Int32;
  # ABI generation (GROK_POLICYD_API_VERSION).
  stateDir @2 :Text;
  runtimeDir @3 :Text;
  socket @4 :Text;
  # Legacy field name. Handler sets the literal "ffi" (in-process Cap'n).
  # Not a filesystem path to a daemon. Prefer ignoring for product control.
  ready @5 :Bool;
  # True when the supervisor handle is open and ready for further ops.
}

enum Decision {
  # Outcome of PolicyDecision.decision.
  deny @0;
  # Hard deny.
  allow @1;
  # Allowed.
  prompt @2;
  # Needs confirm. Some seat callers treat prompt as allow; check the caller.
}

struct PolicyDecision {
  decision @0 :Decision;
  reason @1 :Text;
  # Human-readable reason. Overlong Cap'n text vs TCB buffers fails closed.
  agentId @2 :Text;
  # Echo of agent id (32-hex AgentId) used for the decision.
  tool @3 :Text;
  # Echo of tool (after admit.kind mapping when from admit).
  action @4 :Text;
  # Echo of action (after admit.kind mapping when from admit).
}

enum AgentStateWire {
  # Process state for AgentStatusWire.state.
  stopped @0;
  running @1;
  failed @2;
}

struct AgentStatusWire {
  id @0 :Text;
  # 32-hex of Util.AgentId (C buffer boundary; not free-form identity).
  state @1 :AgentStateWire;
  pid @2 :Int32;
  # Process id when running; 0 otherwise.
  pgid @3 :Int32;
  # Process group id when running; 0 otherwise.
  exitStatus @4 :Int32;
  # Wait-style exit status when stopped/failed; 0 if still running.
  mode @5 :Text;
  # Supervisor mode string (e.g. develop). Prefer Util.SeatMode names when set.
  workspace @6 :Text;
  hasCgroup @7 :Bool;
  # True when a cgroup path is attached for the agent.
}

struct PolicyError {
  # Protocol / TCB errors, not policy deny (see Decision.deny).

  code @0 :Int32;
  # grok errno-style code (GROK_ERR_*).
  message @1 :Text;
  # Short message for logs.
}
