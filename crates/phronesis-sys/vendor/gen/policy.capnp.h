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

#include "util.capnp.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PolicyDecision;
struct PolicydStatus;
struct AgentStatus;
struct SeatCheck;
struct ModelCheck;
struct PathCheck;
struct ShellCheck;
struct PathProbe;
struct ShellView;
struct RiskCheck;
struct AudioCheck;
struct AdmitSeat;
struct AdmitModel;
struct AgentQuery;
struct ReloadShellPack;

typedef struct {capn_ptr p;} PolicyDecision_ptr;
typedef struct {capn_ptr p;} PolicydStatus_ptr;
typedef struct {capn_ptr p;} AgentStatus_ptr;
typedef struct {capn_ptr p;} SeatCheck_ptr;
typedef struct {capn_ptr p;} ModelCheck_ptr;
typedef struct {capn_ptr p;} PathCheck_ptr;
typedef struct {capn_ptr p;} ShellCheck_ptr;
typedef struct {capn_ptr p;} PathProbe_ptr;
typedef struct {capn_ptr p;} ShellView_ptr;
typedef struct {capn_ptr p;} RiskCheck_ptr;
typedef struct {capn_ptr p;} AudioCheck_ptr;
typedef struct {capn_ptr p;} AdmitSeat_ptr;
typedef struct {capn_ptr p;} AdmitModel_ptr;
typedef struct {capn_ptr p;} AgentQuery_ptr;
typedef struct {capn_ptr p;} ReloadShellPack_ptr;

typedef struct {capn_ptr p;} PolicyDecision_list;
typedef struct {capn_ptr p;} PolicydStatus_list;
typedef struct {capn_ptr p;} AgentStatus_list;
typedef struct {capn_ptr p;} SeatCheck_list;
typedef struct {capn_ptr p;} ModelCheck_list;
typedef struct {capn_ptr p;} PathCheck_list;
typedef struct {capn_ptr p;} ShellCheck_list;
typedef struct {capn_ptr p;} PathProbe_list;
typedef struct {capn_ptr p;} ShellView_list;
typedef struct {capn_ptr p;} RiskCheck_list;
typedef struct {capn_ptr p;} AudioCheck_list;
typedef struct {capn_ptr p;} AdmitSeat_list;
typedef struct {capn_ptr p;} AdmitModel_list;
typedef struct {capn_ptr p;} AgentQuery_list;
typedef struct {capn_ptr p;} ReloadShellPack_list;

enum Decision {
	Decision_deny = 0,
	Decision_allow = 1,
	Decision_prompt = 2
};

enum SeatAction {
	SeatAction_publishRun = 0,
	SeatAction_readRun = 1,
	SeatAction_listRuns = 2,
	SeatAction_listEvents = 3
};

enum PathAction {
	PathAction_read = 0,
	PathAction_write = 1,
	PathAction_delete = 2
};

enum RiskAction {
	RiskAction_network = 0,
	RiskAction_secretExport = 1,
	RiskAction_sudo = 2,
	RiskAction_pay = 3,
	RiskAction_auth = 4,
	RiskAction_osChange = 5,
	RiskAction_privilege = 6
};

enum AgentState {
	AgentState_stopped = 0,
	AgentState_running = 1,
	AgentState_failed = 2,
	AgentState_missing = 3
};

enum PolicyReason {
	PolicyReason_unspecified = 0,
	PolicyReason_toolsDefaultDeny = 1,
	PolicyReason_pathOutsideWorkspace = 2,
	PolicyReason_pathUnderWorkspaceAllow = 3,
	PolicyReason_invalidMessage = 4,
	PolicyReason_fieldTooLong = 5,
	PolicyReason_denyAll = 6,
	PolicyReason_highRiskPrompt = 7,
	PolicyReason_seatBoardAllow = 8,
	PolicyReason_modelStartAllow = 9,
	PolicyReason_missingToolAction = 10,
	PolicyReason_unknownSeatAction = 11,
	PolicyReason_packMissing = 12,
	PolicyReason_packLoadFailed = 13,
	PolicyReason_packRuntimeError = 14,
	PolicyReason_packBadResult = 15,
	PolicyReason_shellViewBuildFailed = 16,
	PolicyReason_pythonRequiresUvRun = 17,
	PolicyReason_pythonDashCDenied = 18,
	PolicyReason_pythonMissingPep723 = 19,
	PolicyReason_shellExecAllow = 20,
	PolicyReason_packReloaded = 21,
	PolicyReason_packPathInvalid = 22,
	PolicyReason_shellDangerousRunner = 23,
	PolicyReason_shellRemoteExec = 24,
	PolicyReason_shellPrivilegeDenied = 25,
	PolicyReason_shellGitDangerous = 26,
	PolicyReason_shellSecretInArgv = 27,
	PolicyReason_pathSensitiveDeny = 28,
	PolicyReason_secretExportDenied = 29,
	PolicyReason_audioMicOpenDeny = 30,
	PolicyReason_audioListenArmPrompt = 31,
	PolicyReason_audioAlwaysListenDeny = 32,
	PolicyReason_audioNetworkSttDeny = 33,
	PolicyReason_audioInjectDeny = 34,
	PolicyReason_audioFixtureAllow = 35,
	PolicyReason_audioUnknownAction = 36
};

