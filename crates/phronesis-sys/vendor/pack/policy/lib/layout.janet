# Cap'n encoding indices for ShellView / PathProbe (data bit / pointer slot).
# Not schema field @n numbers. Keep in sync with schema/policy.capnp packing.
# Loaded by host before shell.janet (same sealed env).

(def shell-view-under-workspace-bit 0)
(def shell-view-argv-ptr 1)
(def shell-view-path-probes-ptr 2)
(def path-probe-exists-bit 0)
(def path-probe-head-ptr 2)

(def Decision-deny 0)
(def Decision-allow 1)
(def Decision-prompt 2)

# AudioCheck.action — u16 data offset 0 (schema AudioAction ordinals).
(def audio-check-action-u16 0)

(def AudioAction-micOpen 0)
(def AudioAction-listenArm 1)
(def AudioAction-alwaysListen 2)
(def AudioAction-networkStt 3)
(def AudioAction-inject 4)

(def PolicyReason-pathOutsideWorkspace 2)
(def PolicyReason-pythonRequiresUvRun 17)
(def PolicyReason-pythonDashCDenied 18)
(def PolicyReason-pythonMissingPep723 19)
(def PolicyReason-shellExecAllow 20)
(def PolicyReason-shellDangerousRunner 23)
(def PolicyReason-shellRemoteExec 24)
(def PolicyReason-shellPrivilegeDenied 25)
(def PolicyReason-shellGitDangerous 26)
(def PolicyReason-shellSecretInArgv 27)
(def PolicyReason-pathSensitiveDeny 28)
(def PolicyReason-secretExportDenied 29)
# checkAudio product reasons (meta #97 Track E) — match Cap'n PolicyReason @30–@36
(def PolicyReason-audioMicOpenDeny 30)
(def PolicyReason-audioListenArmPrompt 31)
(def PolicyReason-audioAlwaysListenDeny 32)
(def PolicyReason-audioNetworkSttDeny 33)
(def PolicyReason-audioInjectDeny 34)
(def PolicyReason-audioFixtureAllow 35)
(def PolicyReason-audioUnknownAction 36)
