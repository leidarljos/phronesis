# Pure shell-danger law (no Cap'n): privilege, remote and decoded scripts,
# disks, modes, recursive deletes and git calls that drop work.
# Loaded with other policy/lib/*.janet before the pack entry.
#
# These rules match ljos-policyd's built-in table rule for rule, so a host
# that runs the table and then this pack gets one law, not two. A host
# hands a line here as written and each command it runs (past env, sh -c
# and the like) as its own argv; the rules below judge each command of a
# line by its own words.

(def- fetchers ["curl" "wget" "fetch"])
(def- shells ["sh" "bash" "zsh" "dash" "ksh" "fish"])
(def- wrappers ["sudo" "doas" "env" "nohup" "time" "exec" "command" "nice"])
(def- decoders ["base64" "base32" "basenc" "xxd" "openssl" "uudecode" "gunzip"
                "zcat" "gzip" "bzcat" "bunzip2" "xzcat" "unxz" "zstdcat" "zstd"
                "rev"])
(def- substitution-opens ["$(" "<(" "`"])
# Words that run a script in the shell that reads them: source FILE, . FILE
# and eval TEXT.
(def- sourcers ["source" "." "eval"])
# Interpreters that run the program they read on stdin when the call names
# none, by base name with any version taken off (python3.12), each with the
# short flags that hand it a program inline or a module.
(def- stdin-interpreters
  {"python" "cm" "pypy" "cm" "perl" "eE" "ruby" "e" "node" "ep" "nodejs" "ep"
   "php" "rBRE" "lua" "e"})
# Short curl and wget options that take a value, so the rest of a cluster
# after one is that value: `-fsSLo` FILE writes FILE.
(def- curl-valued "AbcCdDeEFHKmoPQrTtuUwxXyYz")
(def- wget-valued "aABeDiIlOoPQRtTUwX")

(defn- member? [xs x] (truthy? (find (fn [y] (= y x)) xs)))
(defn- pre? [p s] (string/has-prefix? p s))
(defn- has-sub? [needle s] (truthy? (string/find needle s)))
(defn- has-char? [chars c] (truthy? (string/find (string/from-bytes c) chars)))

# A cluster of short flags such as -fu or -rf: one dash, then letters.
(defn- short-flags? [a]
  (and (pre? "-" a) (not (pre? "--" a)) (> (length a) 1)))

(defn- short-has? [a c]
  (and (short-flags? a) (has-sub? c (string/slice a 1))))

(defn privilege-runner? [b]
  (member? ["sudo" "su" "doas" "pkexec" "run0" "machinectl"] b))

# Where a pipeline stage's command word sits: its first word past
# wrappers, assignments and flags.
(defn- command-at [stage]
  (find-index (fn [t]
                (def b (argv-base t))
                (not (or (member? wrappers b)
                         (pre? "-" t)
                         (and (string/find "=" t) (not (pre? "=" t))))))
              stage))

# The command a pipeline stage runs: its first word past wrappers,
# assignments and flags, by base name.
(defn- command-word [stage]
  (def at (command-at stage))
  (when at (argv-base (in stage at))))

# The inline-program flags of the interpreter a command word names, version
# taken off (python3.12 is python), or nil when it names none.
(defn- stdin-interpreter [b]
  (get stdin-interpreters (string/trimr b "0123456789.")))

# The stages of a pipeline: a token `|` separates them, and so does a `|`
# inside a token with no space (`url|sh`). `||` is not a pipe.
(defn- stages [argv]
  (def out @[@[]])
  (each t argv
    (cond
      (= t "|") (array/push out @[])
      (and (string/find "|" t) (not (string/find "||" t)) (not (string/find " " t)))
      (let [parts (string/split "|" t)]
        (eachp [k part] parts
          (when (> (length part) 0) (array/push (last out) part))
          (when (< k (- (length parts) 1)) (array/push out @[]))))
      (array/push (last out) t)))
  (filter (fn [s] (> (length s) 0)) out))

(defn- runs-decoder? [t]
  (truthy?
    (find (fn [open]
            (find (fn [d] (or (string/find (string open d " ") t)
                              (string/has-suffix? (string open d) t)))
                  decoders))
          substitution-opens)))

# A decoded script handed to a shell: a decoder or decompressor piped into
# a shell (`echo ... | base64 -d | sh`), or a shell running one through
# $( ), <( ) or backticks. No reader can check what such a script runs.
(defn decoded-exec? [argv]
  (def st (stages argv))
  (var hit false)
  (for k 0 (- (length st) 1)
    (when (and (member? decoders (command-word (in st k)))
               (member? shells (command-word (in st (+ k 1)))))
      (set hit true)))
  (each s st
    (when (and (member? shells (command-word s)) (find runs-decoder? s))
      (set hit true)))
  hit)

# The commands of a line: words split at ; && || | & and at a trailing ;.
(defn- commands [argv]
  (def out @[@[]])
  (each t argv
    (if (member? [";" "&&" "||" "|" "&"] t)
      (array/push out @[])
      (let [closes (string/has-suffix? ";" t)
            tok (if closes (string/slice t 0 -2) t)]
        (when (> (length tok) 0) (array/push (last out) tok))
        (when closes (array/push out @[])))))
  (filter (fn [c] (> (length c) 0)) out))