enum AudioAction {
	AudioAction_micOpen = 0,
	AudioAction_listenArm = 1,
	AudioAction_alwaysListen = 2,
	AudioAction_networkStt = 3,
	AudioAction_inject = 4
};

struct PolicyDecision {
	enum Decision decision;
	capn_text reason;
	AgentId_ptr agentId;
	enum PolicyReason code;
};

static const size_t PolicyDecision_word_count = 1;

static const size_t PolicyDecision_pointer_count = 2;

static const size_t PolicyDecision_struct_bytes_count = 24;


struct PolicydStatus {
	capn_text version;
	int32_t apiVersion;
	capn_text stateDir;
	capn_text runtimeDir;
	unsigned ready : 1;
};

static const size_t PolicydStatus_word_count = 1;

static const size_t PolicydStatus_pointer_count = 3;

static const size_t PolicydStatus_struct_bytes_count = 32;


struct AgentStatus {
	AgentId_ptr id;
	enum AgentState state;
	int32_t pid;
	int32_t pgid;
	int32_t exitStatus;
	enum SeatMode mode;
	capn_text workspace;
	unsigned hasCgroup : 1;
	capn_text detail;
};

static const size_t AgentStatus_word_count = 3;

static const size_t AgentStatus_pointer_count = 3;

static const size_t AgentStatus_struct_bytes_count = 48;


struct SeatCheck {
	AgentId_ptr agentId;
	enum SeatAction action;
};

static const size_t SeatCheck_word_count = 1;

static const size_t SeatCheck_pointer_count = 1;

static const size_t SeatCheck_struct_bytes_count = 16;


struct ModelCheck {
	AgentId_ptr agentId;
	capn_text model;
};

static const size_t ModelCheck_word_count = 0;

static const size_t ModelCheck_pointer_count = 2;

static const size_t ModelCheck_struct_bytes_count = 16;


struct PathCheck {
	AgentId_ptr agentId;
	enum PathAction action;
	capn_text path;
};

static const size_t PathCheck_word_count = 1;

static const size_t PathCheck_pointer_count = 2;

static const size_t PathCheck_struct_bytes_count = 24;


struct ShellCheck {
	AgentId_ptr agentId;
	capn_text cwd;
	capn_ptr argv;
};

static const size_t ShellCheck_word_count = 0;

static const size_t ShellCheck_pointer_count = 3;

static const size_t ShellCheck_struct_bytes_count = 24;


struct PathProbe {
	capn_text arg;
	capn_text resolved;
	unsigned exists : 1;
	capn_text head;
};

static const size_t PathProbe_word_count = 1;

static const size_t PathProbe_pointer_count = 3;

static const size_t PathProbe_struct_bytes_count = 32;


struct ShellView {
	unsigned underWorkspace : 1;
	capn_text cwd;
	capn_ptr argv;
	PathProbe_list pathProbes;
};

static const size_t ShellView_word_count = 1;

static const size_t ShellView_pointer_count = 3;

static const size_t ShellView_struct_bytes_count = 32;


struct RiskCheck {
	AgentId_ptr agentId;
	enum RiskAction action;
	capn_text path;
};

static const size_t RiskCheck_word_count = 1;

static const size_t RiskCheck_pointer_count = 2;

static const size_t RiskCheck_struct_bytes_count = 24;


