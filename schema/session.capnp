# GrokOS session plane Cap'n API (seat bus).
# Source of truth: trace-analysis/grokos-packages/grokos-schema
#
# Role
# ----
# sessiond is the Cap'n **SERVER** (nng rep on a Unix domain socket).
# Clients: `grokos` CLI, `grokos-agent`, `grokos-shell` (and tests).
# Agents are never Cap'n servers. Peers snoop farm / voice state only via this
# bus (listAgents / getRun / listRunEvents / status.voice / watchVoiceEvents),
# not via model stdout or raw audio.
#
# Trust
# -----
# Transport ACL is same-uid peercred on the UDS (nng). Schema fields are not
# a second ACL: a same-uid peer can forge parentId/resultTail; policyd seat
# checks gate publish/list / voice arm+inject when the TCB is linked.
# Voice plane carries text + metadata only — never PCM, secrets, or API keys.
# confidence is not authorization for shell/exec or Goal admit.
#
# Evolution
# ---------
# protocolVersion must be 1 for this file generation. 0.x may break between
# tags; pin consumers by tag / SCHEMA_PIN / vendored schema copy. Prefer new
# optional fields over reusing ordinals. See SCHEMA_STYLE.md (Sandstorm-derived).

@0xa8f3c2e91b704d15;

using Util = import "util.capnp";

struct Envelope {
  # Top-level frame on the seat bus. Exactly one of request/response/event.

  protocolVersion @0 :UInt32 = 1;
  # Must be 1. Servers reject other values.

  traceId @1 :Util.TraceId;
  # Farm / multi-agent correlation (fixed-width hash, not Text).
  # Zero is allowed for non-farm ops (ping, mode, doctor).

  body :union {
    request @2 :Request;
    response @3 :Response;
    event @4 :SessionEvent;
  }
}

struct Request {
  # Exactly one op. Unknown arms must not be invented by clients.

