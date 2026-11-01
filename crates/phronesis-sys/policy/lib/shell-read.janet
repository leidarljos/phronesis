# Pure credential-read law (no Cap'n, no FS): a line that reads a credential
# store, or uploads one. Loaded with the other policy/lib/*.janet before the
# pack entry.
#
# The host hands each command as its own argv and resolves what needs the
# file system (a symlink that points at a credential store reaches this file
# as the store's path). These rules only read the words.
#
# A `wc` that only counts (`-c`, `-l`, `-m`, `-w`) is a count, not a read.
# So is that count together with `head -c` of at most 4 bytes of the same
# path. Any other reader is a read: `head -c` of 5 or more, `head -n`, plain
# `head`, and `head -c` of 4 bytes with no count of that path on the line.

(def- read-credential-parts
  [".ssh" ".gnupg" ".aws" ".azure" ".kube" ".docker" ".password-store"
   ".netrc" "_netrc" ".git-credentials" ".npmrc" ".pypirc" ".pgpass"
   ".vault-token" ".secrets"])

(def- read-credential-paths
  [".config/gh/" ".config/gcloud" ".config/hub" ".config/ljos/"
   ".cargo/credentials" ".local/share/keyrings" ".codex/auth.json"
   ".claude/.credentials.json" "/etc/shadow" "/etc/gshadow" "/etc/sudoers"])

(def- file-readers
  ["cat" "tac" "head" "tail" "less" "more" "most" "bat" "batcat" "nl" "od"
   "xxd" "hexdump" "strings" "base64" "base32" "cp" "scp" "rsync" "tar" "zip"
   "gzip" "zcat" "bzcat" "xzcat" "dd" "install" "diff" "cmp" "sort" "uniq"
   "cut" "paste" "wc" "md5sum" "sha1sum" "sha256sum" "sha512sum" "b2sum"
   "cksum" "grep" "egrep" "fgrep" "sed" "awk" "gawk" "jq" "yq"])

(defn- rd-pre? [p s] (string/has-prefix? p s))

(defn- rd-member? [xs x] (truthy? (find (fn [y] (= y x)) xs)))

(defn- rd-base [tok]
  (def parts (string/split "/" tok))
  (if (empty? parts) tok (last parts)))

(defn- rd-unquote [w] (string/trim w "\"'"))

(defn- rd-lower [s] (string/replace-all "\\" "/" (string/ascii-lower s)))

(defn- rd-contains? [hay needle] (truthy? (string/find needle hay)))

# `seats.toml` and `ljos.toml` under ~/.config/ljos are config, not a
# credential, unless the word names some other store as well.
(defn- ljos-config-file? [low]
  (when (rd-contains? low ".config/ljos")
    (def trimmed (string/trimr low "/"))
    (def name (rd-base trimmed))
    (when (or (= name "seats.toml") (= name "ljos.toml"))
      (def rest (->> low
                     (string/replace-all ".config/ljos/" " ")
                     (string/replace-all ".config/ljos" " ")))
      (and (not (find |(rd-contains? rest $) read-credential-parts))
           (not (find |(and (not= $ ".config/ljos/") (rd-contains? rest $))
                      read-credential-paths))))))

(defn credential-path? [w]
  ``Whether the word names a credential store by a path part such as `.ssh`,
  a known store path, or a process environment under /proc.``
  (def low (rd-lower (rd-unquote w)))
  (if (ljos-config-file? low)
    false
    (let [parts (string/split "/" low)]
      (or (truthy? (find |(rd-member? read-credential-parts $) parts))
          (truthy? (find |(rd-contains? low $) read-credential-paths))
          (string/has-suffix? ".config/ljos" low)
          (and (rd-pre? "/proc/" low) (string/has-suffix? "/environ" low))))))

(defn- rd-assignment? [w]
  (def m (peg/match ~(* (some (+ :w "_")) (? (* "[" (to "]") "]")) (+ "=" "+=")) w))
  (and (truthy? m) (not (peg/match ~(range "09") w))))

(defn- rd-redirection-body [a]
  (string/triml a "0123456789&"))

(defn- rd-redirection? [a]
  (def rest (rd-redirection-body a))
  (or (rd-pre? ">" rest) (rd-pre? "<" rest)))

(defn- rd-commands [argv]
  (def out @[@[]])
  (each t argv
    (if (rd-member? [";" "&&" "||" "|" "&"] t)
      (array/push out @[])
      (let [closes (string/has-suffix? ";" t)
            tok (if closes (string/slice t 0 -2) t)]
        (when (> (length tok) 0) (array/push (last out) tok))
        (when closes (array/push out @[])))))
  (filter (fn [c] (> (length c) 0)) out))

