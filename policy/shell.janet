# Product shell content pack.
# In: Cap'n ShellView. Out: Cap'n PolicyDecision.

(def Decision-deny 0)
(def Decision-allow 1)

(def PolicyReason-pathOutsideWorkspace 2)
(def PolicyReason-pythonRequiresUvRun 17)
(def PolicyReason-pythonDashCDenied 18)
(def PolicyReason-pythonMissingPep723 19)
(def PolicyReason-shellExecAllow 20)

(defn decide [decision code reason]
  (capnp/build-message 1 2
                       @[[:u16 0 decision]
                         [:u16 2 code]
                         [:text 0 reason]]))

(defn argv-base [tok]
  (def parts (string/split "/" tok))
  (def n (length parts))
  (if (= n 0) tok (in parts (- n 1))))

(defn digits-only? [s]
  (var ok true)
  (each c (string/bytes s)
    (when (or (< c 48) (> c 57))
      (set ok false)))
  ok)

(defn is-python-interp? [b]
  (or (= b "python") (= b "pypy") (= b "pypy3")
      (and (> (length b) 6)
           (= (string/slice b 0 6) "python")
           (digits-only? (string/slice b 6)))))

(defn touches-python? [argv]
  (var hit false)
  (each t argv
    (def b (argv-base t))
    (when (or (is-python-interp? b) (string/has-suffix? ".py" t))
      (set hit true)))
  hit)

(defn uv-run? [argv]
  (var saw-uv false)
  (var hit false)
  (each t argv
    (when (= (argv-base t) "uv") (set saw-uv true))
    (when (and saw-uv (= t "run")) (set hit true)))
  hit)

(defn python-dash-c? [argv]
  (var prev-py false)
  (var hit false)
  (each t argv
    (def b (argv-base t))
    (when (and prev-py (= t "-c")) (set hit true))
    (set prev-py (is-python-interp? b)))
  hit)

(defn has-py-path? [argv]
  (var hit false)
  (each t argv
    (when (string/has-suffix? ".py" t) (set hit true)))
  hit)

(defn pep723? [head]
  (def a (string/find "# /// script" head))
  (if (nil? a)
    false
    (not (nil? (string/find "# ///" (string/slice head (+ a 12)))))))

(defn any-pep723? [root]
  (def probes-ptr (capnp/getp root 2))
  (def n (capnp/list-len probes-ptr))
  (var hit false)
  (var i 0)
  (while (< i n)
    (def el (capnp/list-getp probes-ptr i))
    (when (and (capnp/get-bool el 0)
               (pep723? (capnp/get-text el 2)))
      (set hit true))
    (set i (+ i 1)))
  hit)

(defn read-argv [root]
  (def lp (capnp/getp root 1))
  (def n (capnp/list-len lp))
  (def out @[])
  (var i 0)
  (while (< i n)
    (array/push out (capnp/list-get-text lp i))
    (set i (+ i 1)))
  out)

(defn shell-check [buf]
  (def msg (capnp/message-from-buffer buf))
  (def root (capnp/root msg))
  (unless (capnp/get-bool root 0)
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
