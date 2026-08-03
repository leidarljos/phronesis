# Product shell content pack entry.
# Host loads policy/lib/*.janet (sorted) into the sealed env, then this file.
# In: Cap'n ShellView buffer. Out: Cap'n PolicyDecision buffer.

(defn- decide [decision code reason]
  (capnp/build-message 1 2
                       @[[:u16 0 decision]
                         [:u16 2 code]
                         [:text 0 reason]]))

(defn- any-pep723? [root]
  (def probes-ptr (capnp/getp root shell-view-path-probes-ptr))
  (def n (capnp/list-len probes-ptr))
  (var hit false)
  (var i 0)
  (while (< i n)
    (def el (capnp/list-getp probes-ptr i))
    (when (and (capnp/get-bool el path-probe-exists-bit)
               (pep723? (capnp/get-text el path-probe-head-ptr)))
      (set hit true))
    (set i (+ i 1)))
  hit)

(defn- read-argv [root]
  (def lp (capnp/getp root shell-view-argv-ptr))
  (def n (capnp/list-len lp))
  (def out @[])
  (var i 0)
  (while (< i n)
    (array/push out (capnp/list-get-text lp i))
    (set i (+ i 1)))
  out)

(defn shell-check
  ``Shell content pack entry: Cap'n ShellView bytes in, PolicyDecision out.
  Python paths require uv run (+ PEP 723 when a .py path is present).
  Pure helpers live in policy/lib/ (loaded by the host before this file).``
  [buf]
  (def msg (capnp/message-from-buffer buf))
  (def root (capnp/root msg))
  (unless (capnp/get-bool root shell-view-under-workspace-bit)
    (break (decide Decision-deny PolicyReason-pathOutsideWorkspace
                   "path outside workspace")))
  (def argv (read-argv root))
  # Privilege / remote-exec / banned runners / dangerous git (policy/lib/shell-danger).
  (def danger (shell-danger-deny argv))
  (unless (nil? danger)
    (break (decide Decision-deny (in danger 0) (in danger 1))))
  # Python product law (policy/lib/python-law): uv run + PEP 723.
  (unless (touches-python? argv)
    (break (decide Decision-allow PolicyReason-shellExecAllow
                   "shell exec under workspace")))
  (unless (uv-run? argv)
    (break (decide Decision-deny PolicyReason-pythonRequiresUvRun
                   "python requires uv run + PEP 723")))
  (when (python-dash-c? argv)
    (break (decide Decision-deny PolicyReason-pythonDashCDenied
                   "python -c denied; use uv run --script")))
  (when (and (has-py-path? argv) (not (any-pep723? root)))
    (break (decide Decision-deny PolicyReason-pythonMissingPep723
                   "python missing PEP 723 metadata")))
  (decide Decision-allow PolicyReason-shellExecAllow
          "shell exec under workspace"))