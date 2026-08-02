# GrokOS policyd Cap'n API (TCB language).
# Source of truth: trace-analysis/grokos-packages/grokos-schema
#
# Role
# ----
# Product surface is interface Policyd (methods + typed results). Callers
# (sessiond, agent, shell) and the TCB speak the same methods; transport is
# in-process FFI today (static link). There is no policyd.sock peer.
#
# Seat Cap'n (session.capnp / nng) is a separate server. Do not fold seat Goal
# into this interface.
#
# Trust
# -----
# - Identity: Util.AgentId / Util.TraceId only (fixed-width). Never Text hex
#   identity on the wire (util.capnp). CLI 32-hex is off-wire display only.
# - Path checks: lexical only (no realpath). Overlong Cap'n text → Err.
# - Decision is a *return value*, not a C errno. Protocol failure is Err, not
#   Decision.deny.
#
# Design
# ------
# 1. interface methods are the API (Sandstorm-style). Params/Results are the
#    wire structs (c-capnproto does not emit RPC stubs; methods still define
#    the product contract and the shapes implementers encode).
# 2. Check body is a domain union (seat|model|path|shell|risk) — invalid
#    tool×action pairs are unrepresentable.
# 3. Shell content is ShellOp.argv : List(Text), never a shell command string.
# 4. No parallel free-Text tool/action vocabulary.
#
# Evolution
# ---------
# Prefer new methods / union arms over renumber. deprecated* stays until
# embedders migrate. protocolVersion on CallEnvelope must be 1.

@0xe2859f3833215a0b;

using Util = import "util.capnp";

# ==============================================================================
# Shared value types
# ==============================================================================

enum Decision {
  # Policy outcome. Always a method result field — never a C return code.
  deny @0;
  allow @1;
  # Confirm class. Product hard seats may fail-closed on prompt.
  prompt @2;
}

enum SeatAction {
  publishRun @0;
  readRun @1;
  listRuns @2;
  listEvents @3;
}

enum PathAction {
  read @0;
  write @1;
  # TCB returns Decision.prompt.
  delete @2;
}

enum RiskAction {
  unset @0;
  network @1;
  secretExport @2;
  sudo @3;
  pay @4;
  auth @5;
  osChange @6;
  privilege @7;
}

enum AdmitKind {
  # Coarse admit → maps to a CheckBody arm (fail-closed on unset).
  unset @0;
  seat @1;   # → seat = publishRun
  model @2;  # → model start
  agent @3;  # alias of model
}

enum AgentState {
  # Process slot state (not Util.RunState).
  stopped @0;
  running @1;
  failed @2;
}

enum ErrCode {
  # Protocol / TCB failure codes (method result Err.code). Not Decision.
  unset @0;
  inval @1;
  notFound @2;
  io @3;
  state @4;
  internal @5;
}

struct ModelOp {
  # Open model catalog / invoke id. Empty allowed.
  model @0 :Text;
}

struct PathOp {
  action @0 :PathAction;
  # Clean absolute path under agent workspace for allow. Empty → deny.
  path @1 :Text;
}

struct ShellOp {
  # Absolute cwd / target root for workspace allowlist.
  cwd @0 :Text;
  # spawn(2) argv. Empty = path-only (no content gate).
  # Non-empty: product content gates (Python → uv run + PEP 723 on .py).
  # Caller supplies argv; TCB does not shell-tokenize a string.
  argv @1 :List(Text);
}

struct RiskOp {
  action @0 :RiskAction;
  path @1 :Text;  # optional context; may be empty
}

struct CheckBody {
  # Exactly one domain op. This is the closed product law for checks.
  union {
    seat @0 :SeatAction;
    model @1 :ModelOp;
    path @2 :PathOp;
    shell @3 :ShellOp;
    risk @4 :RiskOp;
  }
}

struct PolicyDecision {
  decision @0 :Decision;
  reason @1 :Text;
  # Short human reason. Not a second protocol.
  agentId @2 :Util.AgentId;
  # Echo of the checked agent.
}

