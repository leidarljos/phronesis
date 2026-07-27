#ifndef CAPN_E2859F3833215A0B
#define CAPN_E2859F3833215A0B
/* AUTO GENERATED - DO NOT EDIT */
#include <capnp_c.h>

#if CAPN_VERSION != 1
#error "version mismatch between capnp_c.h and generated code"
#endif

#ifndef capnp_nowarn
# ifdef __GNUC__
#  define capnp_nowarn __extension__
# else
#  define capnp_nowarn
# endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

struct PolicyEnvelope;
struct PolicyRequest;
struct PolicyCheck;
struct PolicyAdmit;
struct AgentQuery;
struct PolicyResponse;
struct PolicydStatus;
struct PolicyDecision;
struct AgentStatusWire;
struct PolicyError;

typedef struct {capn_ptr p;} PolicyEnvelope_ptr;
typedef struct {capn_ptr p;} PolicyRequest_ptr;
typedef struct {capn_ptr p;} PolicyCheck_ptr;
typedef struct {capn_ptr p;} PolicyAdmit_ptr;
typedef struct {capn_ptr p;} AgentQuery_ptr;
typedef struct {capn_ptr p;} PolicyResponse_ptr;
typedef struct {capn_ptr p;} PolicydStatus_ptr;
typedef struct {capn_ptr p;} PolicyDecision_ptr;
typedef struct {capn_ptr p;} AgentStatusWire_ptr;
typedef struct {capn_ptr p;} PolicyError_ptr;

typedef struct {capn_ptr p;} PolicyEnvelope_list;
typedef struct {capn_ptr p;} PolicyRequest_list;
typedef struct {capn_ptr p;} PolicyCheck_list;
typedef struct {capn_ptr p;} PolicyAdmit_list;
typedef struct {capn_ptr p;} AgentQuery_list;
typedef struct {capn_ptr p;} PolicyResponse_list;
typedef struct {capn_ptr p;} PolicydStatus_list;
typedef struct {capn_ptr p;} PolicyDecision_list;
typedef struct {capn_ptr p;} AgentStatusWire_list;
typedef struct {capn_ptr p;} PolicyError_list;

enum Decision {
	Decision_deny = 0,
	Decision_allow = 1,
	Decision_prompt = 2
};

enum AgentStateWire {
	AgentStateWire_stopped = 0,
	AgentStateWire_running = 1,
	AgentStateWire_failed = 2
};
enum PolicyEnvelope_body_which {
	PolicyEnvelope_body_request = 0,
	PolicyEnvelope_body_response = 1
};

struct PolicyEnvelope {
	uint32_t protocolVersion;
	capn_text traceId;
	enum PolicyEnvelope_body_which body_which;
	capnp_nowarn union {
		PolicyRequest_ptr request;
		PolicyResponse_ptr response;
	} body;
};

static const size_t PolicyEnvelope_word_count = 1;

static const size_t PolicyEnvelope_pointer_count = 2;

static const size_t PolicyEnvelope_struct_bytes_count = 24;

enum PolicyRequest_op_which {
	PolicyRequest_op_status = 0,
	PolicyRequest_op_check = 1,
	PolicyRequest_op_admit = 2,
	PolicyRequest_op_agentStatus = 3
};

struct PolicyRequest {
	enum PolicyRequest_op_which op_which;
	capnp_nowarn union {
		PolicyCheck_ptr check;
		PolicyAdmit_ptr admit;
		AgentQuery_ptr agentStatus;
	} op;
};

static const size_t PolicyRequest_word_count = 1;

static const size_t PolicyRequest_pointer_count = 1;

static const size_t PolicyRequest_struct_bytes_count = 16;


struct PolicyCheck {
	capn_text agentId;
	capn_text tool;
	capn_text action;
	capn_text path;
};

static const size_t PolicyCheck_word_count = 0;

static const size_t PolicyCheck_pointer_count = 4;

static const size_t PolicyCheck_struct_bytes_count = 32;


