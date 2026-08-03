# Product shell content pack.
# In: Cap'n ShellView (buffer). Out: Cap'n PolicyDecision (buffer).
# Host loads this single file via dobytes; do not import other modules.

# Cap'n encoding indices (data bit / pointer slot among pointers).
# Not the same as schema field @n numbers. Keep in sync with policy.capnp packing.
(def- shell-view-under-workspace-bit 0)
(def- shell-view-argv-ptr 1)
(def- shell-view-path-probes-ptr 2)
(def- path-probe-exists-bit 0)
(def- path-probe-head-ptr 2)

(def Decision-deny 0)
(def Decision-allow 1)

(def PolicyReason-pathOutsideWorkspace 2)
(def PolicyReason-pythonRequiresUvRun 17)
(def PolicyReason-pythonDashCDenied 18)
(def PolicyReason-pythonMissingPep723 19)
(def PolicyReason-shellExecAllow 20)

(defn- decide [decision code reason]
  (capnp/build-message 1 2
                       @[[:u16 0 decision]
                         [:u16 2 code]
                         [:text 0 reason]]))

(defn- argv-base [tok]
  (def parts (string/split "/" tok))
  (def n (length parts))
  (if (= n 0) tok (in parts (- n 1))))

(defn- digits-only? [s]
  (if (= (length s) 0)
    false
    (all (fn [c] (and (>= c 48) (<= c 57))) (string/bytes s))))

# Digit/dot version after "python" (3, 3.12, 3.12.1). Not free-threading suffixes.
(defn- python-version-suffix? [s]
  (var i 0)
  (def n (length s))
  (var saw-digit false)
  (while (< i n)
    (def c (in s i))
    (cond
      (and (>= c 48) (<= c 57))
      (do (set saw-digit true) (set i (+ i 1)))
      (= c 46)
      (if (not saw-digit)
        (break false)
        (do (set saw-digit false) (set i (+ i 1))))
      (break false)))
  saw-digit)

(defn- python-interp? [b]
  (or (= b "python")
      (= b "pypy")
      (= b "pypy3")
      (and (> (length b) 6)
           (= (string/slice b 0 6) "python")
           (python-version-suffix? (string/slice b 6)))
      (and (> (length b) 4)
           (= (string/slice b 0 4) "pypy")
           (digits-only? (string/slice b 4)))))

(defn- touches-python? [argv]
  (truthy?
    (find (fn [t]
            (or (python-interp? (argv-base t))
                (string/has-suffix? ".py" t)))
          argv)))

(defn- uv-run? [argv]
  (var saw-uv false)
  (var hit false)
  (each t argv
    (when (= (argv-base t) "uv") (set saw-uv true))
    (when (and saw-uv (= t "run")) (set hit true)))
  hit)

(defn- python-dash-c? [argv]
  (var prev-py false)
  (var hit false)
  (each t argv
    (def b (argv-base t))
    (when (and prev-py (= t "-c")) (set hit true))
    (set prev-py (python-interp? b)))
  hit)

(defn- has-py-path? [argv]
  (truthy?
    (find (fn [t] (string/has-suffix? ".py" t)) argv)))

# Substring open/close; line-anchored PEG is a follow-on (see vault review).
(defn- pep723? [head]
  (def a (string/find "# /// script" head))
  (and a
       (string/find "# ///" (string/slice head (+ a 12)))))

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
  Python paths require uv run (+ PEP 723 when a .py path is present).``
  [buf]
  (def msg (capnp/message-from-buffer buf))
  (def root (capnp/root msg))
  (unless (capnp/get-bool root shell-view-under-workspace-bit)
    (break (decide Decision-deny PolicyReason-pathOutsideWorkspace
                   "path outside workspace")))
  (def argv (read-argv root))
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