(def- rd-wrappers ["env" "nohup" "nice" "time" "command" "exec" "stdbuf" "ionice" "timeout"])

# The program word of a command: past assignments, redirections and the
# wrappers that run the rest of the line, with their flags.
(defn- rd-head-at [cmd]
  (var at (find-index |(and (not (rd-assignment? $)) (not (rd-redirection? $))) cmd))
  (while (and at (rd-member? rd-wrappers (rd-base (in cmd at))))
    (def wrapper (rd-base (in cmd at)))
    (++ at)
    (while (and (< at (length cmd))
                (let [w (in cmd at)]
                  (or (rd-pre? "-" w) (rd-assignment? w) (rd-redirection? w)
                      (and (= wrapper "timeout") (peg/match ~(* (some (range "09")) (? (set "smhd")) -1) w)))))
      (++ at))
    (when (>= at (length cmd)) (set at nil)))
  at)

(defn- wc-count-flag? [w]
  (cond
    (rd-member? ["--bytes" "--lines" "--chars" "--words"] w) true
    (rd-pre? "--" w) false
    (and (rd-pre? "-" w) (> (length w) 1))
    (all |(rd-member? [(chr "c") (chr "l") (chr "m") (chr "w")] $) (string/slice w 1))
    false))

(defn- wc-count-command? [cmd]
  (def at (rd-head-at cmd))
  (when (and at (= (rd-base (in cmd at)) "wc"))
    (var saw false)
    (var opts true)
    (var ok true)
    (each w (slice cmd (+ at 1))
      (cond
        (or (rd-redirection? w) (rd-assignment? w)) nil
        (and opts (= w "--")) (set opts false)
        (and opts (rd-pre? "-" w))
        (if (wc-count-flag? w) (set saw true) (set ok false))))
    (and ok saw)))

(defn- byte-count [raw]
  (when (and (> (length raw) 0) (all |(and (>= $ (chr "0")) (<= $ (chr "9"))) raw))
    (scan-number raw)))

# `head -c N` or `head --bytes N`, and only that: :not-head, [:bytes N], or
# :other for a line count, a second mode, or a bare `head`.
(defn- head-bytes [cmd]
  (def at (rd-head-at cmd))
  (if (or (nil? at) (not= (rd-base (in cmd at)) "head"))
    :not-head
    (do
      (def args (slice cmd (+ at 1)))
      (var n nil)
      (var bad false)
      (var opts true)
      (var i 0)
      (defn take [raw]
        (def k (byte-count raw))
        (if (and k (nil? n)) (set n k) (set bad true)))
      (while (and (not bad) (< i (length args)))
        (def w (in args i))
        (cond
          (or (rd-redirection? w) (rd-assignment? w)) (++ i)
          (and opts (= w "--")) (do (set opts false) (++ i))
          (and opts (rd-pre? "-" w))
          (cond
            (rd-pre? "--bytes=" w) (do (take (string/slice w 8)) (++ i))
            (or (= w "--bytes") (= w "-c"))
            (if (< (+ i 1) (length args))
              (do (take (in args (+ i 1))) (+= i 2))
              (set bad true))
            (and (rd-pre? "-c" w) (> (length w) 2)) (do (take (string/slice w 2)) (++ i))
            (set bad true))
          (++ i)))
      (cond bad :other n [:bytes n] :other))))

# The words of a file-reader call that name what it reads. The place a
# copy, an archive or `dd` writes to is left to the write checks.
(defn- read-sources [head args]
  (def out @[])
  (var skip false)
  (for i 0 (length args)
    (def w (in args i))
    (cond
      skip (set skip false)
      (rd-redirection? w) nil
      (let [takes (case head
                    "cp" (rd-member? ["-t" "--target-directory"] w)
                    "install" (rd-member? ["-t" "--target-directory"] w)
                    "rsync" (rd-member? ["-t" "--target-directory"] w)
                    "scp" (rd-member? ["-t" "--target-directory"] w)
                    "tar" (rd-member? ["-C" "--directory"] w)
                    "sort" (rd-member? ["-o" "--output"] w)
                    false)]
        takes) (set skip true)
      (let [write-opt (or (rd-pre? "--target-directory=" w)
                          (rd-pre? "--directory=" w)
                          (rd-pre? "--output=" w)
                          (and (= head "dd") (not (rd-pre? "if=" w))))
            after-redir (and (> i 0) (rd-redirection? (in args (- i 1))))]
        (unless (or write-opt after-redir) (array/push out w)))))
  (when (and (rd-member? ["cp" "install" "rsync" "scp"] head)
             (not (find |(or (= $ "-t") (rd-pre? "--target-directory" $)) args)))
    (var last-at nil)
    (eachp [j w] out (unless (rd-pre? "-" w) (set last-at j)))
    (when last-at (array/remove out last-at)))
  (when (= head "zip")
    (def first-at (find-index |(not (rd-pre? "-" $)) out))
    (when first-at (array/remove out first-at)))
  out)

