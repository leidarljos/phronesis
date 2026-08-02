# GrokOS policyd Cap'n API (TCB language).
# Source of truth: trace-analysis/grokos-packages/grokos-schema
#
# Role
# ----
# Product API is interface Policyd: one method per domain, Cap'n always linked.
# Each method is zero-copy mappable Cap'n messages (params root in, result root
# out). No CallEnvelope. No ok|err unions. No tool×action free Text.
#
# Speakers: grok-policyd (TCB); sessiond / grokos-agent / grokos-shell.
# Seat Cap'n (session.capnp) is a separate server — do not fold Goal here.
#
# Trust
# -----
# - Identity: Util.AgentId only (fixed-width). Never Text hex on the wire.
# - Path checks: lexical only (no realpath). Overlong text → fail-closed deny.
# - Policy outcome is always PolicyDecision (deny | allow | prompt). Protocol /
#   parse failures are Decision.deny with a reason (fail closed) — not a second
#   error channel.
# - TCB does not authenticate agentId; seat joins it to GROKOS_RUN_ID by convention.
#
# Design
# ------
# Separate methods beat unions: invalid cross-domain combos are unrepresentable
# because they are different entry points. Shared result type is PolicyDecision.
# Shell content is argv : List(Text) on checkShell — never a shell string.
#
# Evolution
# ---------
# Prefer new methods over reshaping existing ones. deprecated* until migrate.

@0xe2859f3833215a0b;

using Util = import "util.capnp";

# ==============================================================================
# Shared values
# ==============================================================================

enum Decision {
  # Policy outcome. Always the method result's decision field.
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
  # High-risk → Decision.prompt.
  delete @2;
}

enum RiskAction {
  network @0;
  secretExport @1;
  sudo @2;
  pay @3;
  auth @4;
  osChange @5;
  privilege @6;
}

enum AgentState {
  # Process slot only (not Util.RunState).
  stopped @0;
  running @1;
  failed @2;
  # Slot missing / query invalid (fail-closed status).
  missing @3;
}

struct PolicyDecision {
  # Sole result type for every check/admit method.
  decision @0 :Decision;
  reason @1 :Text;
  # Short human reason. Protocol failures use deny + reason (fail closed).
  agentId @2 :Util.AgentId;
  # Echo of the checked agent (zero if unset / invalid).
}

struct PolicydStatus {
  version @0 :Text;
  apiVersion @1 :Int32;
  stateDir @2 :Text;
  runtimeDir @3 :Text;
  ready @4 :Bool;
  # False only if supervisor handle is unusable.
}

struct AgentStatus {
  id @0 :Util.AgentId;
  state @1 :AgentState;
  # missing → other fields zero/empty.
  pid @2 :Int32;
  pgid @3 :Int32;
  exitStatus @4 :Int32;
  mode @5 :Util.SeatMode;
  workspace @6 :Text;
  hasCgroup @7 :Bool;
  detail @8 :Text;
  # Human note when state=missing or failed; empty otherwise.
}

# ==============================================================================
# Method params (flat structs — one method, one params type)
# ==============================================================================

struct SeatCheck {
  agentId @0 :Util.AgentId;
  action @1 :SeatAction;
}

struct ModelCheck {
  agentId @0 :Util.AgentId;
  # Open catalog / invoke id. Empty allowed.
  model @1 :Text;
}

struct PathCheck {
  agentId @0 :Util.AgentId;
  action @1 :PathAction;
  # Clean absolute path under workspace for allow. Empty → deny.
  path @2 :Text;
}

struct ShellCheck {
  agentId @0 :Util.AgentId;
  # Absolute cwd / target root for workspace allowlist.
  cwd @1 :Text;
  # spawn(2) argv. Empty = path-only (no content gate).
  # Non-empty: Python scripts require uv+run and PEP 723 on the .py file.
  argv @2 :List(Text);
}

struct RiskCheck {
  agentId @0 :Util.AgentId;
  action @1 :RiskAction;
  # Optional path context; may be empty.
  path @2 :Text;
}

struct AdmitSeat {
  agentId @0 :Util.AgentId;
  detail @1 :Text;
  # Log-only. Not a path. Not identity.
}

struct AdmitModel {
  agentId @0 :Util.AgentId;
  detail @1 :Text;
}

struct AgentQuery {
  agentId @0 :Util.AgentId;
}

# ==============================================================================
# interface Policyd — methods only (no unions)
# ==============================================================================

interface Policyd {
  # Cap'n always linked. C entry points mirror methods: Cap'n params message
  # root in, Cap'n result message root out (zero-copy mappable segments).
  #
  # Caller: sessiond, grokos-agent, grokos-shell.
  # Callee: grok-policyd. Same-uid / linked only.

  status @0 () -> PolicydStatus;
  # Snapshot of open supervisor.

  checkSeat @1 SeatCheck -> PolicyDecision;
  checkModel @2 ModelCheck -> PolicyDecision;
  checkPath @3 PathCheck -> PolicyDecision;
  checkShell @4 ShellCheck -> PolicyDecision;
  checkRisk @5 RiskCheck -> PolicyDecision;

  admitSeat @6 AdmitSeat -> PolicyDecision;
  # Sugar for checkSeat(publishRun).
  admitModel @7 AdmitModel -> PolicyDecision;
  # Sugar for checkModel(empty model). Legacy "agent" admit maps here.

  agentStatus @8 AgentQuery -> AgentStatus;
}