(defn- redirection? [t]
  (def rest (string/triml t "0123456789&"))
  (or (pre? ">" rest) (pre? "<" rest)))

# A redirection word, but not a process substitution: <(curl ...) is an
# operand the command reads as a file.
(defn- plain-redirection? [a]
  (and (redirection? a) (not (pre? "<(" a)) (not (pre? ">(" a))))

# The word an interpreter or shell call runs as its program: [word true]
# for the value of an inline flag (`-c` CODE, `-e` CODE, `--eval` CODE), [word
# false] for the first operand (a script path or <(...)), and nil when the
# call names no program and so reads it from stdin.
(defn- program-word [args inline]
  (var out nil)
  (var done false)
  (var k 0)
  (def n (length args))
  (while (and (not done) (< k n))
    (def a (in args k))
    (++ k)
    (cond
      (or (= a "--") (plain-redirection? a)) nil
      (= a "-") (set done true)
      (pre? "--" a)
      (when (member? ["--eval" "--print" "--command"] a)
        (set out (when (< k n) [(in args k) true]))
        (set done true))
      (pre? "-" a)
      (when (find (fn [c] (has-char? inline c)) (string/bytes (string/slice a 1)))
        (set out (when (< k n) [(in args k) true]))
        (set done true))
      (do (set out [a false]) (set done true))))
  out)

# A pipeline stage that runs what it reads on stdin as a program: a shell,
# or an interpreter called with no program of its own (python3, perl -,
# node). python3 `-m` json.tool, python3 `-c` CODE, perl `-ne` CODE and python3
# parse.py read stdin as data.
(defn- runs-stdin? [stage]
  (def at (command-at stage))
  (when at
    (def b (argv-base (in stage at)))
    (def inline (stdin-interpreter b))
    (or (member? shells b)
        (and inline (nil? (program-word (slice stage (+ at 1)) inline))))))

# The pipelines of a line: its words split at ; && || & and after a word
# that ends in ;. The | words stay, so each piece is one pipeline.
(defn- pipelines [argv]
  (def out @[@[]])
  (each t argv
    (if (member? [";" "&&" "||" "&"] t)
      (array/push out @[])
      (let [closes (string/has-suffix? ";" t)
            tok (if closes (string/trimr t ";") t)]
        (when (> (length tok) 0) (array/push (last out) tok))
        (when closes (array/push out @[])))))
  (filter (fn [p] (> (length p) 0)) out))

# A word that runs a download through $( ), <( ) or backticks: <(curl,
# $( wget, $(/usr/bin/curl.
(defn- runs-download? [t]
  (var hit false)
  (each open substitution-opens
    (each i (string/find-all open t)
      (def rest (string/triml (string/slice t (+ i (length open)))))
      (def word (first (peg/match ~(<- (any (if-not (set " \t\n)`|;") 1))) rest)))
      (when (and word (member? fetchers (argv-base word)))
        (set hit true))))
  hit)

# A stage that runs a download it reads through a substitution: a shell or
# source, . or eval with one on the line (bash <(curl ...), source <(curl
# ...), eval "$(curl ...)"), or an interpreter whose program is one
# (python3 <(curl ...), python3 `-c` "$(curl ...)").
(defn- runs-substituted-download? [s]
  (def at (command-at s))
  (if (nil? at)
    false
    (let [b (argv-base (in s at))
          inline (stdin-interpreter b)]
      (cond
        (or (member? shells b) (member? sourcers b)) (truthy? (find runs-download? s))
        (nil? inline) false
        (let [pw (program-word (slice s (+ at 1)) inline)]
          (truthy? (and pw (runs-download? (in pw 0)))))))))

# The last path part of a URL, the name curl `-O` and wget save it as.
(defn- url-file-name [url]
  (def path (first (string/split "#" (first (string/split "?" url)))))
  (def at (string/find "://" path))
  (def tail (if at (string/slice path (+ at 3)) path))
  (def parts (string/split "/" (string/trimr tail "/")))
  (def f (last parts))
  (when (and (> (length parts) 1) (> (length f) 0)) f))

