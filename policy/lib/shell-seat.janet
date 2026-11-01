# Pure seat-file law (no Cap'n, no FS): a write onto a file the seat keeps
# for itself, which the host publishes as a warning. Loaded with the other
# policy/lib/*.janet before the pack entry.
#
# The seat files are the agent hook files, the ljos and ljos-policyd config
# directories, the ljos git hooks directory and the global git config. They
# are matched by the end of a spelled path under a home: `~`, `$HOME`,
# `${HOME}`, an XDG variable, or any absolute directory. A relative path is
# in the work tree. The host names a path through a link on the disk by its
# target, and a relative path after a `cd` by where it lands; a link made by
# `ln` earlier on the same line is followed here.

(defn- st-pre? [p s] (string/has-prefix? p s))
(defn- st-suf? [p s] (string/has-suffix? p s))
(defn- st-member? [xs x] (truthy? (find (fn [y] (= y x)) xs)))
(defn- st-base [tok] (last (string/split "/" tok)))

(def- seat-files
  ["/.cursor/hooks.json" "/.claude/settings.json" "/.claude/settings.local.json"
   "/.gitconfig" "/.config/git/config"])

(def- seat-dirs
  ["/.config/ljos" "/.config/ljos-policyd" "/.local/share/ljos/git-hooks"])

(def- home-spellings
  [["${XDG_CONFIG_HOME}" "~/.config"] ["$XDG_CONFIG_HOME" "~/.config"]
   ["${XDG_DATA_HOME}" "~/.local/share"] ["$XDG_DATA_HOME" "~/.local/share"]
   ["${HOME}" "~"] ["$HOME" "~"]])

# The path a word spells: quotes off, a home or XDG variable as `~`, and
# `.`, `..` and repeated or trailing slashes folded. Keeps a leading `~`
# or `/`.
(defn- spelled [w]
  (var p (string/replace-all "'" "" (string/replace-all "\"" "" w)))
  (each [from to] home-spellings
    (when (st-pre? from p) (set p (string to (string/slice p (length from))))))
  (def tilde (st-pre? "~" p))
  (def abs (st-pre? "/" p))
  (def body (if tilde (string/slice p 1) p))
  (def parts @[])
  (each part (string/split "/" body)
    (cond
      (or (= part "") (= part ".")) nil
      (= part "..") (array/pop parts)
      (array/push parts part)))
  (def joined (string/join parts "/"))
  (cond
    tilde (if (empty? parts) "~" (string "~/" joined))
    abs (string "/" joined)
    joined))

# A seat file or a path inside a seat directory, with something in front
# of it: `/.cursor/hooks.json` alone names no home.
(defn- seat-path? [p]
  (and (or (st-pre? "/" p) (st-pre? "~" p))
       (or (truthy? (find (fn [f] (and (st-suf? f p) (> (length p) (length f)))) seat-files))
           (truthy? (find (fn [d]
                            (or (and (st-suf? d p) (> (length p) (length d)))
                                (let [at (string/find (string d "/") p)] (and at (> at 0)))))
                          seat-dirs)))))

(defn- st-commands [argv]
  (def out @[@[]])
  (each t argv
    (if (st-member? [";" "&&" "||" "|" "&"] t)
      (array/push out @[])
      (let [closes (and (st-suf? ";" t) (not (st-suf? "\\;" t)))
            tok (if closes (string/slice t 0 -2) t)]
        (when (> (length tok) 0) (array/push (last out) tok))
        (when closes (array/push out @[])))))
  (filter (fn [c] (> (length c) 0)) out))

(def- redirect-peg
  ~(* (? (+ (range "09") "&")) (+ ">>" ">|" ">") (<- (any 1)) -1))

# [targets rest]: the files a command's output redirections write, and the
# command's other words. `2>&1` names no file.
(defn- redirections [cmd]
  (def targets @[])
  (def rest @[])
  (var i 0)
  (while (< i (length cmd))
    (def w (in cmd i))
    (def m (peg/match redirect-peg w))
    (if m
      (let [attached (in m 0)
            target (if (= attached "") (get cmd (+ i 1)) attached)]
        (when (and target (not (st-pre? "&" target)))
          (array/push targets target))
        (+= i (if (= attached "") 2 1)))
      (do (array/push rest w) (++ i))))
  [targets rest])

(def- st-wrappers
  ["env" "nohup" "nice" "time" "command" "builtin" "exec" "stdbuf" "ionice"
   "timeout" "sudo" "doas" "setsid"])

(defn- st-assignment? [w]
  (truthy? (peg/match ~(* (+ (range "az" "AZ") "_") (any (+ (range "az" "AZ" "09") "_")) "=") w)))

