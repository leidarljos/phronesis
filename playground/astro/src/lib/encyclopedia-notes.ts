/**
 * Hand-authored encyclopedia notes for high-traffic PolicyReason codes.
 * Schema names/ordinals come from policy-reasons.generated.ts (codegen).
 * This file is not overwritten by gen-encyclopedia.py.
 */

export interface EncyclopediaSource {
  /** Repo-relative path */
  path: string;
  /** Symbol or function name when applicable */
  symbol?: string;
  note?: string;
}

export interface EncyclopediaNote {
  /** One-line product summary */
  summary: string;
  /** Paragraphs (plain text; rendered as <p>) */
  body: string[];
  sources: EncyclopediaSource[];
  /** Fixture catalog ids that exercise this code */
  relatedFixtures?: string[];
}

/** Keyed by PolicyReason ordinal. */
export const ENCYCLOPEDIA_NOTES: Record<number, EncyclopediaNote> = {
  // pathOutsideWorkspace @2
  2: {
    summary:
      "Deny when the shell cwd (or path-plane path) is outside the seat workspace.",
    body: [
      "Shell content pack: Cap'n ShellView carries underWorkspace. When that bit is false, policy/shell.janet short-circuits with Decision.deny and code pathOutsideWorkspace before danger/secret/python law runs.",
      "Path-plane TCB (checkPath / phronesis_policy_eval): read/write outside the workspace also fail closed on this plane; under-workspace allow is pathUnderWorkspaceAllow (3).",
      "Typical playground fixture: cwd_outside_deny (cwd not under /ws).",
    ],
    sources: [
      {
        path: "policy/shell.janet",
        symbol: "shell-check",
        note: "unless under-workspace → pathOutsideWorkspace",
      },
      {
        path: "src/policy.c",
        symbol: "path_under_workspace",
        note: "lexical workspace prefix check on path plane",
      },
    ],
    relatedFixtures: ["cwd_outside_deny"],
  },

  // packLoadFailed @13
  13: {
    summary: "Deny when a Janet shell pack path is valid but load/compile fails.",
    body: [
      "Host lifecycle for reloadShellPack: empty, relative, overlong, or non-file paths use packPathInvalid (22). A path that exists as a regular file but fails Janet load or compile maps to packLoadFailed.",
      "Successful hot-load returns packReloaded (21). Missing default pack at check time is packMissing (12); runtime errors inside shell-check are packRuntimeError (14).",
      "TCB-only code: packs do not emit packLoadFailed themselves.",
    ],
    sources: [
      {
        path: "src/capnp_api.c",
        symbol: "reloadShellPack",
        note: "rc from phronesis_shell_pack_reload_internal → packLoadFailed",
      },
      {
        path: "src/policy_janet.c",
        note: "pack load / sealed env setup",
      },
    ],
  },

  // pythonRequiresUvRun @17
  17: {
    summary:
      "Deny bare python / .py argv that does not go through uv run (+ PEP 723 when a script path is present).",
    body: [
      "Product shell law: if argv touches a python interpreter or a .py path, the pack requires uv … run. Bare python3 script.py, poetry, and other runners that skip uv fail with this code (or shellDangerousRunner for banned package managers).",
      "Helpers live in policy/lib/python-law.janet (touches-python?, uv-run?). shell.janet applies the deny after danger and secret gates.",
      "Related: pythonDashCDenied (18) for python -c; pythonMissingPep723 (19) when a .py path lacks a PEP 723 script block in PathProbe head.",
    ],
    sources: [
      {
        path: "policy/shell.janet",
        symbol: "shell-check",
        note: "unless uv-run? → pythonRequiresUvRun",
      },
      {
        path: "policy/lib/python-law.janet",
        symbol: "uv-run?",
        note: "detects uv … run with intervening global flags",
      },
    ],
    relatedFixtures: ["bare_python_deny"],
  },

  // shellRemoteExec @24
  24: {
    summary:
      "Deny remote-exec patterns: curl|sh / wget|bash class argv (downloader + shell).",
    body: [
      "policy/lib/shell-danger.janet remote-exec? is the product detector. It flags agent-tokenized argv that contains both a fetch binary (curl, wget, fetch) and a shell (sh, bash, zsh, dash), and also sh -c strings that embed curl/wget piped to a shell.",
      "shell-danger-deny maps a hit to PolicyReason-shellRemoteExec. shell.janet runs this gate before secrets and python law.",
      "Playground fixture curl_sh_deny uses argv [curl, URL, sh] under workspace and expects deny / 24.",
    ],
    sources: [
      {
        path: "policy/lib/shell-danger.janet",
        symbol: "remote-exec?",
        note: "curl|sh / wget|bash class patterns in argv",
      },
      {
        path: "policy/lib/shell-danger.janet",
        symbol: "shell-danger-deny",
        note: "returns [PolicyReason-shellRemoteExec reason]",
      },
      {
        path: "policy/shell.janet",
        symbol: "shell-check",
        note: "invokes shell-danger-deny before secret/python gates",
      },
    ],
    relatedFixtures: ["curl_sh_deny"],
  },

  // shellSecretInArgv @27
  27: {
    summary: "Deny spawn when any argv token carries live credential material.",
    body: [
      "Secrets must not appear in process argv (they leak into TRACE, audit logs, and process listings). policy/lib/shell-secret.janet secret-token? matches vendor PATs (glpat-, ghp_, github_pat_, sk-, Slack xox*, AWS AKIA), PEM private key armor, basic-auth URLs, and password=/api_key= style key-value tokens.",
      "shell-secret-deny returns PolicyReason-shellSecretInArgv. shell.janet applies this after shell-danger and before python law.",
    ],
    sources: [
      {
        path: "policy/lib/shell-secret.janet",
        symbol: "secret-token?",
        note: "vendor PAT / PEM / basic-auth / key=value detectors",
      },
      {
        path: "policy/lib/shell-secret.janet",
        symbol: "shell-secret-deny",
        note: "returns [PolicyReason-shellSecretInArgv reason]",
      },
    ],
    relatedFixtures: ["glpat_secret_deny"],
  },

  // pathSensitiveDeny @28
  28: {
    summary:
      "Deny Read/write on credential-class paths so secrets never leave the seat into model traces.",
    body: [
      "Path plane (src/policy.c path_is_sensitive): lexical match on basenames (.env, .netrc, id_rsa, credentials.json, secrets.yaml, …), path components (.ssh, .gnupg, .aws, .kube, .vault, private_dot_*), and suffixes (.pem, .key, .p12, .pfx). Applies to read and write actions before under-workspace allow.",
      "Distinct from shellSecretInArgv (argv tokens) and secretExportDenied (RiskAction.secretExport fail-closed).",
    ],
    sources: [
      {
        path: "src/policy.c",
        symbol: "path_is_sensitive",
        note: "basename / component / suffix credential-class match",
      },
      {
        path: "src/policy.c",
        symbol: "phronesis_policy_eval",
        note: "read|write + sensitive → PATH_SENSITIVE_DENY",
      },
    ],
    relatedFixtures: ["sensitive_deny"],
  },
};
