# Pure whole-tree law (no Cap'n, no FS): `mv` onto /dev/null or another
# whole tree outside the work tree, and `rsync --delete` over such a tree.
# Loaded with the other policy/lib/*.janet before the pack entry.
#
# The host hands an absolute path inside the work tree as a relative one,
# so an absolute path here, `~`, `$HOME`, /dev/null and a remote `host:path`
# are outside it. A relative destination stays in the work tree.

(defn- tr-pre? [p s] (string/has-prefix? p s))
(defn- tr-member? [xs x] (truthy? (find (fn [y] (= y x)) xs)))
(defn- tr-base [tok]
  (def parts (string/split "/" tok))
  (if (empty? parts) tok (last parts)))

(defn- tr-commands [argv]
  (def out @[@[]])
  (each t argv
    (if (tr-member? [";" "&&" "||" "|" "&"] t)
      (array/push out @[])
      (let [closes (string/has-suffix? ";" t)
            tok (if closes (string/slice t 0 -2) t)]
        (when (> (length tok) 0) (array/push (last out) tok))
        (when closes (array/push out @[])))))
  (filter (fn [c] (> (length c) 0)) out))

(defn- mv-dest [cmd]
  (var dest nil)
  (var done nil)
  (var i 1)
  (while (and (nil? done) (< i (length cmd)))
    (def a (in cmd i))
    (cond
      (or (= a "-t") (= a "--target-directory")) (set done (get cmd (+ i 1) ""))
      (tr-pre? "--target-directory=" a) (set done (string/slice a 19))
      (and (tr-pre? "-" a) (not= a "-")) nil
      (set dest a))
    (++ i))
  (or done dest))

(def- rsync-delete-flags
  ["--delete" "--del" "--delete-before" "--delete-during" "--delete-delay"
   "--delete-after" "--delete-excluded"])

(defn- rsync-deletes? [cmd] (truthy? (find |(tr-member? rsync-delete-flags $) cmd)))

(defn- rsync-dry? [cmd]
  (truthy? (find |(or (= $ "--dry-run") (= $ "-n")
                      (and (tr-pre? "-" $) (not (tr-pre? "--" $)) (string/find "n" $)))
                 cmd)))

(def- rsync-valued
  ["-e" "--rsh" "--exclude" "--include" "--filter" "-f" "--exclude-from"
   "--include-from" "--files-from" "--chmod" "--chown" "--max-size" "--min-size"
   "--backup-dir" "--suffix" "--partial-dir" "--compare-dest" "--copy-dest"
   "--link-dest" "--log-file" "--out-format" "--temp-dir" "-T" "--password-file"
   "--bwlimit" "--info" "--debug" "--rsync-path" "--address" "--port"
   "--timeout" "--contimeout"])

(defn- rsync-dest [cmd]
  (def pos @[])
  (var i 1)
  (var done false)
  (while (and (not done) (< i (length cmd)))
    (def a (in cmd i))
    (cond
      (= a "--") (do (array/concat pos (slice cmd (+ i 1))) (set done true))
      (and (tr-pre? "-" a) (not= a "-"))
      (+= i (if (and (tr-member? rsync-valued a) (not (string/find "=" a))) 2 1))
      (do (array/push pos a) (++ i))))
  (last pos))

# The destination with quotes off and `.` and `..` folded, keeping a leading
# `~` or `/`.
(defn- tidy-dest [dest]
  (def d (string/trim dest "\"'"))
  (def tilde (tr-pre? "~" d))
  (def abs (tr-pre? "/" d))
  (def body (if tilde (string/triml (string/triml d "~") "/") (string/triml d "/")))
  (def parts @[])
  (each p (string/split "/" body)
    (cond
      (or (= p "") (= p ".")) nil
      (= p "..") (array/pop parts)
      (array/push parts p)))
  (cond
    tilde (if (empty? parts) "~" (string "~/" (string/join parts "/")))
    abs (if (empty? parts) "/" (string "/" (string/join parts "/")))
    (string/join parts "/")))

(defn- whole-outside? [dest]
  (or (tr-pre? "~" dest) (tr-pre? "$HOME" dest) (tr-pre? "${HOME}" dest)
      (= dest "/dev/null") (truthy? (string/find ":" dest))
      (tr-pre? "/" dest)))

(defn- is-tree? [dest]
  (if (tr-member? ["/" "~" "$HOME" "${HOME}" "/dev/null" "/home" "/root"] dest)
    true
    (let [cut (last (string/split ":" dest))
          name (last (string/split "/" cut))]
      (and (> (length name) 0) (not (string/find "." name))))))

(defn- outside-tree? [dest]
  (def d (tidy-dest dest))
  (and (> (length d) 0) (whole-outside? d) (is-tree? d)))

(defn shell-tree-deny [argv]
  ``[code reason] for a whole tree moved or mirrored away outside the work
  tree, nil otherwise. `rsync-delete-outside` deletes what the source does
  not hold; `mv-tree-outside` moves the tree away, which a host may publish
  as a warning.``
  (var hit nil)
  (each cmd (tr-commands argv)
    (unless hit
      (case (tr-base (first cmd))
        "mv" (let [d (mv-dest cmd)]
               (when (and d (outside-tree? d))
                 (set hit [PolicyReason-shellDangerousRunner
                           (string "mv-tree-outside: a whole tree moved onto " d)])))
        "rsync" (when (and (rsync-deletes? cmd) (not (rsync-dry? cmd)))
                  (let [d (rsync-dest cmd)]
                    (when (and d (outside-tree? d))
                      (set hit [PolicyReason-shellDangerousRunner
                                (string "rsync-delete-outside: rsync --delete over " d)])))))))
  hit)