# The files a curl or wget call writes, by last path part: the value of `-o`,
# `--output`, `-O` (wget) or `--output-document`, or the URL's own name for curl
# `-O` and for wget with no `-O`. Standard output (-) and /dev/null are no file.
(defn- fetched-files [cmd]
  (def at (command-at cmd))
  (def tool (when at (argv-base (in cmd at))))
  (if (not (member? ["curl" "wget"] tool))
    @[]
    (do
      (def args (slice cmd (+ at 1)))
      (def curl? (= tool "curl"))
      (def valued (if curl? curl-valued wget-valued))
      (def out-flag (if curl? (chr "o") (chr "O")))
      (def long-out (if curl? "--output" "--output-document"))
      (def named @[])
      (var remote (not curl?))
      (var i 0)
      (def n (length args))
      (while (< i n)
        (def a (in args i))
        (def nxt (get args (+ i 1)))
        (++ i)
        (cond
          (= a long-out) (when nxt (array/push named nxt) (++ i))
          (pre? (string long-out "=") a)
          (array/push named (string/slice a (+ 1 (length long-out))))
          (and curl? (member? ["--remote-name" "--remote-name-all"] a)) (set remote true)
          (and (pre? "-" a) (not (pre? "--" a)) (> (length a) 1))
          (do
            (def flags (string/slice a 1))
            (var k 0)
            (var stop false)
            (while (and (not stop) (< k (length flags)))
              (def c (in flags k))
              (cond
                (and curl? (= c (chr "O"))) (set remote true)
                (and (not curl?) (= c (chr "n"))) (set stop true)
                (has-char? valued c)
                (do
                  (def rest (string/slice flags (+ k 1)))
                  (def value (if (= rest "") (do (++ i) nxt) rest))
                  (when (and value (= c out-flag)) (array/push named value))
                  (set stop true)))
              (++ k)))))
      # wget `-O` FILE (or `-O` -) saves nothing under the URL's own name.
      (def wget-named (and (not curl?) (> (length named) 0)))
      (when (and remote (not wget-named))
        (each u args
          (when (string/find "://" u)
            (def f (url-file-name u))
            (when f (array/push named f)))))
      (filter (fn [f] (> (length f) 0))
              (map argv-base
                   (filter (fn [f] (not (member? ["" "-" "/dev/null" "/dev/stdout"] f)))
                           named))))))

# A command that runs one of saved (files by last path part): a path with
# a / as its command word (./i.sh, /tmp/i.sh), or a shell, source, . or
# interpreter whose script operand is one (sh i.sh, python3 /tmp/get.py).
(defn- runs-saved-file? [cmd saved]
  (def at (command-at cmd))
  (if (nil? at)
    false
    (do
      (def word (in cmd at))
      (def b (argv-base word))
      (defn named? [p] (member? saved (argv-base p)))
      (def inline (cond (member? shells b) "c"
                        (member? ["source" "."] b) ""
                        (stdin-interpreter b)))
      (cond
        (and (string/find "/" word) (named? word)) true
        (nil? inline) false
        (let [pw (program-word (slice cmd (+ at 1)) inline)]
          (truthy? (and pw (not (in pw 1)) (named? (in pw 0)))))))))

# A file a fetch on the line writes, then a later command on the line runs:
# curl `-o` i.sh URL && sh i.sh, curl `-fsSLO` URL/x.sh && ./x.sh, wget
# URL/get.py; python3 get.py. curl `-o` data.json URL && jq . data.json runs
# nothing it fetched.
(defn fetched-then-run? [argv]
  (def saved @[])
  (var hit false)
  (each cmd (commands argv)
    (when (and (> (length saved) 0) (runs-saved-file? cmd saved))
      (set hit true))
    (array/concat saved (fetched-files cmd)))
  hit)

# A download handed to a program runner: a fetching stage with a later
# stage of its pipeline that runs stdin (curl URL | sh, curl URL | python3,
# curl URL | tee f | sh), a shell, source, ., eval or an interpreter that
# runs a download through a substitution, a shell whose `-c` script is any of
# these, or a file a fetch on the line writes and a later command runs.
# Naming a download tool and a shell on one line is not that: git fetch,
# then bash build.sh, runs nothing fetched, and curl URL | python3 `-m`
# json.tool reads the download as data.
(defn remote-exec? [argv]
  (var hit false)
  (each line (pipelines argv)
    (def st (stages line))
    (eachp [k s] st
      (when (and (member? fetchers (command-word s))
                 (find runs-stdin? (slice st (+ k 1))))
        (set hit true))
      (when (runs-substituted-download? s) (set hit true))
      (when (member? shells (command-word s))
        (for j 0 (- (length s) 1)
          (when (and (= (in s j) "-c")
                     (remote-exec? (filter (fn [w] (> (length w) 0))
                                           (string/split " " (in s (+ j 1))))))
            (set hit true))))))
  (or hit (fetched-then-run? argv)))

(defn- climbs? [p]
  (truthy? (find (fn [part] (= part "..")) (string/split "/" p))))

(defn- under-tmp? [p]
  (and (not (climbs? p))
       (or (= p "/tmp") (pre? "/tmp/" p)
           (= p "/var/tmp") (pre? "/var/tmp/" p))))

# Directories a build, a package manager or a test run writes and can
# write again.
(def build-output-dirs
  ["node_modules" "target" "dist" "build" "_build" ".venv" "venv"
   "__pycache__" ".pytest_cache" ".mypy_cache" ".ruff_cache" ".tox" ".nox"
   ".next" ".nuxt" ".svelte-kit" ".turbo" ".parcel-cache" ".angular"
   ".gradle" "coverage" "htmlcov"])

(defn- plain-byte? [c]
  (or (and (>= c 48) (<= c 57)) (and (>= c 65) (<= c 90))
      (and (>= c 97) (<= c 122))
      (member? [46 95 45 43 64 47] c)))