# The command's program and its words, past assignments and the wrappers
# that run the rest of the line, with their flags.
(defn- st-run [words]
  (var at 0)
  (def n (length words))
  (while (and (< at n) (st-assignment? (in words at))) (++ at))
  (while (and (< at n) (st-member? st-wrappers (st-base (in words at))))
    (def wrapper (st-base (in words at)))
    (++ at)
    (while (and (< at n)
                (let [w (in words at)]
                  (or (st-pre? "-" w) (st-assignment? w)
                      (and (= wrapper "timeout")
                           (peg/match ~(* (some (range "09")) (? (set "smhd")) -1) w)))))
      (++ at)))
  (slice words at))

(defn- operands [args]
  (def out @[])
  (var all false)
  (each a args
    (cond
      all (array/push out a)
      (= a "--") (set all true)
      (and (st-pre? "-" a) (not= a "-")) nil
      (array/push out a)))
  out)

(defn- flag-value [args shorts longs]
  ``The values of a flag: `-o F`, `-oF`, `-sSLo F`, `--output F`,
  `--output=F`.``
  (def out @[])
  (var i 0)
  (while (< i (length args))
    (def a (in args i))
    (++ i)
    (cond
      (st-member? longs a) (when (< i (length args)) (array/push out (in args i)) (++ i))
      (find (fn [l] (st-pre? (string l "=") a)) longs)
      (array/push out (string/slice a (+ 1 (string/find "=" a))))
      (and (st-pre? "-" a) (not (st-pre? "--" a)) (> (length a) 1))
      (do
        (var k 1)
        (var done false)
        (while (and (not done) (< k (length a)))
          (def c (string/slice a k (+ k 1)))
          (when (st-member? shorts c)
            (set done true)
            (if (= (+ k 1) (length a))
              (when (< i (length args)) (array/push out (in args i)) (++ i))
              (array/push out (string/slice a (+ k 1)))))
          (++ k)))))
  out)

(defn- joined [dir member]
  (if (or (st-pre? "/" member) (st-pre? "~" member)) member (string dir "/" member)))

(defn- tar-writes [args]
  (var dir ".")
  (var extract false)
  (var create false)
  (var archive nil)
  (def members @[])
  (var i 0)
  (var first true)
  (while (< i (length args))
    (def a (in args i))
    (++ i)
    (cond
      (st-member? ["--extract" "--get"] a) (set extract true)
      (= a "--create") (set create true)
      (st-member? ["-C" "--directory"] a) (when (< i (length args)) (set dir (in args i)) (++ i))
      (st-pre? "--directory=" a) (set dir (string/slice a 12))
      (st-member? ["-f" "--file"] a) (when (< i (length args)) (set archive (in args i)) (++ i))
      (st-pre? "--file=" a) (set archive (string/slice a 7))
      (st-pre? "--" a) nil
      (or (and (st-pre? "-" a) (> (length a) 1))
          (and first (peg/match ~(* (some (set "xtczfjJhvOpC")) -1) a)))
      (let [cluster (string/triml a "-")]
        (when (string/find "x" cluster) (set extract true))
        (when (string/find "c" cluster) (set create true))
        (each c ["f" "C"]
          (when-let [k (string/find c cluster)]
            (def tail (string/slice cluster (+ k 1)))
            (def value (if (and (= tail "") (< i (length args)))
                         (do (++ i) (in args (- i 1)))
                         (if (= tail "") nil tail)))
            (if (= c "f") (set archive value) (when value (set dir value))))))
      (array/push members a))
    (set first false))
  (cond
    extract (if (empty? members) [dir] (map (fn [m] (joined dir m)) members))
    (and create archive) [archive]
    []))

(defn- unzip-writes [args]
  (var dir ".")
  (def members @[])
  (var zip true)
  (var i 0)
  (while (< i (length args))
    (def a (in args i))
    (++ i)
    (cond
      (= a "-d") (when (< i (length args)) (set dir (in args i)) (++ i))
      (st-pre? "-d" a) (set dir (string/slice a 2))
      (and (st-pre? "-" a) (not= a "-")) nil
      zip (set zip false)
      (array/push members a)))
  (if (empty? members) [dir] (map (fn [m] (joined dir m)) members)))

(defn- copy-writes [args]
  (def ops (operands args))
  (def dest (last ops))
  (def out @[;(flag-value args ["t"] ["--target-directory"])])
  (when (and dest (> (length ops) 1))
    (array/push out dest)
    (each src (slice ops 0 -2) (array/push out (joined dest (st-base src)))))
  out)

(def- inline-write-calls
  ["open" "rename" "link" "unlink" "mkdir" "rmdir" "truncate" "copy" "move"
   "chmod" "utime" "write"])

(defn- literals [code]
  (or (peg/match ~(any (+ (* "'" (<- (any (if-not "'" 1))) "'")
                          (* "\"" (<- (any (if-not "\"" 1))) "\"")
                          1))
                 code)
      @[]))

