# Pure shell-danger law (no Cap'n). Privilege, remote-exec, package managers, git.
# Loaded with other policy/lib/*.janet before shell.janet.

(defn privilege-runner? [b]
  (or (= b "sudo") (= b "su") (= b "doas") (= b "pkexec")
      (= b "run0") (= b "machinectl")))

(defn dangerous-package-runner? [b]
  (or (= b "poetry")
      (= b "pipx")
      (= b "conda")
      (= b "mamba")
      (= b "micromamba")
      (= b "nix-shell")
      (= b "nix")
      (= b "asdf")
      (= b "npm")
      (= b "yarn")
      (= b "pnpm")
      (= b "bun")
      (= b "cargo-install") # rare basename
      ))

# pip / python -m pip install (global install class)
(defn pip-install? [argv]
  (var saw-pip false)
  (var after-python-m false)
  (var prev-py false)
  (var hit false)
  (each t argv
    (def b (argv-base t))
    (when (or (= b "pip") (= b "pip3"))
      (set saw-pip true))
    (when (and prev-py (= t "-m"))
      (set after-python-m true))
    (when (and after-python-m (= t "pip"))
      (set saw-pip true)
      (set after-python-m false))
    (when (and saw-pip (= t "install"))
      (set hit true))
    (set prev-py (python-interp? b)))
  hit)

(def- fetchers ["curl" "wget" "fetch"])
(def- shells ["sh" "bash" "zsh" "dash" "ksh" "fish"])
(def- wrappers ["sudo" "doas" "env" "nohup" "time" "exec" "command" "nice"])

(defn- member? [xs x] (truthy? (find (fn [y] (= y x)) xs)))

# The command a pipeline stage runs: its first word past wrappers,
# assignments and flags, by base name.
(defn- command-word [stage]
  (var word nil)
  (each t stage
    (def b (argv-base t))
    (when (and (nil? word)
               (not (member? wrappers b))
               (not (string/has-prefix? "-" t))
               (not (and (string/find "=" t) (not (string/has-prefix? "=" t)))))
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
          ["$(" "<(" "`"])))

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
  (or (string/has-prefix? ">" rest) (string/has-prefix? "<" rest)))

(defn- under-tmp? [p]
  (or (= p "/tmp") (string/has-prefix? "/tmp/" p)
      (= p "/var/tmp") (string/has-prefix? "/var/tmp/" p)))

# A recursive rm or rtrash whose own operands reach outside /tmp and
# /var/tmp. Flags and redirections are not operands; every command on
# the line is judged by its own words.
(defn recursive-delete-off-tmp? [argv]
  (truthy?
    (find
      (fn [cmd]
        (def b (argv-base (in cmd 0)))
        (and (or (= b "rm") (= b "rtrash"))
             (or (find (fn [a] (and (string/has-prefix? "-" a)
                                    (not (string/has-prefix? "--" a))
                                    (string/find "r" a) (string/find "f" a)))
                       (slice cmd 1))
                 (and (find (fn [a] (member? ["-r" "-R" "--recursive"] a)) cmd)
                      (find (fn [a] (member? ["-f" "--force"] a)) cmd)))
             (find (fn [a] (and (not (string/has-prefix? "-" a))
                                (not (redirection? a))
                                (not (under-tmp? a))))
                   (slice cmd 1))))
      (commands argv))))

# chmod setting the setuid bit: +s, or a four-digit mode starting with 4.
(defn setuid-chmod? [argv]
  (and (> (length argv) 0)
       (= (argv-base (in argv 0)) "chmod")
       (truthy?
         (find (fn [a] (or (string/find "+s" a)
                           (and (string/has-prefix? "4" a) (>= (length a) 3)
                                (peg/match ~(* (some (range "09")) -1) a))))
               argv))))

# Writing a disk directly: mkfs*, or dd with of=/dev/...
(defn raw-disk? [argv]
  (and (> (length argv) 0)
       (let [b (argv-base (in argv 0))]
         (or (string/has-prefix? "mkfs" b)
             (and (= b "dd")
                  (truthy? (find (fn [a] (string/has-prefix? "of=/dev/" a)) argv)))))))

# git push --force / -f, reset --hard, clean -fdx
(defn git-dangerous? [argv]
  (unless (find (fn [t] (= (argv-base t) "git")) argv)
    (break false))
  (var hit false)
  (var i 0)
  (def n (length argv))
  (while (< i n)
    (def t (in argv i))
    (when (= t "push")
      (var j (+ i 1))
      (while (< j n)
        (def u (in argv j))
        (when (or (= u "--force") (= u "-f") (= u "--force-with-lease"))
          (set hit true))
        (set j (+ j 1))))
    (when (= t "reset")
      (var j (+ i 1))
      (while (< j n)
        (when (= (in argv j) "--hard")
          (set hit true))
        (set j (+ j 1))))
    (when (= t "clean")
      (var j (+ i 1))
      (while (< j n)
        (def u (in argv j))
        (when (or (= u "-fdx") (= u "-ffdx")
                  (and (string/has-prefix? u "-")
                       (string/find "x" u)
                       (string/find "f" u)
                       (string/find "d" u)))
          (set hit true))
        (set j (+ j 1))))
    (set i (+ i 1)))
  hit)

# Returns a deny triple [code reason] or nil if this module has no opinion.
(defn shell-danger-deny [argv]
  (when (find (fn [t] (privilege-runner? (argv-base t))) argv)
    (break [PolicyReason-shellPrivilegeDenied
            "privilege runner denied (sudo/su/doas/pkexec)"]))
  (when (remote-exec? argv)
    (break [PolicyReason-shellRemoteExec
            "remote-exec denied: a download handed to a shell"]))
  (when (setuid-chmod? argv)
    (break [PolicyReason-shellDangerousRunner "chmod setuid denied"]))
  (when (raw-disk? argv)
    (break [PolicyReason-shellDangerousRunner "raw disk write denied (mkfs, dd of=/dev/)"]))
  (when (recursive-delete-off-tmp? argv)
    (break [PolicyReason-shellDangerousRunner "recursive delete outside /tmp denied"]))
  (when (pip-install? argv)
    (break [PolicyReason-shellDangerousRunner
            "pip install denied; use pixi / workspace-declared env"]))
  (when (find (fn [t] (dangerous-package-runner? (argv-base t))) argv)
    (break [PolicyReason-shellDangerousRunner
            "package-manager runner denied; use pixi run"]))
  (when (git-dangerous? argv)
    (break [PolicyReason-shellGitDangerous
            "dangerous git mutation denied (force-push / reset --hard / clean -fdx)"]))
  nil)