# A path written in plain characters that stays below the working
# directory: not absolute, no ~, no $, no glob, no `..` and no `.` part.
# A leading ./ and a trailing / are allowed.
(defn plain-relative? [p]
  (var rest p)
  (while (pre? "./" rest) (set rest (string/slice rest 2)))
  (set rest (string/trimr rest "/"))
  (and (> (length rest) 0)
       (not (pre? "-" rest))
       (all plain-byte? (string/bytes rest))
       (all (fn [part] (and (> (length part) 0) (not= part ".") (not= part "..")))
            (string/split "/" rest))))

# A plain relative path inside a build output directory, or the directory
# itself: target/, ./build, web/node_modules.
(defn build-output? [p]
  (and (plain-relative? p)
       (truthy? (find (fn [part] (member? build-output-dirs part))
                      (string/split "/" p)))))

# A cd, pushd or popd on the line that can leave the working tree: an
# absolute target, ~, -, .., a variable, or none at all (home).
(defn leaves-tree? [argv]
  (truthy?
    (find
      (fn [cmd]
        (def b (argv-base (in cmd 0)))
        (cond
          (= b "popd") true
          (or (= b "cd") (= b "pushd"))
          (let [t (find (fn [a] (or (not (pre? "-" a)) (= a "-"))) (slice cmd 1))]
            (or (nil? t) (not (plain-relative? t))))
          false))
      (commands argv))))

# Scratch directories a project keeps at its top for throwaway output.
(def scratch-dirs ["tmp" ".tmp" "temp"])

# A plain relative path under a scratch directory at the top of the tree,
# or the directory itself: tmp/, ./tmp/test-output. A scratch name deeper
# down (src/tmp) says nothing about what the path holds.
(defn scratch? [p]
  (var rest p)
  (while (pre? "./" rest) (set rest (string/slice rest 2)))
  (def parts (string/split "/" rest))
  (and (plain-relative? p)
       (member? scratch-dirs (in parts 0))
       (not (member? parts ".git"))))

# A recursive rm or rtrash with an operand that is neither under /tmp or
# /var/tmp, nor a build output directory or a top-level scratch directory
# named as a plain relative path. When the line can cd out of the tree,
# only /tmp operands pass.
(defn- recursive-delete? [cmd moved]
  (def b (argv-base (in cmd 0)))
  (and (or (= b "rm") (= b "rtrash"))
       (or (truthy? (find (fn [a] (and (short-flags? a) (has-sub? "r" a) (has-sub? "f" a)))
                          (slice cmd 1)))
           (and (truthy? (find (fn [a] (member? ["-r" "-R" "--recursive"] a)) cmd))
                (truthy? (find (fn [a] (member? ["-f" "--force"] a)) cmd))))
       (truthy? (find (fn [a] (and (not (pre? "-" a))
                                   (not (redirection? a))
                                   (not (under-tmp? a))
                                   (or moved (not (or (build-output? a) (scratch? a))))))
                      (slice cmd 1)))))

(defn recursive-delete-off-tmp? [argv]
  (def moved (leaves-tree? argv))
  (truthy? (find (fn [cmd] (recursive-delete? cmd moved)) (commands argv))))

# Names a compiler, an interpreter, an editor or a desktop writes and
# writes again: bytecode, objects, patch leftovers, editor swap files and
# folder metadata. A find -name cleanup of these loses no work.
(def regenerable-names
  ["*.pyc" "*.pyo" "__pycache__" ".pytest_cache" ".mypy_cache" ".ruff_cache"
   "*.o" "*.obj" "*.class" "*.gcda" "*.gcno" "*.orig" "*.rej" "*.swp" "*.swo"
   "*~" ".DS_Store" "._*" "Thumbs.db" "desktop.ini"])

# A find cleanup inside the working tree: every starting point is . or a
# plain relative path outside .git, and every name test names a file in
# regenerable-names, with at least one. An -o, a !, a -not or a group can
# turn a name test around, so none passes. `find . -delete` has no name
# test and `find . -name '*.rs' -delete` names source, so both stay refused.
(defn- regenerable-cleanup? [starts expr]
  (defn in-tree? [p]
    (def q (string/trimr p "/"))
    (and (or (= q ".") (plain-relative? q))
         (not (member? (string/split "/" q) ".git"))))
  (var ok (and (all in-tree? starts)
               (not (find (fn [a] (member? ["-o" "-or" "!" "-not" "," "(" ")" "\\(" "\\)"] a))
                          expr))))
  (var named 0)
  (def n (length expr))
  (for k 0 (- n 1)
    (def test (in expr k))
    (def value (in expr (+ k 1)))
    (cond
      (member? ["-name" "-iname"] test)
      (if (member? regenerable-names value) (++ named) (set ok false))
      (member? ["-path" "-ipath" "-wholename"] test)
      (let [parts (filter (fn [part] (and (not= part "*") (not= part ".")))
                          (string/split "/" value))]
        (if (and (> (length parts) 0) (all (fn [part] (member? regenerable-names part)) parts))
          (++ named)
          (set ok false)))
      (member? ["-regex" "-iregex" "-lname" "-ilname"] test)
      (set ok false)))
  (and ok (> named 0)))

