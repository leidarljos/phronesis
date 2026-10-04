#ifndef CAPN_E570DD5CBCE72246
#define CAPN_E570DD5CBCE72246
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

struct KeyValue;
struct BackendState;
struct AgentId;
struct TraceId;

typedef struct {capn_ptr p;} KeyValue_ptr;
typedef struct {capn_ptr p;} BackendState_ptr;
typedef struct {capn_ptr p;} AgentId_ptr;
typedef struct {capn_ptr p;} TraceId_ptr;

typedef struct {capn_ptr p;} KeyValue_list;
typedef struct {capn_ptr p;} BackendState_list;
typedef struct {capn_ptr p;} AgentId_list;
typedef struct {capn_ptr p;} TraceId_list;

enum RunState {
	RunState_unknown = 0,
	RunState_idle = 1,
	RunState_working = 2,
	RunState_blocked = 3,
	RunState_done = 4,
	RunState_cancelled = 5,
	RunState_admitted = 6,
	RunState_starting = 7,
	RunState_running = 8,
	RunState_waiting = 9,
	RunState_succeeded = 10,
	RunState_failed = 11
};

enum AgentSurface {
	AgentSurface_unset = 0,
	AgentSurface_board = 1,
	AgentSurface_planAwaitingApproval = 2,
	AgentSurface_userQuestion = 3
};

enum SeatMode {
	SeatMode_develop = 0,
	SeatMode_focus = 1
};

struct KeyValue {
	capn_text key;
	capn_text value;
};

static const size_t KeyValue_word_count = 0;

static const size_t KeyValue_pointer_count = 2;

static const size_t KeyValue_struct_bytes_count = 16;


struct BackendState {
	unsigned available : 1;
	capn_text detail;
};

static const size_t BackendState_word_count = 1;

static const size_t BackendState_pointer_count = 1;

static const size_t BackendState_struct_bytes_count = 16;


struct AgentId {
	uint64_t hi;
	uint64_t lo;
};

static const size_t AgentId_word_count = 2;

static const size_t AgentId_pointer_count = 0;

static const size_t AgentId_struct_bytes_count = 16;


struct TraceId {
	uint64_t hi;
	uint64_t lo;
};

static const size_t TraceId_word_count = 2;

static const size_t TraceId_pointer_count = 0;

static const size_t TraceId_struct_bytes_count = 16;


KeyValue_ptr new_KeyValue(struct capn_segment*);
BackendState_ptr new_BackendState(struct capn_segment*);
AgentId_ptr new_AgentId(struct capn_segment*);
TraceId_ptr new_TraceId(struct capn_segment*);

KeyValue_list new_KeyValue_list(struct capn_segment*, int len);
BackendState_list new_BackendState_list(struct capn_segment*, int len);
AgentId_list new_AgentId_list(struct capn_segment*, int len);
TraceId_list new_TraceId_list(struct capn_segment*, int len);

void read_KeyValue(struct KeyValue*, KeyValue_ptr);
void read_BackendState(struct BackendState*, BackendState_ptr);
void read_AgentId(struct AgentId*, AgentId_ptr);
void read_TraceId(struct TraceId*, TraceId_ptr);

void write_KeyValue(const struct KeyValue*, KeyValue_ptr);
void write_BackendState(const struct BackendState*, BackendState_ptr);
void write_AgentId(const struct AgentId*, AgentId_ptr);
void write_TraceId(const struct TraceId*, TraceId_ptr);

void get_KeyValue(struct KeyValue*, KeyValue_list, int i);
void get_BackendState(struct BackendState*, BackendState_list, int i);
void get_AgentId(struct AgentId*, AgentId_list, int i);
void get_TraceId(struct TraceId*, TraceId_list, int i);

void set_KeyValue(const struct KeyValue*, KeyValue_list, int i);
void set_BackendState(const struct BackendState*, BackendState_list, int i);
void set_AgentId(const struct AgentId*, AgentId_list, int i);
void set_TraceId(const struct TraceId*, TraceId_list, int i);

#ifdef __cplusplus
}
#endif
#endif
