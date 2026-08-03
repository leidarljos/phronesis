# Pure secret-in-argv law (no Cap'n). Deny spawn if any argv token carries
# live credential material that must never leave the seat or hit traces.

(defn- has-sub [s needle]
  (not (nil? (string/find needle s))))

(defn- has-vendor-secret? [s]
  (or (has-sub s "glpat-")
      (has-sub s "ghp_")
      (has-sub s "gho_")
      (has-sub s "ghu_")
      (has-sub s "ghs_")
      (has-sub s "ghr_")
      (has-sub s "github_pat_")
      (has-sub s "sk-")
      (has-sub s "xoxb-")
      (has-sub s "xoxa-")
      (has-sub s "xoxp-")
      (has-sub s "xoxr-")
      (has-sub s "xoxs-")
      (has-sub s "xapp-")
      (has-sub s "AKIA")))

# Basic-auth URL: scheme://user:password@host (password may be a PAT).
(def basic-auth-url-peg
  (peg/compile
    ~{:main (* (any 1) "://" (some (if-not (set ":@") 1)) ":"
               (some (if-not (set "@") 1)) "@")}))

(defn secret-token? [tok]
  (def s (string tok))
  (if (= (length s) 0)
    (break false))
  (when (has-sub s "PRIVATE KEY-----")
    (break true))
  (when (peg/match basic-auth-url-peg s)
    (break true))
  (when (or (has-sub s "password=")
            (has-sub s "passwd=")
            (has-sub s "api_key=")
            (has-sub s "apikey=")
            (has-sub s "access_token=")
            (has-sub s "secret_key=")
            (has-sub s "client_secret="))
    (break true))
  (has-vendor-secret? s))

# Returns deny triple [code reason] or nil.
(defn shell-secret-deny [argv]
  (if (find secret-token? argv)
    [PolicyReason-shellSecretInArgv
     "secret material in argv denied (must not leave seat or enter traces)"]
    nil))

# Redact secret substrings in free text (tool results / traces). Returns new string.
(defn redact-secrets [s]
  (def out @"")
  (var i 0)
  (def n (length s))
  (while (< i n)
    (def rest (string/slice s i))
    (cond
      (string/has-prefix? rest "glpat-")
      (do (buffer/push-string out "glpat-***REDACTED***")
          (set i (+ i 6))
          (while (and (< i n)
                      (or (and (>= (in s i) 48) (<= (in s i) 57))
                          (and (>= (in s i) 65) (<= (in s i) 90))
                          (and (>= (in s i) 97) (<= (in s i) 122))
                          (= (in s i) 45) (= (in s i) 95)))
            (set i (+ i 1))))
      (string/has-prefix? rest "ghp_")
      (do (buffer/push-string out "ghp_***REDACTED***")
          (set i (+ i 4))
          (while (and (< i n) (not (= (in s i) 32)) (not (= (in s i) 10)))
            (set i (+ i 1))))
      (string/has-prefix? rest "-----BEGIN")
      (do (buffer/push-string out "-----BEGIN ***REDACTED KEY-----")
          (def end (string/find "-----END" rest))
          (if end
            (set i (+ i end 20))
            (set i n)))
      (do (buffer/push-byte out (in s i))
          (set i (+ i 1)))))
  (string out))