struct AgentStatus {
  id @0 :Util.AgentId;
  state @1 :AgentState;
  pid @2 :Int32;
  pgid @3 :Int32;
  exitStatus @4 :Int32;
  mode @5 :Util.SeatMode;
  workspace @6 :Text;
  hasCgroup @7 :Bool;
}

struct PolicydStatus {
  version @0 :Text;
  apiVersion @1 :Int32;
  stateDir @2 :Text;
  runtimeDir @3 :Text;
  ready @4 :Bool;
  # Always true when the supervisor handle is open for further methods.
}

struct Err {
  # Method-level failure. Distinct from Decision.deny.
  code @0 :ErrCode;
  message @1 :Text;
}

# ==============================================================================
# Method params / results (typed returns — not int out-params)
# ==============================================================================

struct StatusResults {
  union {
    ok @0 :PolicydStatus;
    err @1 :Err;
  }
}

struct CheckParams {
  agentId @0 :Util.AgentId;
  # Zero = no workspace bind (path/shell fail closed).
  body @1 :CheckBody;
  # Required. Unset body → Err.inval.
}

struct CheckResults {
  # check() and admit() both return this.
  union {
    ok @0 :PolicyDecision;
    err @1 :Err;
  }
}

struct AdmitParams {
  agentId @0 :Util.AgentId;
  kind @1 :AdmitKind;
  # unset → Err.inval. Prefer check() when the domain is known.
  detail @2 :Text;
  # Log-only. Not a path. Not identity.
}

struct AgentStatusParams {
  agentId @0 :Util.AgentId;
  # Zero → Err.inval.
}

struct AgentStatusResults {
  union {
    ok @0 :AgentStatus;
    err @1 :Err;
  }
}

# ==============================================================================
# Product API — methods with typed results
# ==============================================================================

interface Policyd {
  # In-process TCB capability. Host holds a supervisor; methods evaluate against
  # that supervisor's agent slots / action log.
  #
  # Caller: sessiond, grokos-agent, grokos-shell (linked libgrok_policyd).
  # Callee: grok-policyd implementation of these methods.
  # Never expose across uid boundaries without a new trust review.
  #
  # Every method returns a Results union: ok = domain value, err = protocol/TCB
  # failure. Policy deny/allow/prompt is always inside ok : PolicyDecision.

  status @0 () -> StatusResults;
  # Snapshot of open supervisor (version, dirs, ready).

  check @1 CheckParams -> CheckResults;
  # Full domain check (seat / model / path / shell / risk).

  admit @2 AdmitParams -> CheckResults;
  # Coarse sugar over check. Mapping (fail-closed):
  #   seat  → CheckBody.seat = publishRun
  #   model → CheckBody.model (empty model text)
  #   agent → same as model

  agentStatus @3 AgentStatusParams -> AgentStatusResults;
  # Process-slot snapshot for one agentId.
}

# ==============================================================================
# Transport envelope (FFI bytes in/out for hosts without Cap'n RPC runtime)
# ==============================================================================
#
# c-capnproto does not generate interface stubs. Callers pack CallEnvelope
# request / response around the method params/results above. Cap'n-RPC hosts
# may ignore this envelope and call interface Policyd directly.
#
# Method ordinals match interface Policyd.

struct CallEnvelope {
  protocolVersion @0 :UInt32 = 1;
  # Must be 1.

  traceId @1 :Util.TraceId;
  # Log correlation only. Zero = unset.

  body :union {
    # --- requests (caller → TCB) ---
    status @2 :Void;
    check @3 :CheckParams;
    admit @4 :AdmitParams;
    agentStatus @5 :AgentStatusParams;

    # --- responses (TCB → caller); same ordinals as method results ---
    statusResults @6 :StatusResults;
    checkResults @7 :CheckResults;
    # admit shares CheckResults shape
    admitResults @8 :CheckResults;
    agentStatusResults @9 :AgentStatusResults;
  }
}
