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

# curl|sh / wget|bash: argv contains a downloader and a shell as separate tokens
# (agent-tokenized). Also catch `sh -c` with curl in the string (substring).
(defn remote-exec? [argv]
  (var has-fetch false)
  (var has-shell false)
  (var hit false)
  (each t argv
    (def b (argv-base t))
    (when (or (= b "curl") (= b "wget") (= b "fetch"))
      (set has-fetch true))
    (when (or (= b "sh") (= b "bash") (= b "zsh") (= b "dash"))
      (set has-shell true))
    # sh -c 'curl … | bash' style single token
    (when (or (string/find "curl " t)
              (string/find "wget " t))
      (when (or (string/find "| sh" t)
                (string/find "|sh" t)
                (string/find "| bash" t)
                (string/find "|bash" t))
        (set hit true))))
  (or hit (and has-fetch has-shell)))

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
            "remote-exec pattern denied (curl|sh / fetch+shell)"]))
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
