# Opt-in seat pack: the default law of policy/shell.janet plus the seat's
# package and Python rules (uv run with PEP 723 for Python; pixi in place
# of pip, npm, yarn, pnpm, bun, poetry and the like).
#
# Load it instead of shell.janet, or beside it, for example
#   PHRONESIS_JANET_PACK=$prefix/share/phronesis/policy/seat.janet
# Packs compose fail-closed, so loading both refuses what either refuses.

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
  ``Seat pack entry: Cap'n ShellView bytes in, PolicyDecision out.
  The default law first, then the seat's package rules, then its Python
  law: uv run, and PEP 723 metadata when a .py path is present.``
  [buf]
  (def msg (capnp/message-from-buffer buf))
  (def root (capnp/root msg))
  (unless (capnp/get-bool root shell-view-under-workspace-bit)
    (break (decide Decision-deny PolicyReason-pathOutsideWorkspace
                   "path outside workspace")))
  (def argv (read-argv root))
  (def danger (shell-danger-deny argv))
  (unless (nil? danger)
    (break (decide Decision-deny (in danger 0) (in danger 1))))
  (def secret (shell-secret-deny argv))
  (unless (nil? secret)
    (break (decide Decision-deny (in secret 0) (in secret 1))))
  (def package (seat-package-deny argv))
  (unless (nil? package)
    (break (decide Decision-deny (in package 0) (in package 1))))
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

(defn audio-check
  ``Voice gate entry, the same as policy/shell.janet's, so this pack can
  stand alone.``
  [buf]
  (def msg (capnp/message-from-buffer buf))
  (def root (capnp/root msg))
  (def action (capnp/get-u16 root audio-check-action-u16 0))
  (def pair (audio-decide action))
  (decide (in pair 0) (in pair 1) "audio gate"))
