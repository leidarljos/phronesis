# Pure secret-in-argv law (no Cap'n). Deny spawn if any argv token carries
# live credential material that must never leave the seat or hit traces.

(defn- credential-byte? [c]
  (or (and (>= c 48) (<= c 57))
      (and (>= c 65) (<= c 90))
      (and (>= c 97) (<= c 122))
      (= c 95)))

(defn- at-token-start? [s i]
  (or (= i 0)
      (not (credential-byte? (in s (- i 1))))))

# A credential prefix starts a token and has a payload. The same letters
# inside a word, such as a task directory or flexapp-x, are not a credential.
(defn- has-credential-prefix? [s prefix]
  (var found false)
  (each i (string/find-all prefix s)
    (def end (+ i (length prefix)))
    (when (and (at-token-start? s i)
               (< end (length s))
               (credential-byte? (in s end)))
      (set found true)))
  found)

# PEM armor is itself the credential. It must start a token. A copy glued
# onto a word is not armor.
(defn- has-bounded-marker? [s marker]
  (var found false)
  (each i (string/find-all marker s)
    (when (at-token-start? s i)
      (set found true)))
  found)

(defn- has-listed-prefix? [s prefixes]
  (truthy? (find (fn [p] (has-credential-prefix? s p)) prefixes)))

(def- vendor-prefixes
  ["glpat-" "ghp_" "gho_" "ghu_" "ghs_" "ghr_" "github_pat_" "sk-"
   "xoxb-" "xoxa-" "xoxp-" "xoxr-" "xoxs-" "xapp-" "AKIA"])

# Assignment keys match in any ASCII case. PASSWORD= and --password= are
# the same key. The key must start a token and the value must begin with
# a credential byte.
(def- assignment-prefixes
  ["password=" "passwd=" "api_key=" "apikey=" "access_token="
   "secret_key=" "client_secret="])

(defn- has-vendor-secret? [s]
  (has-listed-prefix? s vendor-prefixes))

(defn- has-assignment-secret? [s]
  (has-listed-prefix? (string/ascii-lower s) assignment-prefixes))

# Basic-auth URL: scheme://user:password@host (password may be a PAT).
(def basic-auth-url-peg
  (peg/compile
    ~{:main (* (any 1) "://" (some (if-not (set ":@") 1)) ":"
               (some (if-not (set "@") 1)) "@")}))

(defn secret-token? [tok]
  (def s (string tok))
  (if (= (length s) 0)
    (break false))
  (when (has-bounded-marker? s "PRIVATE KEY-----")
    (break true))
  (when (peg/match basic-auth-url-peg s)
    (break true))
  (when (has-assignment-secret? s)
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
    (def boundary (at-token-start? s i))
    (cond
      (and boundary (string/has-prefix? "glpat-" rest))
      (do (buffer/push-string out "glpat-***REDACTED***")
          (set i (+ i 6))
          (while (and (< i n)
                      (or (and (>= (in s i) 48) (<= (in s i) 57))
                          (and (>= (in s i) 65) (<= (in s i) 90))
                          (and (>= (in s i) 97) (<= (in s i) 122))
                          (= (in s i) 45) (= (in s i) 95)))
            (set i (+ i 1))))
      (and boundary (string/has-prefix? "ghp_" rest))
      (do (buffer/push-string out "ghp_***REDACTED***")
          (set i (+ i 4))
          (while (and (< i n) (not (= (in s i) 32)) (not (= (in s i) 10)))
            (set i (+ i 1))))
      (and boundary (string/has-prefix? "-----BEGIN" rest))
      (do (buffer/push-string out "-----BEGIN ***REDACTED KEY-----")
          (def end (string/find "-----END" rest))
          (if end
            (set i (+ i end 20))
            (set i n)))
      (do (buffer/push-byte out (in s i))
          (set i (+ i 1)))))
  (string out))