  op :union {
    ping @0:Void;
    # Liveness. Response: pong.

    status @1:Void;
    # Aggregate seat snapshot. Response: status.

    getMode @2:Void;
    # Current SeatMode. Response: mode.

    setMode @3:SetMode;
    # Set product mode. Response: mode (echo).

    doctor @4:Void;
    # Hard/soft check report. Response: doctor.

    shutdown @5:Void;
    # Graceful sessiond exit. Response: bye.

    # Seat run board (multi-peer visibility). sessiond retains a bounded board.
    # Any same-uid client may list/get/events after peers publish via reportAgent.

    reportAgent @6:ReportAgent;
    # Upsert one board row. Gated by peercred + optional policyd seat check.

    listAgents @7:Void;
    # Full board snapshot. Response: agentsList.

    getRun @8:GetRun;
    # One row by id. Response: runDetail or error.

    listRunEvents @9:ListRunEvents;
    # Retained event trail. Response: runEvents.

    # ------------------------------------------------------------------
    # Subagent plane — Cap'n RPC = stock SubagentBackend / SubagentEvent,
    # only remote (sessiond SERVER owns the single registry).
    #
    # Local (stock grok-build, one process):
    #   ChannelBackend → SubagentCoordinator (same procedure names/semantics).
    # Seated (GrokOS):
    #   Cap'n client → these Request arms → sessiond (no second coordinator).
    #
    # Task tool / get_task_output / kill_task / between-turn Completions and
    # Outstanding MUST call these procedures. Do not invent parallel "farm"
    # product APIs; CLI farm/spawn is only sugar over the same RPCs.
    # policyd authorizes; never model HTTP payloads on this bus.
    # ------------------------------------------------------------------

    spawnAgent @10:SpawnAgent;
    # = SubagentBackend::spawn / SubagentEvent::Spawn (admit half).
    # sessiond mints AgentId + registry row only. Does NOT exec the child;
    # admitter-local parent launches product agent (same as stock: coordinator
    # accepts, ChildRunner runs elsewhere). Response: agentAdmitted immediately.

    getAgent @11:GetAgent;
    # = SubagentBackend::query (non-blocking snapshot) / Inspect body.
    # One agent by canonical id. Response: agentDetail.

    listChildren @12:ListChildren;
    # = ListActive / ListRunning (children of parent). Response: childrenList.

    watchAgentEvents @13:WatchAgentEvents;
    # Join / progress stream (stock blocking Query(block=true) long-poll).
    # Events for agentId with seq > sinceSeq. Response: agentEvents.

    sendAgentMessage @14:SendAgentMessage;
    # Parent/sibling → agent mailbox (sessiond holds queue). Response: messageAccepted.
    # Receiver drains via querySubagent / watch — same process would use in-memory
    # waiters; RPC keeps the queue in sessiond.

    cancelAgent @15:CancelAgent;
    # = SubagentBackend::cancel / SubagentEvent::Cancel(SubagentId).
    # Best-effort process stop + confirmed terminal on registry.
    # Response: agentCancelled.

    reportAgentProgress @16:ReportAgentProgress;
    # Child → registry progress (stock ActiveChild progress). Response: progressAck.

    completeAgent @17:CompleteAgent;
    # Child → terminal result (stock finish_child). Response: completeAck.

    # ------------------------------------------------------------------
    # Workspace plane (herdr-class pane/tab topology on the Cap'n bus).
    # Mux geometry lives in grokos-shell-engine; sessiond is the seat registry
    # so peers (shell CLI, agent, MCP) share stable pane↔agent bindings.
    # ------------------------------------------------------------------

    reportPane @18:ReportPane;
    # Upsert one pane row (shell product after list/split/start). Response: paneDetail.

    listPanes @19:ListPanes;
    # Snapshot of pane registry. Response: panesList.

    getPane @20:GetPane;
    # One pane by id (+ optional workspace). Response: paneDetail or error.

    releasePane @21:ReleasePane;
    # Drop registry row when pane closed. Response: bye (void ok).

    # ------------------------------------------------------------------
    # Work DAG plane (shared plan/goal/task graph; sessiond sole mutator).
    # Multi-process agents speak Cap'n only. Store: in-memory now;
    # Valkey/Redis via systemd later. No Dolt. Peers list/claim without
    # reading private session transcripts.
    # ------------------------------------------------------------------

    upsertWork @22:UpsertWork;
    # Create or refresh a work node. Response: workDetail.

    listWork @23:ListWork;
    # Snapshot of work nodes (optional status filter). Response: workList.

    getWork @24:GetWork;
    # One work node by id. Response: workDetail or error.

    claimWork @25:ClaimWork;
    # CAS claim Ready/Todo→Claimed. Response: workClaimed or error.

    completeWork @26:CompleteWork;
    # Terminal status (done|failed|cancelled). Response: workDetail.

    linkWork @27:LinkWork;
    # parent → child dependency edge. Response: workAck.

    verifyWork @28:Void;
    # Structural DAG verify (cycles, missing deps). Response: workAck or error.

    # ------------------------------------------------------------------
    # Voice plane (seat listen + STT text; Cap'n control plane only).
    # Product path: arm → capture/VAD/STT (sessiond) → partial/final text.
    # Peers snoop via status.voice, voiceStatus, and watchVoiceEvents.
    # No raw PCM / Data audio frames on this bus (debug dumps stay off-wire).
    # Same-uid peercred ACL only; policyd gates arm/inject (Track E).
    # Text length fail-closed: implementer policy (see VoiceSize); not schema consts.
    # ------------------------------------------------------------------

    voiceStatus @29:Void;
    # Snapshot VoiceListenState. Response: voice.
    # Equivalent to status.voice for CLI/doctor; does not arm or capture.

    voiceArm @30:VoiceArm;
    # Start listen mode (off by default). Response: voice (echo state).
    # Server may return error.denied (policy) or error.offline (audio down).
    # Does not select STT backend/model on the wire (sessiond pin / config).

    voiceDisarm @31:Void;
    # Stop listen / release capture. Response: voice.
    # Idempotent: already-disarmed is success with armed=false.

    watchVoiceEvents @32:WatchVoiceEvents;
    # Partial/final/armed/disarmed/error trail (long-poll friendly).
    # Response: voiceEvents. Primary stream path (not SessionEvent memfd).

    voicePushUtterance @33:VoicePushUtterance;
    # Harness/dev inject of transcript text (no mic). Response: voice.
    # Production seats should policy-deny (tool=voice action=inject).
    # Overlong text → error.invalid (do not truncate client injects).

    # ------------------------------------------------------------------
    # Pane geometry control (Cap'n product API only).
    # Law: agents / farm / tools speak Cap'n here — never shell out to
    # `grokos-shell pane …`, never invent engine IPC from agent-core.
    # sessiond is SERVER: admit + drive mux (shell product is sessiond's
    # adapter). Response shapes are the seat registry truth.
    # ------------------------------------------------------------------

    splitPane @34:SplitPane;
    # Create tiled pane (optional root command + optional agentId bind).
    # Response: paneDetail.

    focusPane @35:FocusPane;
    # Focus by paneId, or by direction when direction is set. Response: paneDetail or bye.

    closePane @36:ClosePane;
    # Close pane + release registry row. Response: bye.

    readPane @37:ReadPane;
    # Dump pane buffer. Response: paneBuffer.

    writePane @38:WritePane;
    # Write text (optional Enter). Response: bye.

    # ------------------------------------------------------------------
    # Subagent plane (continued) — procedures stock drains each turn that
    # were missing as Cap'n RPC (must not stay ChannelBackend-only).
    # Ordinals continue after writePane; additive for 0.3.x.
    # ------------------------------------------------------------------

    querySubagent @39:QuerySubagent;
    # = SubagentBackend::query (block + timeout_ms). Response: subagentSnapshot
    # or error.notFound. Prefer this over getAgent when block/timeout matter.

    drainSubagentCompletions @40:DrainSubagentCompletions;
    # = SubagentEvent::Completions (between-turn / idle reminder).
    # Returns terminal children for parent not in suppressIds, then marks them
    # drained on sessiond (one-shot surface, stock buffer_completions semantics).
    # Response: subagentCompletions.

    subagentOutstanding @41:SubagentOutstanding;
    # = SubagentEvent::Outstanding (turn freeze / usage incomplete).
    # Live + background children for parentAgentId + optional parentPromptId.
    # Response: subagentOutstandingReply.

    subagentRegistryCounts @42:Void;
    # = SubagentEvent::RegistryCounts. Response: subagentRegistryCounts.
    # Snapshot pending/active/completed under the caller's seat scope.

    # ------------------------------------------------------------------
    # Multi-process seat attach tokens (meta #126).
    # sessiond mints after a live hard prepare; clients present the token
    # on later tools. Not a same-UID stamp file. Not Policyd.admitSeat.
    # Cap'n Policyd remains tool allow/deny TCB after checkAdmit succeeds.
    # ------------------------------------------------------------------

    issueAdmit @43:IssueAdmit;
    # Mint a short-lived opaque token bound to this seat + workspace + mode.
    # Response: admitIssued. Fail closed if sessiond would reject hard prepare.

    checkAdmit @44:CheckAdmit;
    # Validate token (unknown / expired / revoked / mode mismatch → error).
    # Success may refresh expiry. Response: admitOk.

    revokeAdmit @45:RevokeAdmit;
    # Drop one token, or all when token is empty. Response: bye.
  }
}

# --- Typed agent farm (IDs are Util.AgentId hashes, not Text) ---------------

enum IsolationMode {
  # sessiond product default (process supervision + optional jail).
  default @0;
  # No extra isolation beyond process group.
  none @1;
  # Git worktree isolation when product supports it.
  worktree @2;
  # Process-only boundary (no worktree).
  process @3;
}

enum ReasoningEffort {
  # Closed model reasoning budget. unset = product/model default.
  unset @0;
  low @1;
  medium @2;
  high @3;
  max @4;
}