(defn- file-upload [v]
  (def body (first (string/split ";" v)))
  (def spec (rd-unquote (last (string/split "=" body))))
  (def p (cond (rd-pre? "@" spec) (string/slice spec 1)
               (rd-pre? "<" spec) (string/slice spec 1)
               nil))
  (when (and p (> (length p) 0)) p))

# Files a curl or wget call uploads. `@file` and `<file` are file reads; a
# literal data string is not.
(defn- upload-files [head args]
  (def flags (if (= head "wget")
               ["--post-file" "--body-file"]
               ["--upload-file" "-T" "-d" "--data" "--data-binary"
                "--data-urlencode" "-F" "--form"]))
  (def out @[])
  (var i 0)
  (while (< i (length args))
    (def a (in args i))
    (if (rd-member? flags a)
      (do
        (when (< (+ i 1) (length args))
          (def v (in args (+ i 1)))
          (if (or (= head "wget") (rd-member? ["-T" "--upload-file"] a))
            (array/push out (string/triml v "@"))
            (when-let [p (file-upload v)] (array/push out p))))
        (+= i 2))
      (do
        (if (= head "curl")
          (let [v (some |(when (rd-pre? $ a) (string/slice a (length $)))
                        ["--data=" "--data-binary=" "--data-urlencode=" "--form=" "-d" "-F"])]
            (cond
              (and (rd-pre? "-T" a) (> (length a) 2))
              (array/push out (rd-unquote (string/triml (string/slice a 2) "@")))
              (rd-pre? "--upload-file=" a)
              (array/push out (string/triml (string/slice a 14) "@"))
              (and v (> (length v) 0))
              (when-let [p (file-upload v)] (array/push out p))))
          (cond
            (rd-pre? "--post-file=" a) (array/push out (string/slice a 12))
            (rd-pre? "--body-file=" a) (array/push out (string/slice a 12))))
        (++ i))))
  out)

(defn credential-read [argv]
  ``The credential read on the line: [:deny BY PATH], [:count BY PATH] for a
  count that prints no content, or nil.``
  (def counts @[])
  (def prefixes @[])
  (var deny nil)
  (defn note-deny [by path] (unless deny (set deny [by path])))
  (each cmd (rd-commands argv)
    (def counting (wc-count-command? cmd))
    (def prefix (head-bytes cmd))
    (var k 0)
    (while (< k (length cmd))
      (def w (in cmd k))
      (++ k)
      (def body (rd-redirection-body w))
      (when (and (rd-pre? "<" body)
                 (not (rd-pre? "<<" body))
                 (not (rd-pre? "<(" body)))
        (def inline (string/triml body "<>"))
        (def target (if (= inline "") (get cmd k "") inline))
        (when (credential-path? target)
          (cond
            counting (array/push counts ["<" target])
            (tuple? prefix) (array/push prefixes ["<" target (in prefix 1)])
            (note-deny "<" target)))))
    (def at (rd-head-at cmd))
    (when at
      (def head (rd-base (in cmd at)))
      (def args (slice cmd (+ at 1)))
      (when (rd-member? file-readers head)
        (when-let [p (find credential-path? (read-sources head args))]
          (cond
            (and counting (= head "wc")) (array/push counts [head p])
            (= head "head")
            (if (tuple? prefix)
              (array/push prefixes [head p (in prefix 1)])
              (note-deny head p))
            (note-deny head p))))
      (when (rd-member? ["curl" "wget"] head)
        (when-let [p (find credential-path? (upload-files head args))]
          (note-deny head p)))))
  (def same (fn [a b] (= (rd-unquote a) (rd-unquote b))))
  (def over (find (fn [[_ path n]]
                    (or (> n 4) (not (find (fn [[_ c]] (same c path)) counts))))
                  prefixes))
  (cond
    deny [:deny ;deny]
    over [:deny (in over 0) (in over 1)]
    (not (empty? counts)) [:count ;(first counts)]
    nil))

(defn shell-read-deny [argv]
  ``[code reason] for a credential read, nil otherwise. A count that prints no
  content is `credential-count`, which a host may publish as a warning; a
  read is `reads-credential`.``
  (def hit (credential-read argv))
  (when hit
    (def [kind by path] hit)
    (if (= kind :deny)
      [PolicyReason-pathSensitiveDeny (string "reads-credential: " by " with " path)]
      [PolicyReason-pathSensitiveDeny (string "credential-count: " by " with " path)])))