struct AudioCheck {
	AgentId_ptr agentId;
	enum AudioAction action;
};

static const size_t AudioCheck_word_count = 1;

static const size_t AudioCheck_pointer_count = 1;

static const size_t AudioCheck_struct_bytes_count = 16;


struct AdmitSeat {
	AgentId_ptr agentId;
	capn_text detail;
};

static const size_t AdmitSeat_word_count = 0;

static const size_t AdmitSeat_pointer_count = 2;

static const size_t AdmitSeat_struct_bytes_count = 16;


struct AdmitModel {
	AgentId_ptr agentId;
	capn_text detail;
};

static const size_t AdmitModel_word_count = 0;

static const size_t AdmitModel_pointer_count = 2;

static const size_t AdmitModel_struct_bytes_count = 16;


struct AgentQuery {
	AgentId_ptr agentId;
};

static const size_t AgentQuery_word_count = 0;

static const size_t AgentQuery_pointer_count = 1;

static const size_t AgentQuery_struct_bytes_count = 8;


struct ReloadShellPack {
	capn_text path;
};

static const size_t ReloadShellPack_word_count = 0;

static const size_t ReloadShellPack_pointer_count = 1;

static const size_t ReloadShellPack_struct_bytes_count = 8;


PolicyDecision_ptr new_PolicyDecision(struct capn_segment*);
PolicydStatus_ptr new_PolicydStatus(struct capn_segment*);
AgentStatus_ptr new_AgentStatus(struct capn_segment*);
SeatCheck_ptr new_SeatCheck(struct capn_segment*);
ModelCheck_ptr new_ModelCheck(struct capn_segment*);
PathCheck_ptr new_PathCheck(struct capn_segment*);
ShellCheck_ptr new_ShellCheck(struct capn_segment*);
PathProbe_ptr new_PathProbe(struct capn_segment*);
ShellView_ptr new_ShellView(struct capn_segment*);
RiskCheck_ptr new_RiskCheck(struct capn_segment*);
AudioCheck_ptr new_AudioCheck(struct capn_segment*);
AdmitSeat_ptr new_AdmitSeat(struct capn_segment*);
AdmitModel_ptr new_AdmitModel(struct capn_segment*);
AgentQuery_ptr new_AgentQuery(struct capn_segment*);
ReloadShellPack_ptr new_ReloadShellPack(struct capn_segment*);

PolicyDecision_list new_PolicyDecision_list(struct capn_segment*, int len);
PolicydStatus_list new_PolicydStatus_list(struct capn_segment*, int len);
AgentStatus_list new_AgentStatus_list(struct capn_segment*, int len);
SeatCheck_list new_SeatCheck_list(struct capn_segment*, int len);
ModelCheck_list new_ModelCheck_list(struct capn_segment*, int len);
PathCheck_list new_PathCheck_list(struct capn_segment*, int len);
ShellCheck_list new_ShellCheck_list(struct capn_segment*, int len);
PathProbe_list new_PathProbe_list(struct capn_segment*, int len);
ShellView_list new_ShellView_list(struct capn_segment*, int len);
RiskCheck_list new_RiskCheck_list(struct capn_segment*, int len);
AudioCheck_list new_AudioCheck_list(struct capn_segment*, int len);
AdmitSeat_list new_AdmitSeat_list(struct capn_segment*, int len);
AdmitModel_list new_AdmitModel_list(struct capn_segment*, int len);
AgentQuery_list new_AgentQuery_list(struct capn_segment*, int len);
ReloadShellPack_list new_ReloadShellPack_list(struct capn_segment*, int len);

void read_PolicyDecision(struct PolicyDecision*, PolicyDecision_ptr);
void read_PolicydStatus(struct PolicydStatus*, PolicydStatus_ptr);
void read_AgentStatus(struct AgentStatus*, AgentStatus_ptr);
void read_SeatCheck(struct SeatCheck*, SeatCheck_ptr);
void read_ModelCheck(struct ModelCheck*, ModelCheck_ptr);
void read_PathCheck(struct PathCheck*, PathCheck_ptr);
void read_ShellCheck(struct ShellCheck*, ShellCheck_ptr);
void read_PathProbe(struct PathProbe*, PathProbe_ptr);
void read_ShellView(struct ShellView*, ShellView_ptr);
void read_RiskCheck(struct RiskCheck*, RiskCheck_ptr);
void read_AudioCheck(struct AudioCheck*, AudioCheck_ptr);
void read_AdmitSeat(struct AdmitSeat*, AdmitSeat_ptr);
void read_AdmitModel(struct AdmitModel*, AdmitModel_ptr);
void read_AgentQuery(struct AgentQuery*, AgentQuery_ptr);
void read_ReloadShellPack(struct ReloadShellPack*, ReloadShellPack_ptr);