enum AgentRole {
  # Closed product roles for every agent (root, goal worker, task child).
  # Wire + board + coordinator use this only — never free-form Text for role.
  # Extend only with schema bump (no parallel string labels).
  unset @0;
  general @1;
  # Default / generic agent (CLI token "agent" maps here).
  explore @2;
  shell @3;
  plan @4;
  review @5;
  orchestrator @6;
  # Product seat interactive root (session shell / primary board agent).
  product @7;
}

enum ToolClass {
  # Capability bits for CapabilityPolicy.allowTools.
  unset @0;
  shell @1;
  fileRead @2;
  fileWrite @3;
  web @4;
  subagent @5;
  mcp @6;
}

enum NetworkPolicy {
  default @0;
  deny @1;
  allow @2;
}

enum FilesystemPolicy {
  default @0;
  readOnly @1;
  workspaceWrite @2;
}

enum AgentMessageKind {
  # Closed mailbox message kinds (SendAgentMessage).
  # Mirrors Hermes/OpenClaw handoff: parent sees summary/result, not full child trace.
  unset @0;
  user @1;
  system @2;
  tool @3;
  progress @4;
  control @5;
  # Child → parent structured completion handoff (OpenClaw announce / Hermes summary).
  result @6;
}

enum AgentEventKind {
  # Closed event kinds on WatchAgentEvents (OpenHands EventLog discipline).
  unset @0;
  lifecycle @1;
  progress @2;
  message @3;
  complete @4;
  cancel @5;
  # Child finished; parent can Watch without polling GetAgent (Magentic ledger style).
  childComplete @6;
}

enum TerminalReason {
  # Why the agent left the runnable set. Distinct from RunState.
  unset @0;
  completed @1;
  # Agent called completeAgent with success.
  failed @2;
  # Agent called completeAgent with failure, or process non-zero without complete.
  cancelledConfirmed @3;
  # CancelAgent killed process group/cgroup/scope and reaped death.
  spawnFailed @4;
  # sessiond could not start the process.
  killFailed @5;
  # Cancel signalled but termination not confirmed.
  alreadyTerminal @6;
  # Cancel/get against an already-terminal agent.
}

struct CapabilityPolicy {
  # Structured tool/skill policy. Not JSON-in-Text. Not secrets.
  allowTools @0 :List(ToolClass);
  # Empty list = product default tool set.
  network @1 :NetworkPolicy = default;
  filesystem @2 :FilesystemPolicy = default;
  # Open skill names (files/catalog keys) — Text is correct for open content.
  allowSkills @3 :List(Text);
}

struct ContextRef {
  # How the child session is seeded. Exactly one arm.
  union {
    fresh @0 :Void;
    # New conversation; no fork/resume.
    fork @1 :Util.AgentId;
    # Fork context from another agent id (non-zero).
    resume @2 :Util.AgentId;
    # Resume an existing agent/session id (non-zero).
  }
}

struct ExecutionBudget {
  maxTurns @0 :UInt32 = 8;
  # Model turn budget; servers may clamp.
  timeoutSecs @1 :UInt32 = 0;
  # Soft wall clock; 0 = none / product default.
  maxChildren @2 :UInt32 = 0;
  # Concurrent live children under this agent when it is a parent.
  # 0 = product default (sessiond: 3, Hermes-class). Orchestrator roles may raise.
  maxDepth @3 :UInt8 = 0;
  # Max tree depth from root for spawn under this agent. 0 = product default (1).
  # OpenCode subagent_depth / Hermes max_spawn_depth law.
}

struct SpawnAgent {
  # Full Task contract. sessiond allocates agentId when zero.

  agentId @0 :Util.AgentId;
  # Zero = sessiond mints (xxh3_128 of admit material). Non-zero = propose.

  parentId @1 :Util.AgentId;
  # Zero = root.

  rootId @2 :Util.AgentId;
  # Zero = parentId if non-zero else allocated agentId.

  traceId @3 :Util.TraceId;
  # Zero = sessiond mints farm correlation.

  role @4 :AgentRole = unset;
  # Closed product role.

  model @5 :Text;
  # Open model catalog id (e.g. grok-4.5-build). Empty = product default.
  # Text is correct: model names are open content, not protocol law.

  reasoningEffort @6 :ReasoningEffort = unset;

  cwd @7 :Text;
  # Absolute or product-relative path. Empty = sessiond cwd. Open path string.

  prompt @8 :Text;
  # Task prompt. Required non-empty. Open content.

  capability @9 :CapabilityPolicy;
  # Structured policy; not a JSON blob.

  context @10 :ContextRef;
  # fresh | fork(agent) | resume(agent).

  isolation @11 :IsolationMode = default;

  budget @12 :ExecutionBudget;

  depth @13 :UInt8 = 0;
  # Depth in farm tree. 0 = root (or unset; sessiond computes parent.depth+1).
  # Clients should leave 0; sessiond is authoritative.
}

struct GetAgent {
  agentId @0 :Util.AgentId;
  # Non-zero required.
}

struct ListChildren {
  parentId @0 :Util.AgentId;
  # Non-zero required.
}

struct WatchAgentEvents {
  agentId @0 :Util.AgentId;
  # Non-zero required.
  sinceSeq @1 :UInt64;
  # Return events with seq > sinceSeq.
  limit @2 :UInt32 = 200;
  # Servers may clamp.
  waitSecs @3 :UInt32 = 0;
  # 0 = return immediately; >0 long-poll up to waitSecs for new events.
}

struct SendAgentMessage {
  agentId @0 :Util.AgentId;
  # Destination (non-zero).
  fromId @1 :Util.AgentId;
  # Sender parent/sibling (non-zero).
  kind @2 :AgentMessageKind = unset;
  body @3 :Text;
  # Open message body. Not secrets / API keys.
}

struct CancelAgent {
  agentId @0 :Util.AgentId;
  # Non-zero required (unlike cancel all via zero cancelAgent).
  reason @1 :Text;
  # Human reason only; machine outcome is TerminalReason on response.
}