# find ... -delete, or -exec rm (also -execdir, -ok, shred, unlink), whose
# starting points are not all under /tmp. No starting point is `.`. A
# cleanup of files a tool writes again inside the working tree is allowed,
# unless the line can cd out of the tree first.
(defn- find-delete? [cmd moved]
  (def n (length cmd))
  (var exec-delete false)
  (for k 0 (- n 1)
    (when (and (member? ["-exec" "-execdir" "-ok" "-okdir"] (in cmd k))
               (member? ["rm" "shred" "unlink"] (argv-base (in cmd (+ k 1)))))
      (set exec-delete true)))
  (and (= (argv-base (in cmd 0)) "find")
       (or (member? cmd "-delete") exec-delete)
       (let [starts (take-while (fn [a] (and (not (pre? "-" a))
                                             (not (member? ["(" "!" "\\("] a))))
                                (slice cmd 1))
             expr (slice cmd (+ 1 (length starts)))]
         (and (not (and (not (empty? starts)) (all under-tmp? starts)))
              (or moved (not (regenerable-cleanup? starts expr)))))))

(defn find-delete-off-tmp? [argv]
  (def moved (leaves-tree? argv))
  (truthy? (find (fn [cmd] (find-delete? cmd moved)) (commands argv))))

# A mode that sets the setuid or setgid bit: an octal mode of four or more
# digits whose leading digit holds 4 or 2 (4755, 04755, 2775), or a symbolic
# clause that adds s (u+s, g+s, +s, u=rwxs). 0755, 755, +x and g-s set
# neither.
(defn- sets-id-bit? [mode]
  (def n (length mode))
  (if (and (>= n 4) (all (fn [c] (and (>= c 48) (<= c 55))) (string/bytes mode)))
    (not= 0 (band (- (in mode (- n 4)) 48) 6))
    (truthy?
      (find
        (fn [clause]
          (def ops (string/triml clause "ugoa"))
          (and (> (length ops) 0)
               (member? [43 45 61] (in ops 0))
               (all (fn [c] (has-char? "+-=rwxXstugo" c)) (string/bytes ops))
               (do
                 (var adds false)
                 (var hit false)
                 (each c (string/bytes ops)
                   (cond
                     (or (= c 43) (= c 61)) (set adds true)
                     (= c 45) (set adds false)
                     (and (= c 115) adds) (set hit true)))
                 hit)))
        (string/split "," mode)))))

# The mode a chmod or install call sets: chmod's first word that is not a
# flag, or the value of install's `-m`, `-mMODE`, `--mode=MODE` or `--mode` MODE.
(defn- mode-operand [argv]
  (def b (argv-base (in argv 0)))
  (def args (slice argv 1))
  (cond
    (= b "chmod") (find (fn [a] (not (pre? "-" a))) args)
    (= b "install")
    (do
      (var out nil)
      (var done false)
      (var k 0)
      (def n (length args))
      (while (and (not done) (< k n))
        (def a (in args k))
        (++ k)
        (cond
          (= a "--mode") (do (set out (get args k)) (set done true))
          (pre? "--mode=" a) (do (set out (string/slice a 7)) (set done true))
          (and (pre? "-" a) (not (pre? "--" a)) (> (length a) 1))
          (do
            (def flags (string/slice a 1))
            (var j 0)
            (var stop false)
            (while (and (not stop) (< j (length flags)))
              (def c (in flags j))
              (cond
                (= c (chr "m"))
                (do
                  (def rest (string/slice flags (+ j 1)))
                  (set out (if (= rest "") (get args k) rest))
                  (set done true)
                  (set stop true))
                (has-char? "gotS" c) (set stop true))
              (++ j)))))
      out)
    nil))

# A chmod or install that sets the setuid or setgid bit.
(defn setuid-mode? [argv]
  (and (> (length argv) 0)
       (let [m (mode-operand argv)]
         (truthy? (and m (sets-id-bit? m))))))

# A block device: a whole disk, a partition, or a mapped or RAID volume.
# /dev/null, /dev/zero and the other character devices are not one.
(defn block-device? [p]
  (and (pre? "/dev/" p)
       (let [name (string/slice p 5)]
         (truthy? (find (fn [d] (pre? d name))
                        ["sd" "hd" "vd" "xvd" "nvme" "mmcblk" "disk" "md" "dm-"
                         "mapper/" "loop" "rbd" "nbd" "zd"])))))

# A redirection that writes to a block device: > /dev/sda, >/dev/sda,
# 1>> /dev/nvme0n1.
(defn- writes-block-device? [argv]
  (var hit false)
  (eachp [i a] argv
    (def rest (string/triml a "0123456789&"))
    (when (pre? ">" rest)
      (def target (string/triml (string/slice rest 1) ">|"))
      (when (if (= target "")
              (let [nxt (get argv (+ i 1))] (and nxt (block-device? nxt)))
              (block-device? target))
        (set hit true))))
  hit)