struct PolicyAdmit {
	capn_text agentId;
	capn_text kind;
	capn_text detail;
};

static const size_t PolicyAdmit_word_count = 0;

static const size_t PolicyAdmit_pointer_count = 3;

static const size_t PolicyAdmit_struct_bytes_count = 24;


struct AgentQuery {
	capn_text agentId;
};

static const size_t AgentQuery_word_count = 0;

static const size_t AgentQuery_pointer_count = 1;

static const size_t AgentQuery_struct_bytes_count = 8;

enum PolicyResponse_ok_which {
	PolicyResponse_ok_status = 0,
	PolicyResponse_ok_check = 1,
	PolicyResponse_ok_admit = 2,
	PolicyResponse_ok_agentStatus = 3,
	PolicyResponse_ok_error = 4
};

struct PolicyResponse {
	enum PolicyResponse_ok_which ok_which;
	capnp_nowarn union {
		PolicydStatus_ptr status;
		PolicyDecision_ptr check;
		PolicyDecision_ptr admit;
		AgentStatusWire_ptr agentStatus;
		PolicyError_ptr error;
	} ok;
};

static const size_t PolicyResponse_word_count = 1;

static const size_t PolicyResponse_pointer_count = 1;

static const size_t PolicyResponse_struct_bytes_count = 16;


struct PolicydStatus {
	capn_text version;
	int32_t apiVersion;
	capn_text stateDir;
	capn_text runtimeDir;
	capn_text socket;
	unsigned ready : 1;
};

static const size_t PolicydStatus_word_count = 1;

static const size_t PolicydStatus_pointer_count = 4;

static const size_t PolicydStatus_struct_bytes_count = 40;


struct PolicyDecision {
	enum Decision decision;
	capn_text reason;
	capn_text agentId;
	capn_text tool;
	capn_text action;
};

static const size_t PolicyDecision_word_count = 1;

static const size_t PolicyDecision_pointer_count = 4;

static const size_t PolicyDecision_struct_bytes_count = 40;


struct AgentStatusWire {
	capn_text id;
	enum AgentStateWire state;
	int32_t pid;
	int32_t pgid;
	int32_t exitStatus;
	capn_text mode;
	capn_text workspace;
	unsigned hasCgroup : 1;
};

static const size_t AgentStatusWire_word_count = 2;

static const size_t AgentStatusWire_pointer_count = 3;

static const size_t AgentStatusWire_struct_bytes_count = 40;


struct PolicyError {
	int32_t code;
	capn_text message;
};

static const size_t PolicyError_word_count = 1;

static const size_t PolicyError_pointer_count = 1;

static const size_t PolicyError_struct_bytes_count = 16;


PolicyEnvelope_ptr new_PolicyEnvelope(struct capn_segment*);
PolicyRequest_ptr new_PolicyRequest(struct capn_segment*);
PolicyCheck_ptr new_PolicyCheck(struct capn_segment*);
PolicyAdmit_ptr new_PolicyAdmit(struct capn_segment*);
AgentQuery_ptr new_AgentQuery(struct capn_segment*);
PolicyResponse_ptr new_PolicyResponse(struct capn_segment*);
PolicydStatus_ptr new_PolicydStatus(struct capn_segment*);
PolicyDecision_ptr new_PolicyDecision(struct capn_segment*);
AgentStatusWire_ptr new_AgentStatusWire(struct capn_segment*);
PolicyError_ptr new_PolicyError(struct capn_segment*);

PolicyEnvelope_list new_PolicyEnvelope_list(struct capn_segment*, int len);
PolicyRequest_list new_PolicyRequest_list(struct capn_segment*, int len);
PolicyCheck_list new_PolicyCheck_list(struct capn_segment*, int len);
PolicyAdmit_list new_PolicyAdmit_list(struct capn_segment*, int len);
AgentQuery_list new_AgentQuery_list(struct capn_segment*, int len);
PolicyResponse_list new_PolicyResponse_list(struct capn_segment*, int len);
PolicydStatus_list new_PolicydStatus_list(struct capn_segment*, int len);
PolicyDecision_list new_PolicyDecision_list(struct capn_segment*, int len);
AgentStatusWire_list new_AgentStatusWire_list(struct capn_segment*, int len);
PolicyError_list new_PolicyError_list(struct capn_segment*, int len);