struct ReportAgentProgress {
  agentId @0 :Util.AgentId;
  # Must equal sessiond-allocated id for this supervised process.
  state @1 :Util.RunState;
  # Prefer starting|running|waiting|blocked.
  message @2 :Text;
  # Short human status (open).
  detail @3 :Text;
  # Tool milestone text (open). Prefer structured events later.
  pid @4 :Int32;
  # OS pid when known; 0 unknown.
  modelSession @5 :Data;
  # Opaque model conversation handle (bytes). Empty if unknown.
  # Not free-form Text identity.
}

struct CompleteAgent {
  agentId @0 :Util.AgentId;
  state @1 :Util.RunState;
  # Terminal: succeeded|failed|cancelled|done. blocked is not terminal.
  exitCode @2 :Int32;
  resultTail @3 :Text;
  # Final summary/stdout peel (open content).
  modelSession @4 :Data;
  # Opaque model conversation handle.
  message @5 :Text;
  # Human terminal message (open).
}

# --- Stock SubagentBackend procedures as Cap'n (remote coordinator) -----------

struct QuerySubagent {
  # = SubagentBackend::query / SubagentEvent::Query

  agentId @0 :Util.AgentId;
  # Non-zero child id.
  parentId @1 :Util.AgentId;
  # Zero = no parent scope filter; non-zero = only if parent matches.
  block @2 :Bool;
  # true = wait until terminal or timeoutMs (stock Query.block).
  timeoutMs @3 :UInt32 = 0;
  # 0 with block=false = immediate snapshot; 0 with block=true = product default.
}

struct DrainSubagentCompletions {
  # = SubagentEvent::Completions (between-turn drain)

  parentId @0 :Util.AgentId;
  # Parent AgentId (seat GROKOS_RUN_ID). Zero = invalid.
  suppressIds @1 :List(Util.AgentId);
  # Already-surfaced children (stock suppress_ids). Empty = none.
}

struct SubagentCompletionEntry {
  # Stock SubagentCompletionSummary on the wire.

  agentId @0 :Util.AgentId;
  subagentType @1 :Text;
  # Role/type token (open catalog string; maps to AgentRole when closed).
  description @2 :Text;
  success @3 :Bool;
  durationMs @4 :UInt64;
  toolCalls @5 :UInt32;
  turns @6 :UInt32;
  output @7 :Text;
  # Final output tail (open content).
}

struct SubagentCompletions {
  entries @0 :List(SubagentCompletionEntry);
}

struct SubagentOutstanding {
  # = SubagentEvent::Outstanding

  parentId @0 :Util.AgentId;
  # Seat parent AgentId (non-zero).
  parentPromptId @1 :Text;
  # Stock prompt_id scope; empty = all prompts under parent.
}

struct SubagentOutstandingReply {
  liveIds @0 :List(Util.AgentId);
  # Turn-blocking live children (stock live_ids).
  backgroundLive @1 :Bool;
  # Any background live child under scope.
  subagentUsageNotApplied @2 :Bool;
  # Sticky incomplete usage flag (stock).
}

struct SubagentRegistryCounts {
  pending @0 :UInt32;
  active @1 :UInt32;
  completed @2 :UInt32;
}

# --- Multi-process seat attach tokens (sessiond-issued; meta #126) ------------

struct IssueAdmit {
  # Mint after live sessiond + workspace bind (hard prepare).

  workspace @0 :Text;
  # Absolute workspace bind path. Required non-empty.
  agentId @1 :Util.AgentId;
  # Optional farm/run id (zero = seat-only attach, no spawnAgent).
}

struct CheckAdmit {
  token @0 :Text;
  # Opaque hex from admitIssued.token. Required non-empty.
  workspace @1 :Text;
  # Optional echo; when non-empty must match the issued workspace.
}

struct RevokeAdmit {
  token @0 :Text;
  # Non-empty = that token. Empty = revoke all tokens on this sessiond.
}

struct AdmitIssued {
  token @0 :Text;
  # Opaque hex (sessiond-only store). Not AgentId. Not a stamp file body.
  expiryUnix @1 :Util.UnixSecs;
  # Exclusive unix seconds. Product default TTL 900s unless sessiond config.
}

struct AdmitOk {
  expiryUnix @0 :Util.UnixSecs;
  # Current expiry (may be refreshed on check).
}

struct SubagentSnapshot {
  # Stock SubagentSnapshot for querySubagent response.

  agentId @0 :Util.AgentId;
  description @1 :Text;
  subagentType @2 :Text;
  state @3 :Util.RunState;
  # initializing≈admitted/starting; running; terminal succeeded|failed|cancelled.
  output @4 :Text;
  # Completed output or empty.
  error @5 :Text;
  # Failed/cancelled reason or empty.
  toolCalls @6 :UInt32;
  turns @7 :UInt32;
  durationMs @8 :UInt64;
  startedAtEpochMs @9 :UInt64;
}

struct AgentRecord {
  # Full queryable agent row for GetAgent.

  id @0 :Util.AgentId;
  parentId @1 :Util.AgentId;
  rootId @2 :Util.AgentId;
  traceId @3 :Util.TraceId;
  state @4 :Util.RunState;
  role @5 :AgentRole;
  model @6 :Text;
  reasoningEffort @7 :ReasoningEffort;
  cwd @8 :Text;
  capability @9 :CapabilityPolicy;
  context @10 :ContextRef;
  isolation @11 :IsolationMode;
  budget @12 :ExecutionBudget;
  pid @13 :Int32;
  startedUnix @14 :Util.UnixSecs;
  finishedUnix @15 :Util.UnixSecs;
  updatedUnix @16 :Util.UnixSecs;
  exitCode @17 :Int32;
  resultTail @18 :Text;
  terminalReason @19 :TerminalReason;
  message @20 :Text;
  depth @21 :UInt8;
  # Authoritative tree depth (0 = root).
}

struct AgentAdmitted {
  # SpawnAgent response — returns immediately; work is concurrent.

  agentId @0 :Util.AgentId;
  # Canonical id for all subsequent ops (dispatch, board, cancel, result).
  state @1 :Util.RunState;
  # admitted or starting.
  parentId @2 :Util.AgentId;
  rootId @3 :Util.AgentId;
  traceId @4 :Util.TraceId;
}