# Tools that rewrite a partition table, a filesystem signature or a whole
# block device, each with the flags that only read.
(def- disk-writers
  {"wipefs" ["-n" "--no-act" "-h" "--help"]
   "sgdisk" ["-p" "--print" "-v" "--verify" "-i" "--info" "-h" "--help"]
   "sfdisk" ["-l" "--list" "-d" "--dump" "-J" "--json" "-s" "--show-size"
             "-V" "--verify" "-h" "--help"]
   "fdisk" ["-l" "--list" "-h" "--help"]
   "gdisk" ["-l" "-h" "--help"]
   "cfdisk" ["-h" "--help"]
   "parted" ["-l" "--list" "print" "-h" "--help"]
   "blkdiscard" ["-h" "--help"]
   "mkswap" ["-h" "--help"]
   "shred" ["-h" "--help"]
   "badblocks" []})

# A command that overwrites a disk: mkfs on a device (or with no target),
# dd writing to a block device, a partitioning or wiping tool on a block
# device, cryptsetup formatting or erasing one, badblocks -w on one.
# `mkfs.ext4 -F disk.img` formats a file and `dd of=/dev/null` writes
# nowhere, so neither is refused.
(defn- raw-disk-command? [cmd]
  (def b (argv-base (in cmd 0)))
  (def args (slice cmd 1))
  (def operands (filter (fn [a] (not (pre? "-" a))) args))
  (cond
    (pre? "mkfs" b)
    (do
      (def targets @[])
      (var skip false)
      (each a args
        (cond
          skip (set skip false)
          (member? ["-t" "--type" "-L" "-b" "-O" "-E" "-m"] a) (set skip true)
          (not (pre? "-" a)) (array/push targets a)))
      (or (empty? targets)
          (truthy? (find (fn [t] (or (pre? "/dev/" t) (has-sub? "$" t))) targets))))
    (= b "dd")
    (truthy? (find (fn [a] (and (pre? "of=" a)
                                (let [t (string/slice a 3)]
                                  (or (block-device? t) (has-sub? "$" t)))))
                   args))
    (= b "cryptsetup")
    (truthy? (find (fn [a] (member? ["luksFormat" "erase" "luksErase" "reencrypt"] a))
                   operands))
    (and (= b "wipefs")
         (not (find (fn [a] (or (member? ["--all" "--offset"] a)
                                (pre? "--offset=" a)
                                (short-has? a "a") (short-has? a "o")))
                    args)))
    false
    (= b "badblocks")
    (and (member? args "-w") (truthy? (find block-device? operands)))
    (let [reads (get disk-writers b)]
      (and reads
           (truthy? (find block-device? operands))
           (not (find (fn [a] (member? reads a)) args))))))

(defn raw-disk? [argv]
  (or (writes-block-device? argv)
      (truthy? (find raw-disk-command? (commands argv)))))

# Paths a recursive mode or owner change must not start from: the root, a
# home directory, the system trees.
(defn- system-or-home-tree? [p]
  (def q (string/trimr p "/"))
  (cond
    (or (= q "") (climbs? q)) true
    (member? ["~" "$HOME" "${HOME}" "/home" "/root" "/Users"] q) true
    (pre? "/home/" q) (not (has-sub? "/" (string/slice q 6)))
    (pre? "/Users/" q) (not (has-sub? "/" (string/slice q 7)))
    (and (not (or (= q "/var/tmp") (pre? "/var/tmp/" q)))
         (truthy? (find (fn [r] (or (= q r) (pre? (string r "/") q)))
                        ["/etc" "/usr" "/bin" "/sbin" "/lib" "/lib64" "/boot" "/opt"
                         "/srv" "/var" "/sys" "/proc" "/dev" "/System" "/Library"
                         "/private"])))))

(defn- digits? [s]
  (and (> (length s) 0) (all (fn [c] (and (>= c 48) (<= c 57))) (string/bytes s))))

# A mode that takes every permission away (000, a-rwx) or gives everyone
# write (777, o+w, a+w).
(defn- sweeping-mode? [m]
  (if (and (digits? m) (>= (length m) 3) (<= (length m) 4))
    (let [n (length m)
          d (fn [i] (- (in m i) 48))
          u (d (- n 3)) g (d (- n 2)) o (d (- n 1))]
      (or (and (= u 0) (= g 0) (= o 0)) (not= 0 (band o 2))))
    (truthy?
      (find
        (fn [clause]
          (def at (or (find-index (fn [c] (member? [43 45 61] c)) (string/bytes clause))
                      (length clause)))
          (def who (string/slice clause 0 at))
          (def rest (string/slice clause at))
          (def everyone (or (= who "") (has-sub? "a" who) (has-sub? "o" who)))
          (or (and everyone (pre? "+" rest) (has-sub? "w" rest))
              (and (or (has-sub? "a" who) (= who "ugo"))
                   (pre? "-" rest)
                   (has-sub? "r" rest) (has-sub? "w" rest) (has-sub? "x" rest))))
        (string/split "," m)))))

(defn- mode-word? [a]
  (and (> (length a) 1)
       (all (fn [c] (member? [114 119 120 88 115 116] c)) (string/bytes (string/slice a 1)))))