void read_PolicyEnvelope(struct PolicyEnvelope*, PolicyEnvelope_ptr);
void read_PolicyRequest(struct PolicyRequest*, PolicyRequest_ptr);
void read_PolicyCheck(struct PolicyCheck*, PolicyCheck_ptr);
void read_PolicyAdmit(struct PolicyAdmit*, PolicyAdmit_ptr);
void read_AgentQuery(struct AgentQuery*, AgentQuery_ptr);
void read_PolicyResponse(struct PolicyResponse*, PolicyResponse_ptr);
void read_PolicydStatus(struct PolicydStatus*, PolicydStatus_ptr);
void read_PolicyDecision(struct PolicyDecision*, PolicyDecision_ptr);
void read_AgentStatusWire(struct AgentStatusWire*, AgentStatusWire_ptr);
void read_PolicyError(struct PolicyError*, PolicyError_ptr);

void write_PolicyEnvelope(const struct PolicyEnvelope*, PolicyEnvelope_ptr);
void write_PolicyRequest(const struct PolicyRequest*, PolicyRequest_ptr);
void write_PolicyCheck(const struct PolicyCheck*, PolicyCheck_ptr);
void write_PolicyAdmit(const struct PolicyAdmit*, PolicyAdmit_ptr);
void write_AgentQuery(const struct AgentQuery*, AgentQuery_ptr);
void write_PolicyResponse(const struct PolicyResponse*, PolicyResponse_ptr);
void write_PolicydStatus(const struct PolicydStatus*, PolicydStatus_ptr);
void write_PolicyDecision(const struct PolicyDecision*, PolicyDecision_ptr);
void write_AgentStatusWire(const struct AgentStatusWire*, AgentStatusWire_ptr);
void write_PolicyError(const struct PolicyError*, PolicyError_ptr);

void get_PolicyEnvelope(struct PolicyEnvelope*, PolicyEnvelope_list, int i);
void get_PolicyRequest(struct PolicyRequest*, PolicyRequest_list, int i);
void get_PolicyCheck(struct PolicyCheck*, PolicyCheck_list, int i);
void get_PolicyAdmit(struct PolicyAdmit*, PolicyAdmit_list, int i);
void get_AgentQuery(struct AgentQuery*, AgentQuery_list, int i);
void get_PolicyResponse(struct PolicyResponse*, PolicyResponse_list, int i);
void get_PolicydStatus(struct PolicydStatus*, PolicydStatus_list, int i);
void get_PolicyDecision(struct PolicyDecision*, PolicyDecision_list, int i);
void get_AgentStatusWire(struct AgentStatusWire*, AgentStatusWire_list, int i);
void get_PolicyError(struct PolicyError*, PolicyError_list, int i);

void set_PolicyEnvelope(const struct PolicyEnvelope*, PolicyEnvelope_list, int i);
void set_PolicyRequest(const struct PolicyRequest*, PolicyRequest_list, int i);
void set_PolicyCheck(const struct PolicyCheck*, PolicyCheck_list, int i);
void set_PolicyAdmit(const struct PolicyAdmit*, PolicyAdmit_list, int i);
void set_AgentQuery(const struct AgentQuery*, AgentQuery_list, int i);
void set_PolicyResponse(const struct PolicyResponse*, PolicyResponse_list, int i);
void set_PolicydStatus(const struct PolicydStatus*, PolicydStatus_list, int i);
void set_PolicyDecision(const struct PolicyDecision*, PolicyDecision_list, int i);
void set_AgentStatusWire(const struct AgentStatusWire*, AgentStatusWire_list, int i);
void set_PolicyError(const struct PolicyError*, PolicyError_list, int i);

#ifdef __cplusplus
}
#endif
#endif