struct ChildrenList {
  parentId @0 :Util.AgentId;
  children @1 :List(AgentRecord);
}

struct AgentEvent {
  seq @0 :UInt64;
  # Monotonic per agentId (or per sessiond instance documented by server).
  tsUnix @1 :Util.UnixSecs;
  agentId @2 :Util.AgentId;
  kind @3 :AgentEventKind;
  state @4 :Util.RunState;
  message @5 :Text;
  # Open human text.
  detail @6 :Text;
  # Open detail.
  fromId @7 :Util.AgentId;
  # For message events; zero otherwise.
}

struct AgentEventsList {
  agentId @0 :Util.AgentId;
  events @1 :List(AgentEvent);
  nextSeq @2 :UInt64;
  # Client passes as sinceSeq on next WatchAgentEvents.
}

struct AgentCancelled {
  agentId @0 :Util.AgentId;
  state @1 :Util.RunState;
  # cancelled on confirmed kill; failed if kill-failed.
  terminalReason @2 :TerminalReason;
  exitCode @3 :Int32;
}

struct ReportAgent {
  # Client publish (upsert). Terminal fields meaningful when state is
  # done|cancelled|succeeded|failed. blocked is waiting-on-gate, not terminal.
  # Identity fields are Util.AgentId / TraceId (xxh3-128), never free Text.

  id @0 :Util.AgentId;
  # Stable run id (sessiond-minted or client-proposed non-zero). Zero invalid.

  role @1 :AgentRole = unset;
  # Closed product role (same AgentRole as SpawnAgent). Never Text.

  state @2 :Util.RunState;
  # Closed herdr-class state.

  message @3 :Text;
  # Short human status. Not a transcript store. Open content.

  pid @4 :Int32;
  # OS pid when known; 0 if unknown.

  sessionId @5 :Text;
  # Model/conversation session id when known. Empty ok. Open content
  # (provider handle), not GrokOS farm identity.

  parentId @6 :Util.AgentId;
  # Zero = root. Non-zero = child of that agent id (farm).

  resultTail @7 :Text;
  # Final stdout/summary peel for terminal states. Open content.

  exitCode @8 :Int32;
  # Process/product exit for terminal states; 0 if N/A.

  traceId @9 :Util.TraceId;
  # Echo Envelope.traceId or parent fan-out id. Zero = unset.

  surface @10 :Util.AgentSurface = unset;
  # Product UI surface (plan / question / board). Orthogonal to state.
  # Peers MUST read this enum — never sniff message Text for control flow.
}

struct AgentInfo {
  # Server board row (getRun / listAgents / status.agents).

  id @0 :Util.AgentId;
  role @1 :AgentRole = unset;
  # Closed product role (same AgentRole as SpawnAgent / ReportAgent). Never Text.
  state @2 :Util.RunState;
  message @3 :Text;
  pid @4 :Int32;
  updatedUnix @5 :Util.UnixSecs;
  # Last upsert time (Unix seconds).
  sessionId @6 :Text;
  # Model conversation id (open); not AgentId.
  parentId @7 :Util.AgentId;
  resultTail @8 :Text;
  exitCode @9 :Int32;
  traceId @10 :Util.TraceId;
  startedUnix @11 :Util.UnixSecs;
  # First observe time; 0 if unknown.
  finishedUnix @12 :Util.UnixSecs;
  # Terminal transition; 0 if not terminal.
  surface @13 :Util.AgentSurface = unset;
  # Product UI surface; same law as ReportAgent.surface.
}

struct AgentsList {
  agents @0 :List(AgentInfo);
}

struct GetRun {
  id @0 :Util.AgentId;
  # Board id to fetch. Zero is invalid.
}

struct ListRunEvents {
  runId @0 :Util.AgentId;
  # Zero = all retained events; else filter by agent id.
  sinceSeq @1 :UInt64;
  # Return events with seq > sinceSeq.
  limit @2 :UInt32 = 200;
  # Max events; servers may clamp.
}

struct RunEvent {
  seq @0 :UInt64;
  # Monotonic per sessiond instance.
  tsUnix @1 :Util.UnixSecs;
  source @2 :Text;
  # e.g. agent, goal, sessiond. Open labels.
  kind @3 :Text;
  # e.g. report, handoff. Open; not RunState.
  detail @4 :Text;
  runId @5 :Util.AgentId;
  # Zero when not bound to a run.
  sessionId @6 :Text;
  # Model conversation id (open).
}

struct RunEventsList {
  events @0 :List(RunEvent);
}

struct SetMode {
  mode @0 :Util.SeatMode;
  # Closed product mode (was Text in v0.1).
}



struct Response {
  ok :union {
    pong @0:Pong;
    status @1:StatusInfo;
    mode @2:Mode;
    doctor @3:DoctorReport;
    error @4:Error;
    bye @5:Void;
    agentsList @6:AgentsList;
    runDetail @7:AgentInfo;
    runEvents @8:RunEventsList;
    agentAdmitted @9:AgentAdmitted;
    agentDetail @10:AgentRecord;
    childrenList @11:ChildrenList;
    agentEvents @12:AgentEventsList;
    messageAccepted @13:Void;
    agentCancelled @14:AgentCancelled;
    progressAck @15:Void;
    completeAck @16:Void;
    panesList @17:PanesList;
    paneDetail @18:PaneRecord;
    workDetail @19:WorkRecord;
    workList @20:WorkList;
    workClaimed @21:WorkClaimed;
    workAck @22:Void;
    voice @23:VoiceListenState;
    # voiceStatus / voiceArm / voiceDisarm / voicePushUtterance.
    voiceEvents @24:VoiceEventsList;
    # watchVoiceEvents.
    paneBuffer @25:PaneBuffer;
    # readPane text payload.
    subagentSnapshot @26:SubagentSnapshot;
    # querySubagent.
    subagentCompletions @27:SubagentCompletions;
    # drainSubagentCompletions.
    subagentOutstandingReply @28:SubagentOutstandingReply;
    # subagentOutstanding.
    subagentRegistryCounts @29:SubagentRegistryCounts;
    # subagentRegistryCounts.
    admitIssued @30:AdmitIssued;
    # issueAdmit.
    admitOk @31:AdmitOk;
    # checkAdmit (expiry may be refreshed).
  }
}

