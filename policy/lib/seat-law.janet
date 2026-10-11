# Pure seat law (no Cap'n): the package rules a seat can opt into with
# policy/seat.janet. The default pack (policy/shell.janet) does not apply
# them, because on a person's own machine `npm test` and `pip install -e .`
# are ordinary work.

(defn dangerous-package-runner? [b]
  (truthy? (find (fn [x] (= x b))
                 ["poetry" "pipx" "conda" "mamba" "micromamba" "nix-shell" "nix"
                  "asdf" "npm" "yarn" "pnpm" "bun" "cargo-install"])))

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

# Returns a deny pair [code reason] or nil.
(defn seat-package-deny [argv]
  (when (pip-install? argv)
    (break [PolicyReason-shellDangerousRunner
            "pip install denied; use pixi / workspace-declared env"]))
  (when (find (fn [t] (dangerous-package-runner? (argv-base t))) argv)
    (break [PolicyReason-shellDangerousRunner
            "package-manager runner denied; use pixi run"]))
  nil)
