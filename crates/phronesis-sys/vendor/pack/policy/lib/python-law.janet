# Pure shell-content law helpers (no Cap'n, no FS).
# Unit-tested via tests/test_pack_lib.c (embedded Janet). Host loads this
# before policy/shell.janet into the sealed pack env.

(defn argv-base [tok]
  (def parts (string/split "/" tok))
  (def n (length parts))
  (if (= n 0) tok (in parts (- n 1))))

(defn digits-only? [s]
  (if (= (length s) 0)
    false
    (all (fn [c] (and (>= c 48) (<= c 57))) (string/bytes s))))

# Digit/dot version after "python" (3, 3.12, 3.12.1). No free-threading suffixes.
(defn python-version-suffix? [s]
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

(defn python-interp? [b]
  (or (= b "python")
      (= b "pypy")
      (= b "pypy3")
      (and (> (length b) 6)
           (= (string/slice b 0 6) "python")
           (python-version-suffix? (string/slice b 6)))
      (and (> (length b) 4)
           (= (string/slice b 0 4) "pypy")
           (digits-only? (string/slice b 4)))))

(defn touches-python? [argv]
  (truthy?
    (find (fn [t]
            (or (python-interp? (argv-base t))
                (string/has-suffix? ".py" t)))
          argv)))

# uv ... run (non-adjacent; intervening global flags allowed).
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
    (set prev-py (python-interp? b)))
  hit)

(defn has-py-path? [argv]
  (truthy?
    (find (fn [t] (string/has-suffix? ".py" t)) argv)))

# PEP 723 script block on its own lines (open then later close).
# bol = byte offset 0 or after LF/CR. Requires a real close fence after open.
(def pep723-peg
  (peg/compile
    ~{:main (* (any (if-not :open 1)) :open (any (if-not :close 1)) :close)
      :nl (+ "\n" "\r\n" "\r")
      :ws (any (set " \t"))
      :bol (+ (cmt (position) ,|(= $ 0))
              (look -1 "\n")
              (look -1 "\r"))
      :open (* :bol :ws "# /// script" :ws (+ :nl -1))
      :close (* :bol :ws "# ///" :ws (+ :nl -1))}))

(defn pep723? [head]
  (truthy? (peg/match pep723-peg head)))