# --- Workspace plane (herdr-class; pane ids are engine public strings) -------

struct ReportPane {
  # Shell/product publishes mux truth onto the seat board.
  # paneId is engine-stable (e.g. terminal_1). workspaceId is mux session name.

  paneId @0 :Text;
  # Required non-empty. Engine form: terminal_N | plugin_N.
  tabId @1 :Text;
  # Tab public id or name when known; empty if unknown.
  workspaceId @2 :Text;
  # Mux session name (herdr workspace analog). Empty = default product session.
  label @3 :Text;
  # Human pane name / rename target.
  cwd @4 :Text;
  # Pane working directory when known.
  agentId @5 :Util.AgentId;
  # Cap'n AgentId bound to this pane (zero = no agent binding).
  agentLabel @6 :Text;
  # Display label for integrated agent (grokos-agent, or peer agent, …).
  agentState @7 :Util.RunState = unknown;
  # herdr agent_status on this pane.
  focused @8 :Bool;
  # Whether this pane holds focus in its tab.
  command @9 :Text;
  # Running command argv0 or product launcher when known.
  floating @10 :Bool;
  # Engine floating bit when known.
}

struct ListPanes {
  workspaceId @0 :Text;
  # Empty = all workspaces on this seat.
}

struct GetPane {
  paneId @0 :Text;
  # Required non-empty.
  workspaceId @1 :Text;
  # Optional disambiguator when the same engine id appears in multiple sessions.
}

struct ReleasePane {
  paneId @0 :Text;
  workspaceId @1 :Text;
}

struct PaneRecord {
  paneId @0 :Text;
  tabId @1 :Text;
  workspaceId @2 :Text;
  label @3 :Text;
  cwd @4 :Text;
  agentId @5 :Util.AgentId;
  agentLabel @6 :Text;
  agentState @7 :Util.RunState;
  focused @8 :Bool;
  command @9 :Text;
  floating @10 :Bool;
  updatedUnix @11 :Util.UnixSecs;
}

struct PanesList {
  panes @0 :List(PaneRecord);
}

struct SplitPane {
  # Create a tiled pane in the mux session (workspaceId).
  # sessiond SERVER implements; clients never drive the engine socket.
  # Farm visibility: after SpawnAgent admit, Cap'n splitPane with command
  # (pane root process) + agentId (bind board row). No CLI shell-out.

  workspaceId @0 :Text;
  # Mux session name; empty = product default (grokos-develop).
  direction @1 :Text;
  # left|right|up|down; empty = engine default placement.
  name @2 :Text;
  # Optional pane title.
  cwd @3 :Text;
  # Absolute working directory for the new pane process. Empty = session default.
  command @4 :Text;
  # Pane **root** process: absolute executable path, optional argv joined
  # product-side (first token is argv0). Empty = default interactive shell
  # in that pane only. Not "type into bash"; not agent shelling grokos-shell.
  noFocus @5 :Bool;
  # When true, do not steal focus from the caller pane.
  agentId @6 :Util.AgentId;
  # Optional Cap'n bind (zero = unbound). Farm: set to SpawnAgent-minted id
  # so listPanes / getPane show the child without a second invent protocol.
}

struct FocusPane {
  paneId @0 :Text;
  # When non-empty, focus this pane id.
  workspaceId @1 :Text;
  direction @2 :Text;
  # When non-empty (and paneId empty), move focus left|right|up|down.
}

struct ClosePane {
  paneId @0 :Text;
  workspaceId @1 :Text;
}

struct ReadPane {
  paneId @0 :Text;
  workspaceId @1 :Text;
  source @2 :Text;
  # visible | recent | recent-unwrapped (herdr-class). Empty = visible.
  lines @3 :UInt32;
  # 0 = full dump for source; else last N lines.
  ansi @4 :Bool;
  # Keep ANSI when true (default strip for recent-unwrapped).
}

struct WritePane {
  paneId @0 :Text;
  workspaceId @1 :Text;
  text @2 :Text;
  enter @3 :Bool;
  # When true, append newline (pane run parity).
}

struct PaneBuffer {
  paneId @0 :Text;
  workspaceId @1 :Text;
  text @2 :Text;
}

# --- Work DAG plane ---------------------------------------------------------
# Identity is Util.AgentId (xxh3-128) everywhere — never Text ids.
# Closed enums for kind/status/role. One open Text field: summary (prose).

enum WorkStatus {
  # Closed work lifecycle. CAS claim/complete use these.
  unset @0;
  todo @1;
  ready @2;
  claimed @3;
  running @4;
  blocked @5;
  done @6;
  failed @7;
  cancelled @8;
}

enum WorkRole {
  # Legion/taskflow-class role on a node.
  unset @0;
  explore @1;
  architect @2;
  implementor @3;
  verifier @4;
  orchestrator @5;
  general @6;
}

enum WorkKind {
  # Closed node kinds (not free-form Text).
  unset @0;
  goal @1;
  step @2;
  task @3;
  molecule @4;
}

struct UpsertWork {
  # Create or refresh a work node (sessiond sole mutator).
  # Zero id = sessiond mints a new WorkId (same xxh3 family as AgentId).
  id @0 :Util.AgentId;
  kind @1 :WorkKind = task;
  status @2 :WorkStatus = todo;
  role @3 :WorkRole = unset;
  parentWork @4 :Util.AgentId;
  # Zero = root work node.
  actorId @5 :Util.AgentId;
  # Who is writing (ledger). Zero allowed for CLI.
  summary @6 :Text;
  # Only open-content field (title+body prose). Prefer short.
}

struct ListWork {
  status @0 :WorkStatus = unset;
  # unset = all statuses.
  limit @1 :UInt32 = 200;
}

