/**
 * Client-side secret-looking argv detector (share-URL gate).
 * Mirrors policy/lib/shell-secret.janet vendor prefixes + basic patterns.
 * Does not claim parity with the pack; only warns before putting tokens in a URL hash.
 */

const VENDOR_PREFIXES = [
  "glpat-",
  "ghp_",
  "gho_",
  "ghu_",
  "ghs_",
  "ghr_",
  "github_pat_",
  "sk-",
  "xoxb-",
  "xoxa-",
  "xoxp-",
  "xoxr-",
  "xoxs-",
  "xapp-",
  "AKIA",
];

const KEY_VALUE_HINTS = [
  "password=",
  "passwd=",
  "api_key=",
  "apikey=",
  "access_token=",
  "secret_key=",
  "client_secret=",
];

/** Basic-auth URL: scheme://user:password@host */
const BASIC_AUTH = /:\/\/[^:@\s]+:[^@\s]+@/;

export function tokenLooksSecret(tok: string): boolean {
  if (!tok) return false;
  if (tok.includes("PRIVATE KEY-----")) return true;
  if (BASIC_AUTH.test(tok)) return true;
  const lower = tok.toLowerCase();
  for (const h of KEY_VALUE_HINTS) {
    if (lower.includes(h)) return true;
  }
  for (const p of VENDOR_PREFIXES) {
    if (tok.includes(p)) return true;
  }
  return false;
}

export function findSecretTokens(argv: string[]): string[] {
  return argv.filter(tokenLooksSecret);
}

export interface SecretWarnResult {
  risky: boolean;
  tokens: string[];
  message: string | null;
}

export function warnSecretsInArgv(argv: string[]): SecretWarnResult {
  const tokens = findSecretTokens(argv);
  if (tokens.length === 0) {
    return { risky: false, tokens: [], message: null };
  }
  return {
    risky: true,
    tokens,
    message:
      "Argv contains secret-looking material. Sharing will put tokens in the URL hash " +
      "(browser history, screenshots, referrers). Confirm only if intentional.",
  };
}

export function redactTokenPreview(tok: string, max = 12): string {
  if (tok.length <= max) return `${tok.slice(0, 4)}…`;
  return `${tok.slice(0, max)}…`;
}