void write_PolicyDecision(const struct PolicyDecision*, PolicyDecision_ptr);
void write_PolicydStatus(const struct PolicydStatus*, PolicydStatus_ptr);
void write_AgentStatus(const struct AgentStatus*, AgentStatus_ptr);
void write_SeatCheck(const struct SeatCheck*, SeatCheck_ptr);
void write_ModelCheck(const struct ModelCheck*, ModelCheck_ptr);
void write_PathCheck(const struct PathCheck*, PathCheck_ptr);
void write_ShellCheck(const struct ShellCheck*, ShellCheck_ptr);
void write_PathProbe(const struct PathProbe*, PathProbe_ptr);
void write_ShellView(const struct ShellView*, ShellView_ptr);
void write_RiskCheck(const struct RiskCheck*, RiskCheck_ptr);
void write_AudioCheck(const struct AudioCheck*, AudioCheck_ptr);
void write_AdmitSeat(const struct AdmitSeat*, AdmitSeat_ptr);
void write_AdmitModel(const struct AdmitModel*, AdmitModel_ptr);
void write_AgentQuery(const struct AgentQuery*, AgentQuery_ptr);
void write_ReloadShellPack(const struct ReloadShellPack*, ReloadShellPack_ptr);

void get_PolicyDecision(struct PolicyDecision*, PolicyDecision_list, int i);
void get_PolicydStatus(struct PolicydStatus*, PolicydStatus_list, int i);
void get_AgentStatus(struct AgentStatus*, AgentStatus_list, int i);
void get_SeatCheck(struct SeatCheck*, SeatCheck_list, int i);
void get_ModelCheck(struct ModelCheck*, ModelCheck_list, int i);
void get_PathCheck(struct PathCheck*, PathCheck_list, int i);
void get_ShellCheck(struct ShellCheck*, ShellCheck_list, int i);
void get_PathProbe(struct PathProbe*, PathProbe_list, int i);
void get_ShellView(struct ShellView*, ShellView_list, int i);
void get_RiskCheck(struct RiskCheck*, RiskCheck_list, int i);
void get_AudioCheck(struct AudioCheck*, AudioCheck_list, int i);
void get_AdmitSeat(struct AdmitSeat*, AdmitSeat_list, int i);
void get_AdmitModel(struct AdmitModel*, AdmitModel_list, int i);
void get_AgentQuery(struct AgentQuery*, AgentQuery_list, int i);
void get_ReloadShellPack(struct ReloadShellPack*, ReloadShellPack_list, int i);

void set_PolicyDecision(const struct PolicyDecision*, PolicyDecision_list, int i);
void set_PolicydStatus(const struct PolicydStatus*, PolicydStatus_list, int i);
void set_AgentStatus(const struct AgentStatus*, AgentStatus_list, int i);
void set_SeatCheck(const struct SeatCheck*, SeatCheck_list, int i);
void set_ModelCheck(const struct ModelCheck*, ModelCheck_list, int i);
void set_PathCheck(const struct PathCheck*, PathCheck_list, int i);
void set_ShellCheck(const struct ShellCheck*, ShellCheck_list, int i);
void set_PathProbe(const struct PathProbe*, PathProbe_list, int i);
void set_ShellView(const struct ShellView*, ShellView_list, int i);
void set_RiskCheck(const struct RiskCheck*, RiskCheck_list, int i);
void set_AudioCheck(const struct AudioCheck*, AudioCheck_list, int i);
void set_AdmitSeat(const struct AdmitSeat*, AdmitSeat_list, int i);
void set_AdmitModel(const struct AdmitModel*, AdmitModel_list, int i);
void set_AgentQuery(const struct AgentQuery*, AgentQuery_list, int i);
void set_ReloadShellPack(const struct ReloadShellPack*, ReloadShellPack_list, int i);

#ifdef __cplusplus
}
#endif
#endif