# A recursive chmod, chown, chgrp or setfacl that starts at the root, a home
# directory or a system tree, or a recursive chmod that takes every
# permission away or hands everyone write outside /tmp.
(defn- recursive-mode-command? [cmd]
  (def b (argv-base (in cmd 0)))
  (def args (slice cmd 1))
  (and (member? ["chmod" "chown" "chgrp" "setfacl"] b)
       (truthy? (find (fn [a] (or (= a "--recursive") (short-has? a "R"))) args))
       (let [words (filter (fn [a] (or (not (pre? "-" a)) (and (= b "chmod") (mode-word? a))))
                           args)
             spec (if (= b "setfacl") nil (get words 0))
             paths (if (= b "setfacl") words (slice words (min 1 (length words))))]
         (and (or (= b "setfacl") (not (nil? spec)))
              (or (truthy? (find system-or-home-tree? paths))
                  (and (= b "chmod")
                       (sweeping-mode? spec)
                       (truthy? (find (fn [p] (not (or (= p "/tmp") (pre? "/tmp/" p)
                                                       (pre? "/var/tmp" p))))
                                      paths))))))))

(defn recursive-mode-change? [argv]
  (truthy? (find recursive-mode-command? (commands argv))))

# A git push that can overwrite or drop history on the remote with no
# check: `--force`, a short cluster holding f (`-fu`), `--mirror`, or a refspec
# starting with +. A `--force-with-lease` push is judged by
# lease-push-guarded? below.
(defn- force-push? [cmd]
  (def at (find-index (fn [a] (= a "push")) cmd))
  (and at
       (truthy? (find (fn [a] (or (= a "--force") (= a "--mirror")
                                  (short-has? a "f")
                                  (and (pre? "+" a) (> (length a) 1))))
                      (slice cmd (+ at 1))))))

# The branches a lease push may not replace on its own. ljos turns this
# refusal into a question for the person, and adds the protected branches
# a checkout names in its own config. Mirrors ljos-policyd's LEASE_GUARDED.
(def- lease-guarded ["main" "master" "develop"])

# git push options that take the next word as their value.
(def- push-valued ["-o" "--push-option" "--receive-pack" "--exec" "--repo"])

(defn- lease-flag? [a]
  (or (= a "--force-with-lease") (pre? "--force-with-lease=" a)))

# The branch a name means on the remote, refs/heads/ taken off. nil when
# the words do not say: empty, HEAD, or a word the shell still expands or
# globs ($b, *).
(defn- lease-branch [name]
  (def n (if (pre? "refs/heads/" name) (string/slice name 11) name))
  (unless (or (= n "") (= n "HEAD")
              (truthy? (find (fn [c] (has-sub? c n)) ["$" "`" "*" "?" "["])))
    n))

# The branch a refspec writes on the remote: the dst of src:dst, else the
# refspec itself, a leading + taken off.
(defn- refspec-branch [spec]
  (def s (if (pre? "+" spec) (string/slice spec 1) spec))
  (def colon (string/find ":" s))
  (lease-branch (if colon (string/slice s (+ colon 1)) s)))

# A `--force-with-lease` push (any form, with `--force-if-includes` or not)
# whose destination is main, master or develop, or that its words do not
# name: no refspec, HEAD, a wildcard, `--all` or `--branches`. The destination
# is each refspec's branch on the remote, and a ref the lease names
# (`--force-with-lease=main:abc123`) counts too. The first word past the
# options is the remote. A lease push to any other branch passes: the
# remote refuses it when it holds commits the pusher has not seen. Mirrors
# ljos-policyd's lease_push_guarded.
(defn- lease-push-guarded? [cmd]
  (def at (find-index (fn [a] (= a "push")) cmd))
  (def rest (if at (slice cmd (+ at 1)) []))
  (when (find lease-flag? rest)
    (def words @[])
    (def named @[])
    (var every false)
    (var skip false)
    (var tail false)
    (each a rest
      (cond
        tail (array/push words a)
        skip (set skip false)
        (= a "--") (set tail true)
        (member? push-valued a) (set skip true)
        (member? ["--all" "--branches"] a) (set every true)
        (pre? "--force-with-lease=" a)
        (let [v (string/slice a 19)
              colon (string/find ":" v)
              name (if colon (string/slice v 0 colon) v)]
          (unless (= name "") (array/push named [(lease-branch name)])))
        (pre? "-" a) nil
        (array/push words a)))
    (def specs (if (> (length words) 1) (slice words 1) []))
    (or every
        (empty? specs)
        (not (nil? (find-index (fn [b] (or (nil? b) (member? lease-guarded b)))
                               (array/concat (map refspec-branch specs)
                                             (map first named))))))))

# Git's subcommand index past global options (-C DIR, -c KEY=VALUE, ...).
(defn- git-subcommand-at [cmd]
  (var i 1)
  (var at nil)
  (def n (length cmd))
  (while (and (nil? at) (< i n))
    (def a (in cmd i))
    (cond
      (member? ["-C" "-c" "--git-dir" "--work-tree" "--namespace"] a) (+= i 2)
      (pre? "-" a) (++ i)
      (set at i)))
  at)

(defn- whole-tree? [p] (member? ["." "./" ":/" ":" "*" "-A"] p))

