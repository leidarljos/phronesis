# Pure audio / voice gate law (no Cap'n, no FS).
# Unit-tested via tests/test_pack_lib.c. Host loads sorted lib/ before shell.janet
# (name sorts after layout.janet so Decision/AudioAction/PolicyReason bind).
# Product defaults for Policyd.checkAudio (meta #97 Track E).
# Host TCB still applies GROKOS_POLICYD_DENY_ALL / GROKOS_POLICYD_AUDIO_ALLOW.

(defn audio-decide
  ``Map AudioAction ordinal → [decision code].
  Defaults: micOpen/alwaysListen/networkStt/inject deny; listenArm prompt;
  unknown → deny. Fixture allow is host env only (not here).``
  [action]
  (cond
    (= action AudioAction-micOpen)
    [Decision-deny PolicyReason-audioMicOpenDeny]
    (= action AudioAction-listenArm)
    [Decision-prompt PolicyReason-audioListenArmPrompt]
    (= action AudioAction-alwaysListen)
    [Decision-deny PolicyReason-audioAlwaysListenDeny]
    (= action AudioAction-networkStt)
    [Decision-deny PolicyReason-audioNetworkSttDeny]
    (= action AudioAction-inject)
    [Decision-deny PolicyReason-audioInjectDeny]
    [Decision-deny PolicyReason-audioUnknownAction]))