(defn- inline-writes [run]
  (def code (inline-program run))
  (if (and code (find (fn [c] (string/find c code)) inline-write-calls))
    (literals code)
    []))

(defn- in-place? [args flag]
  (truthy? (find (fn [a] (or (st-pre? (string "--" flag) a)
                             (and (st-pre? "-" a) (not (st-pre? "--" a)) (string/find "i" a))))
                 args)))

(def- git-valued ["-C" "-c" "--git-dir" "--work-tree" "--namespace" "--config-env"])

(defn- abbrev? [a long] (and (>= (length a) 4) (st-pre? a long)))

# What a git call writes into the seat's git setup: `core.hooksPath` set
# with -c, or by `git config` at global or system scope, or a config file
# `git config --file` writes.
(defn- git-writes [args]
  (var hit nil)
  (var i 0)
  (while (and (< i (length args)) (st-pre? "-" (in args i)))
    (def a (in args i))
    (when (and (= a "-c") (< (+ i 1) (length args))
               (st-pre? "core.hookspath=" (string/ascii-lower (in args (+ i 1)))))
      (set hit "core.hooksPath"))
    (+= i (if (st-member? git-valued a) 2 1)))
  (when (and (nil? hit) (= (get args i) "config"))
    (def rest (slice args (+ i 1)))
    (def files (flag-value rest ["f"] ["--file"]))
    (def file-values (map (fn [f] f) files))
    (def ops (filter (fn [a] (not (st-member? file-values a))) (operands rest)))
    (def changes (find (fn [a] (st-member? ["--unset" "--unset-all" "--replace-all" "--add" "--edit" "-e"] a)) rest))
    (def reads (find (fn [a] (or (st-pre? "--get" a) (st-member? ["--list" "-l"] a))) rest))
    (def writes (or changes (and (not reads) (>= (length ops) 2))))
    (def wide (find (fn [a] (or (abbrev? a "--global") (abbrev? a "--system"))) rest))
    (when writes
      (cond
        (and wide (= (string/ascii-lower (get ops 0 "")) "core.hookspath")) (set hit "core.hooksPath")
        (find (fn [f] (seat-path? (spelled f))) files) (set hit (spelled (find (fn [f] (seat-path? (spelled f))) files))))))
  hit)

# The paths a command writes, by its program.
(defn- written [run]
  (def head (st-base (in run 0)))
  (def args (slice run 1))
  (def stem (or (get (peg/match ~(<- (any (if-not (range "09") 1))) head) 0) head))
  (cond
    (st-member? ["tee" "sponge" "ln" "truncate" "touch" "mkdir" "rm" "rmdir" "chmod" "chown" "patch"] head)
    (operands args)
    (st-member? ["cp" "install" "rsync" "mv"] head) (copy-writes args)
    (= head "dd") (map (fn [a] (string/slice a 3)) (filter (fn [a] (st-pre? "of=" a)) args))
    (= head "sed") (if (in-place? args "in-place") (operands args) [])
    (= head "perl") (if (in-place? args "in-place") (operands args) (inline-writes run))
    (st-member? ["awk" "gawk"] head) (if (find (fn [a] (= a "inplace")) args) (operands args) [])
    (= head "curl") (flag-value args ["o"] ["--output"])
    (= head "wget") (flag-value args ["O"] ["--output-document"])
    (= head "tar") (tar-writes args)
    (= head "unzip") (unzip-writes args)
    (st-member? ["python" "pypy" "node" "ruby" "deno" "bun"] stem) (inline-writes run)
    []))

(defn shell-seat-warn [argv]
  ``[code reason :warn] for a write onto a seat file, nil otherwise. The
  reason is `seat-guard-path: PATH`.``
  (def links @[])
  (var hit nil)
  (defn through-links [p]
    (var out p)
    (each [link target] links
      (cond
        (= out link) (set out target)
        (st-pre? (string link "/") out) (set out (string target (string/slice out (length link))))))
    out)
  (each cmd (st-commands argv)
    (unless hit
      (def [targets words] (redirections cmd))
      (def run (st-run words))
      (def paths @[;targets])
      (when (> (length run) 0)
        (if (= (st-base (in run 0)) "git")
          (when-let [g (git-writes (slice run 1))] (set hit g))
          (array/concat paths (written run))))
      (each w paths
        (unless hit
          (def p (through-links (spelled w)))
          (when (seat-path? p) (set hit p))))
      (when (and (> (length run) 0) (= (st-base (in run 0)) "ln"))
        (def ops (operands (slice run 1)))
        (when (= (length ops) 2)
          (array/push links [(spelled (in ops 1)) (spelled (in ops 0))])))))
  (when hit
    [PolicyReason-pathSensitiveDeny (string "seat-guard-path: " hit) :warn]))