struct GetWork {
  id @0 :Util.AgentId;
  # Required non-zero.
}

struct ClaimWork {
  id @0 :Util.AgentId;
  assigneeId @1 :Util.AgentId;
  # Both required non-zero.
  expectedGen @2 :UInt64;
  # 0 = ignore gen; else CAS must match.
}

struct CompleteWork {
  id @0 :Util.AgentId;
  status @1 :WorkStatus;
  # Must be done|failed|cancelled.
  actorId @2 :Util.AgentId;
  summary @3 :Text;
  # Optional final prose; empty keeps prior summary.
}

struct LinkWork {
  # parent must be done before child is claimable.
  parentId @0 :Util.AgentId;
  childId @1 :Util.AgentId;
  actorId @2 :Util.AgentId;
}

struct WorkRecord {
  id @0 :Util.AgentId;
  kind @1 :WorkKind;
  status @2 :WorkStatus;
  role @3 :WorkRole;
  assigneeId @4 :Util.AgentId;
  parentWork @5 :Util.AgentId;
  deps @6 :List(Util.AgentId);
  gen @7 :UInt64;
  createdUnix @8 :Util.UnixSecs;
  updatedUnix @9 :Util.UnixSecs;
  finishedUnix @10 :Util.UnixSecs;
  summary @11 :Text;
  # Sole open-content field on the record.
}

struct WorkList {
  works @0 :List(WorkRecord);
}

struct WorkClaimed {
  id @0 :Util.AgentId;
  gen @1 :UInt64;
  # New generation after successful claim.
}

# --- Voice plane (listen + STT text; no PCM on Cap'n) ------------------------
#
# Text fields are implementer-bounded UTF-8 (sessiond chooses ceilings). Product
# fail-closed policy still applies at the server:
#   - Live STT partial/final: may truncate to the implementer text ceiling.
#   - voicePushUtterance / client-supplied text: reject with error.invalid if over.
#   - Meta Text (backend, modelId, lang) and sourceNode: implementer meta/source ceilings.
#   - lastError: implementer error ceiling (truncate ok for diagnostics).
# No Data / raw audio fields. Debug PCM dumps stay off this bus (local escape only).
# Prefer ASAP transcription; do not ship bulk audio on this control plane.

struct VoiceSize {
  # Empty on purpose: schema describes message shape, not product byte ceilings.
  # UTF-8 length limits for voice Text fields (transcript, meta, sourceNode,
  # lastError) are left to implementers (sessiond config / policy). Baking
  # fixed consts into the IDL would freeze policy as protocol law and force
  # Schema bumps when operators tune limits. See voice-plane header above for
  # fail-closed guidance implementers should still follow.
  # Intentional exception to SCHEMA_STYLE "const limits" (Sandstorm
  # Manifest.sizeLimitInWords): those are for true protocol invariants; this
  # marker is for operator/policy ceilings only — do not thrash back to consts.
}

struct VoiceArm {
  # Request body for voiceArm. Minimal knobs only (A4).
  # Backend/model are NOT selected on the wire — sessiond config / env pin (Track C).
  # Who sets: client. Empty fields mean server defaults.

  wakeRequired @0 :Bool = false;
  # Client arm preference. Server stores and echoes on VoiceListenState.wakeRequired.
  # Default false. Semantics of wake vs VAD are sessiond policy, not this field.

  lang @1 :Text;
  # Preferred language hint (BCP-47-ish open text). Empty = sessiond default (often "en").
  # Implementer-bounded (meta); overlong → error.invalid.

  sourceNode @2 :Text;
  # Optional PipeWire source node name/id. Empty = WirePlumber default source.
  # Implementer-bounded (source); overlong → error.invalid.
  # Display/selection only — not a filesystem path and not secrets.
}

struct VoicePushUtterance {
  # Harness/dev inject. Simulates ASR output without opening the mic.
  # Who sets: test client / CI. Production: policyd should deny voice/inject.
  # Trust: same-uid can forge; not an authz channel. confidence is not authz.

  text @0 :Text;
  # Required non-empty. Implementer-bounded (transcript); overlong → error.invalid
  # (no truncate on inject).

  asFinal @1 :Bool = true;
  # true → emit final (and update state.final); false → partial only.

  confidence @2 :Float32 = -1.0;
  # -1.0 = unknown. Not used for seat ACL or shell/exec authz.

  lang @3 :Text;
  # Optional; empty keeps current listen lang. Implementer-bounded (meta).
}

struct WatchVoiceEvents {
  # Long-poll trail of voice plane events (primary stream; A2).
  # Mirrors WatchAgentEvents discipline. SessionEvent memfd is not product law (A11).

  sinceSeq @0 :UInt64;
  # Return events with seq > sinceSeq. 0 = from start of retained ring.

  limit @1 :UInt32 = 50;
  # Preferred max events. 50 is intentionally lower than WatchAgentEvents (200):
  # partial spam. Servers may clamp (recommend max 200).

  waitSecs @2 :UInt32 = 0;
  # 0 = return immediately; >0 long-poll up to waitSecs for new events.
}

enum VoiceEventKind {
  # Closed event kinds on the voice trail. Not free Text.
  # unset = never-set sentinel (Cap'n zero default), not a product kind.
  unset @0;
  partial @1;
  # Intermediate transcript (streaming commit policy).
  final @2;
  # Stable committed utterance text.
  armed @3;
  # Listen mode became armed.
  disarmed @4;
  # Listen mode became disarmed.
  error @5;
  # Soft/hard voice path error (text carries diagnostic; bounded).
}

struct VoiceEvent {
  # One retained trail row (watchVoiceEvents).

  seq @0 :UInt64;
  # Strictly increasing on the listen plane. Used as watch cursor.

  kind @1 :VoiceEventKind;
  # Closed kind.

  text @2 :Text;
  # partial/final body, or error diagnostic. Empty for armed/disarmed when unused.
  # Implementer-bounded (transcript, or error ceiling when kind=error).

