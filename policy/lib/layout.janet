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

(def PolicyReason-pathOutsideWorkspace 2)
(def PolicyReason-pythonRequiresUvRun 17)
(def PolicyReason-pythonDashCDenied 18)
(def PolicyReason-pythonMissingPep723 19)
(def PolicyReason-shellExecAllow 20)
(def PolicyReason-shellDangerousRunner 23)
(def PolicyReason-shellRemoteExec 24)
(def PolicyReason-shellPrivilegeDenied 25)
(def PolicyReason-shellGitDangerous 26)
