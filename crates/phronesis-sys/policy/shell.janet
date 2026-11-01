# Product shell + audio pack entry.
# Host loads policy/lib/*.janet (sorted) into the sealed env, then this file.
# shell-check: Cap'n ShellView → PolicyDecision (danger / secrets).
# audio-check: Cap'n AudioCheck → PolicyDecision (voice gates; meta #97 Track E).

(defn- decide [decision code reason]
  (capnp/build-message 1 2
                       @[[:u16 0 decision]
                         [:u16 2 code]
                         [:text 0 reason]]))

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
  The default law: privilege, remote and decoded scripts, disks, modes,
  recursive deletes, git calls that drop work, and secrets in argv. The
  seat's package and Python rules live in policy/seat.janet, which a seat
  loads beside or instead of this file.``
  [buf]
  (def msg (capnp/message-from-buffer buf))
  (def root (capnp/root msg))
  (unless (capnp/get-bool root shell-view-under-workspace-bit)
    (break (decide Decision-deny PolicyReason-pathOutsideWorkspace
                   "path outside workspace")))
  (def argv (read-argv root))
  # Privilege / remote-exec / disks / deletes / dangerous git (policy/lib/shell-danger).
  (def danger (shell-danger-deny argv))
  (unless (nil? danger)
    (break (decide Decision-deny (in danger 0) (in danger 1))))
  # Secrets must never appear in spawn argv (policy/lib/shell-secret).
  (def secret (shell-secret-deny argv))
  (unless (nil? secret)
    (break (decide Decision-deny (in secret 0) (in secret 1))))
  (decide Decision-allow PolicyReason-shellExecAllow
          "shell exec under workspace"))

(defn audio-check
  ``Voice gate pack entry: Cap'n AudioCheck bytes in, PolicyDecision out.
  Reads AudioAction (u16 @0); product table in policy/lib/voice-law.janet.
  Host stamps agentId and may short-circuit DENY_ALL / AUDIO_ALLOW.``
  [buf]
  (def msg (capnp/message-from-buffer buf))
  (def root (capnp/root msg))
  (def action (capnp/get-u16 root audio-check-action-u16 0))
  (def pair (audio-decide action))
  (decide (in pair 0) (in pair 1) "audio gate"))
