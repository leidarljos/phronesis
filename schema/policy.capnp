# GrokOS policyd Cap'n peer wire (meta #70 / Slice C).
# Not the seat bus (session.capnp / GKSP). Magic on the wire is GKPP.
# Schema SoT for this package; grokos-session pins a copy under crates/grokos-schema/schema/.

@0xe2859f3833215a0b;

struct PolicyEnvelope {
  protocolVersion @0 :UInt32 = 1;
  traceId @1 :Text;
  body :union {
    request @2 :PolicyRequest;
    response @3 :PolicyResponse;
  }
}

struct PolicyRequest {
  op :union {
    # Liveness / version (daemon + ABI generation).
    status @0 :Void;
    # Fail-closed tool/action/path evaluation (maps grok_policy_check).
    check @1 :PolicyCheck;
    # Admit plane before model / agent work.
    admit @2 :PolicyAdmit;
    # Multi-agent TCB probe (maps grok_supervisor_status).
    agentStatus @3 :AgentQuery;
  }
}

struct PolicyCheck {
  agentId @0 :Text;
  tool @1 :Text;
  action @2 :Text;
  path @3 :Text;
}

struct PolicyAdmit {
  agentId @0 :Text;
  kind @1 :Text;
  detail @2 :Text;
}

struct AgentQuery {
  agentId @0 :Text;
}

struct PolicyResponse {
  ok :union {
    status @0 :PolicydStatus;
    check @1 :PolicyDecision;
    admit @2 :PolicyDecision;
    agentStatus @3 :AgentStatusWire;
    error @4 :PolicyError;
  }
}

struct PolicydStatus {
  version @0 :Text;
  apiVersion @1 :Int32;
  stateDir @2 :Text;
  runtimeDir @3 :Text;
  socket @4 :Text;
  ready @5 :Bool;
}

enum Decision {
  deny @0;
  allow @1;
  prompt @2;
}

struct PolicyDecision {
  decision @0 :Decision;
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
  exitStatus @4 :Int32;
  mode @5 :Text;
  workspace @6 :Text;
  hasCgroup @7 :Bool;
}

struct PolicyError {
  code @0 :Int32;
  message @1 :Text;
}
