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

(defn- member? [xs x] (truthy? (find (fn [y] (= y x)) xs)))
(defn- pre? [p s] (string/has-prefix? p s))
(defn- has-sub? [needle s] (truthy? (string/find needle s)))

# A cluster of short flags such as -fu or -rf: one dash, then letters.
(defn- short-flags? [a]
  (and (pre? "-" a) (not (pre? "--" a)) (> (length a) 1)))

(defn- short-has? [a c]
  (and (short-flags? a) (has-sub? c (string/slice a 1))))

(defn privilege-runner? [b]
  (member? ["sudo" "su" "doas" "pkexec" "run0" "machinectl"] b))

# The command a pipeline stage runs: its first word past wrappers,
# assignments and flags, by base name.
(defn- command-word [stage]
  (var word nil)
  (each t stage
    (def b (argv-base t))
    (when (and (nil? word)
               (not (member? wrappers b))
               (not (pre? "-" t))
               (not (and (string/find "=" t) (not (pre? "=" t)))))
      (set word b)))
  word)

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

(defn- runs-download? [t]
  (truthy?
    (find (fn [open] (find (fn [f] (string/find (string open f) t)) fetchers))
          substitution-opens)))

# A download handed to a shell: a fetching stage piped into a shell stage,
# or a shell that runs a download through $( ), <( ), backticks, or -c with
# a script that does either. Naming a download tool and a shell on one
# line is not that: `git fetch`, then `bash build.sh`, runs nothing fetched.
(defn remote-exec? [argv]
  (def st (stages argv))
  (var hit false)
  (for k 0 (- (length st) 1)
    (when (and (member? fetchers (command-word (in st k)))
               (member? shells (command-word (in st (+ k 1)))))
      (set hit true)))
  (each s st
    (when (member? shells (command-word s))
      (when (find runs-download? s) (set hit true))
      (for k 0 (- (length s) 1)
        (when (and (= (in s k) "-c")
                   (remote-exec? (filter (fn [w] (> (length w) 0))
                                         (string/split " " (in s (+ k 1))))))
          (set hit true)))))
  hit)

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

# A recursive rm or rtrash with an operand that is neither under /tmp or
# /var/tmp nor a build output directory named as a plain relative path.
# When the line can cd out of the tree, only /tmp operands pass.
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
                                   (or moved (not (build-output? a)))))
                      (slice cmd 1)))))

(defn recursive-delete-off-tmp? [argv]
  (def moved (leaves-tree? argv))
  (truthy? (find (fn [cmd] (recursive-delete? cmd moved)) (commands argv))))

# find ... -delete, or -exec rm (also -execdir, -ok, shred, unlink), whose
# starting points are not all under /tmp. No starting point is `.`.
(defn- find-delete? [cmd]
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
                                (slice cmd 1))]
         (or (empty? starts) (not (all under-tmp? starts))))))

# chmod setting the setuid bit: +s, or a mode of three or more digits
# starting with 4.
(defn setuid-chmod? [argv]
  (and (> (length argv) 0)
       (= (argv-base (in argv 0)) "chmod")
       (truthy?
         (find (fn [a] (or (string/find "+s" a)
                           (and (pre? "4" a) (>= (length a) 3)
                                (peg/match ~(* (some (range "09")) -1) a))))
               argv))))

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

# A git push that can overwrite or drop history on the remote: --force, a
# short cluster holding f (-fu), --mirror, or a refspec starting with +.
# --force-with-lease (any form) and --force-if-includes are not that: the
# remote refuses them when it holds commits the pusher has not seen.
(defn- force-push? [cmd]
  (def at (find-index (fn [a] (= a "push")) cmd))
  (and at
       (truthy? (find (fn [a] (or (= a "--force") (= a "--mirror")
                                  (short-has? a "f")
                                  (and (pre? "+" a) (> (length a) 1))))
                      (slice cmd (+ at 1))))))

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
    (force-push? cmd) "git-force-push: force push denied (--force-with-lease is allowed)"
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
            "curl-pipe-shell: a download handed to a shell denied"]))
  (when (decoded-exec? argv)
    (break [PolicyReason-shellRemoteExec
            "decoded-pipe-shell: a decoded script handed to a shell denied"]))
  (when (find setuid-chmod? (commands argv))
    (break [PolicyReason-shellDangerousRunner "chmod-setuid: chmod setuid denied"]))
  (when (raw-disk? argv)
    (break [PolicyReason-shellDangerousRunner
            "raw-disk: writing a block device denied (mkfs, dd, wipefs, a redirect)"]))
  (when (recursive-mode-change? argv)
    (break [PolicyReason-shellDangerousRunner
            "recursive-chmod-chown: a recursive mode or owner change of a system or home tree denied"]))
  (when (recursive-delete-off-tmp? argv)
    (break [PolicyReason-shellDangerousRunner
            "rm-rf-outside-tmp: recursive delete outside /tmp and the build output directories denied"]))
  (when (find find-delete? (commands argv))
    (break [PolicyReason-shellDangerousRunner
            "find-delete-outside-tmp: find -delete outside /tmp denied"]))
  (def g (git-dangerous? argv))
  (when g
    (break [PolicyReason-shellGitDangerous g]))
  nil)