# A git call that throws away work no commit holds. Returns its reason, or
# nil. Mirrors ljos-policyd's table: force push, remote ref delete, reset
# --hard, clean -f, stash clear, branch -D, reflog expire to now or delete,
# filter-branch, update-ref -d, worktree remove --force, and checkout or
# restore of the whole tree.
(defn git-dangerous [cmd]
  (def at (git-subcommand-at cmd))
  (def sub (when at (in cmd at)))
  (def rest (if at (slice cmd (+ at 1)) []))
  (def first-word (get rest 0))
  (defn short [c] (truthy? (find (fn [a] (short-has? a c)) rest)))
  (defn has [f] (member? rest f))
  (cond
    (force-push? cmd) "git-force-push: force push denied"
    (lease-push-guarded? cmd)
    "git-force-push: lease push onto main, master, develop or an unnamed branch denied"
    (nil? sub) nil
    (and (= sub "push")
         (find (fn [a] (or (= a "--delete") (short-has? a "d")
                           (and (pre? ":" a) (> (length a) 1))))
               rest))
    "git-push-delete: deleting a remote ref denied"
    (and (= sub "reset") (has "--hard")) "git-reset-hard: reset --hard denied"
    (and (= sub "clean") (or (short "f") (has "--force"))) "git-clean-force: clean -f denied"
    (and (= sub "stash") (= first-word "clear")) "git-stash-clear: stash clear denied"
    (and (= sub "branch") (or (short "D") (and (has "--delete") (or (has "--force") (short "f")))))
    "git-branch-force-delete: branch -D denied"
    (and (= sub "reflog")
         (or (= first-word "delete")
             (and (= first-word "expire")
                  (find (fn [a] (and (pre? "--expire" a)
                                     (or (string/has-suffix? "=now" a)
                                         (string/has-suffix? "=all" a))))
                        rest))))
    "git-reflog-expire: dropping reflog entries denied"
    (member? ["filter-branch" "filter-repo"] sub) "git-history-rewrite: history rewrite denied"
    (and (= sub "update-ref") (short "d")) "git-update-ref-delete: update-ref -d denied"
    (and (= sub "worktree") (= first-word "remove") (or (short "f") (has "--force")))
    "git-worktree-force-remove: worktree remove --force denied"
    (= sub "checkout")
    (let [dd (find-index (fn [a] (= a "--")) rest)
          after (if dd (slice rest (+ dd 1)) rest)]
      (when (and (find whole-tree? after) (or dd (= (length rest) 1)))
        "git-discard-worktree: discarding the whole tree denied"))
    (= sub "restore")
    (do
      (def paths @[])
      (var skip false)
      (each a rest
        (cond
          skip (set skip false)
          (member? ["-s" "--source"] a) (set skip true)
          (not (pre? "-" a)) (array/push paths a)))
      (when (and (not (find (fn [a] (member? ["-p" "--patch"] a)) rest))
                 (or (empty? paths) (find whole-tree? paths)))
        "git-discard-worktree: discarding the whole tree denied"))
    nil))

# Each git command on the line, or one whose command word is still a
# variable ($g, ${GIT}) and so could be git.
(defn git-dangerous? [argv]
  (var reason nil)
  (each cmd (commands argv)
    (def b (argv-base (in cmd 0)))
    (when (and (nil? reason) (or (= b "git") (pre? "$" b)))
      (set reason (git-dangerous cmd))))
  reason)

# Returns a deny pair [code reason] or nil if this module has no opinion.
# Each reason starts with the token ljos-policyd's table uses.
(defn shell-danger-deny [argv]
  (when (find (fn [t] (privilege-runner? (argv-base t))) argv)
    (break [PolicyReason-shellPrivilegeDenied
            "sudo: privilege runner denied (sudo/su/doas/pkexec)"]))
  (when (remote-exec? argv)
    (break [PolicyReason-shellRemoteExec
            "curl-pipe-shell: a download run by a shell or an interpreter denied"]))
  (when (decoded-exec? argv)
    (break [PolicyReason-shellRemoteExec
            "decoded-pipe-shell: a decoded script handed to a shell denied"]))
  (when (find setuid-mode? (commands argv))
    (break [PolicyReason-shellDangerousRunner
            "chmod-setuid: a setuid or setgid mode denied"]))
  (when (raw-disk? argv)
    (break [PolicyReason-shellDangerousRunner
            "raw-disk: writing a block device denied (mkfs, dd, wipefs, a redirect)"]))
  (when (recursive-mode-change? argv)
    (break [PolicyReason-shellDangerousRunner
            "recursive-chmod-chown: a recursive mode or owner change of a system or home tree denied"]))
  (when (recursive-delete-off-tmp? argv)
    (break [PolicyReason-shellDangerousRunner
            "rm-rf-outside-tmp: recursive delete outside /tmp and the build output directories denied"]))
  (when (find-delete-off-tmp? argv)
    (break [PolicyReason-shellDangerousRunner
            "find-delete-outside-tmp: find -delete outside /tmp denied"]))
  (def g (git-dangerous? argv))
  (when g
    (break [PolicyReason-shellGitDangerous g]))
  nil)