  confidence @3 :Float32 = -1.0;
  # -1.0 = unknown. Meaningful for partial/final; not authz (A14).

  latencyMs @4 :UInt32 = 0;
  # 0 = unknown. Last-path latency when kind=final (or partial if known).

  tsUnix @5 :Util.UnixSecs;
  # Event time. Zero if unknown.
}

struct VoiceEventsList {
  # Response for watchVoiceEvents.

  events @0 :List(VoiceEvent);
  # Ordered by seq ascending. May be empty.

  nextSeq @1 :UInt64;
  # Client passes as sinceSeq on the next watch. Typically max(seq) in events,
  # or sinceSeq unchanged when empty after wait.
}

struct VoiceListenState {
  # Snapshot of the seat voice plane (status.voice / voiceStatus / arm echo).
  # Who sets: sessiond only. Clients must not forge this as authority for mic
  # open without going through voiceArm + policy.
  # Trust: same-uid peers see this; schema is not a second ACL.
  # No PCM / Data. Backend and modelId are echo of server pin only (A5).

  armed @0 :Bool;
  # True while listen mode is active. Default false at sessiond start.

  wakeRequired @1 :Bool;
  # Echo of arm config. false when disarmed or not configured.

  backend @2 :Text;
  # STT backend id echo: "whisper.cpp" | "faster-whisper" | … Empty if unknown/off.
  # Implementer-bounded (meta). Open set — Text is correct (not a closed enum).

  modelId @3 :Text;
  # Model pin id echo (not weights, not filesystem dump). Empty if unknown/off.
  # Implementer-bounded (meta).

  sourceNode @4 :Text;
  # Active capture source display. Empty if disarmed/unknown.
  # Implementer-bounded (source).

  partial @5 :Text;
  # Last partial transcript only (not a full history). Empty if none.
  # Implementer-bounded (transcript); live STT may truncate.

  final @6 :Text;
  # Last committed final transcript. Empty if none.
  # Implementer-bounded (transcript).

  lang @7 :Text;
  # Active language hint. Empty if unset. Implementer-bounded (meta).

  confidence @8 :Float32 = -1.0;
  # Last partial/final confidence. -1.0 = unknown. Not authz (A14).

  latencyMs @9 :UInt32 = 0;
  # Last final (or known) path latency in ms. 0 = unknown.

  updatedUnix @10 :Util.UnixSecs;
  # Last state change. Zero if never updated.

  seq @11 :UInt64;
  # Listen-plane monotonic seq (matches latest trail event when advanced).
  # 0 at cold start.

  lastError @12 :Text;
  # Bounded diagnostic when arm/STT/capture fails. Empty if ok.
  # Implementer-bounded (error); servers may truncate.
  # Wake live-bit (heard this window) is deferred until a wake product path
  # lands; use wakeRequired config echo + event trail then, not a premature field.
}

struct Pong {
  sessiond @0 :Text;
  # Server identity string (version tag). Not a security token.
}

struct Mode {
  mode @0 :Util.SeatMode;
  # Closed product mode (was Text in v0.1).
}

struct Error {
  message @0 :Text;
  # Human message (v0.1 field; keep ordinal).
  code @1 :ErrorCode = unknown;
  # Machine code for control flow (v0.2+). Default unknown.
}

enum ErrorCode {
  unknown @0;
  offline @1;
  denied @2;
  notFound @3;
  invalid @4;
  internal @5;
}



struct LoadState {
  level @0 :UInt8;
  cpuSomeAvg10 @1 :Float32;
  memorySomeAvg10 @2 :Float32;
  ioSomeAvg10 @3 :Float32;
  goalsActive @4 :UInt32;
  goalsDeferred @5 :UInt64;
  admitGoals @6 :Bool;
  detail @7 :Text;
  systemdOomd @8 :Util.BackendState;
  metaOomd @9 :Util.BackendState;
}

struct HardwareState {
  power @0 :Util.BackendState;
  thermal @1 :Util.BackendState;
  display @2 :Util.BackendState;
  input @3 :Util.BackendState;
  bluetooth @4 :Util.BackendState;
  gpu @5 :Util.BackendState;
  pressureLevel @6 :UInt8;
  summary @7 :Text;
}

struct StatusInfo {
  mode @0 :Util.SeatMode;
  # Closed product mode (was Text in v0.1).
  modelOnInstant @1 :Bool;
  # Must be false: models never on instant path.
  stateDir @2 :Text;
  socket @3 :Text;
  # Absolute path of the seat UDS.
  sway @4 :Util.BackendState;
  systemdUser @5 :Util.BackendState;
  eventCount @6 :UInt64;
  activeGoal @7 :Text;
  grokBin @8 :Text;
  pipewire @9 :Util.BackendState;
  network @10 :Util.BackendState;
  load @11 :LoadState;
  hardware @12 :HardwareState;
  agents @13 :List(AgentInfo);
  voice @14 :VoiceListenState;
  # Voice plane snapshot for peer snoop (same shape as voiceStatus).
  # Disarmed defaults when voice feature is off or never armed.
}

struct DoctorCheck {
  name @0 :Text;
  # Open check name. Voice plane conventions (sessiond B/C, not schema law):
  #   "voice" | "voice.mic" | "voice.stt" | "voice.wake" | "voice.vad"
  # Soft vs hard is the `hard` bit; do not invent parallel Cap'n doctor structs.
  ok @1 :Bool;
  hard @2 :Bool;
  detail @3 :Text;
}

struct DoctorReport {
  hardOk @0 :Bool;
  checks @1 :List(DoctorCheck);
}

struct SessionEvent {
  # Optional unsolicited event. Prefer RunEvent for board trails.

  tsRfc3339 @0 :Text;
  # Deprecated for new writers; prefer tsUnix. Kept for v0.1 readers.
  source @1 :Text;
  kind @2 :Text;
  attrs @3 :List(Util.KeyValue);
  # Was List(Attr); KeyValue is the same key/value layout (util).
  tsUnix @4 :Util.UnixSecs;
  # Preferred wall time (v0.2+). Zero if only tsRfc3339 set.
}
