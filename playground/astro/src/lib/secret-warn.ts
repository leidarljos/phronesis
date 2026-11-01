/**
 * Client-side secret-looking argv detector (share-URL gate).
 * Mirrors policy/lib/shell-secret.janet: a prefix must start a token and
 * carry a payload. The same letters inside a word are not a credential.
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

function credentialByte(code: number): boolean {
  return (
    (code >= 48 && code <= 57) ||
    (code >= 65 && code <= 90) ||
    (code >= 97 && code <= 122) ||
    code === 95
  );
}

function atTokenStart(s: string, i: number): boolean {
  return i === 0 || !credentialByte(s.charCodeAt(i - 1));
}

/** Prefix starts a token and the next byte is a payload character. */
function hasCredentialPrefix(s: string, prefix: string): boolean {
  let from = 0;
  while (from < s.length) {
    const i = s.indexOf(prefix, from);
    if (i < 0) return false;
    const end = i + prefix.length;
    if (atTokenStart(s, i) && end < s.length && credentialByte(s.charCodeAt(end))) {
      return true;
    }
    from = i + 1;
  }
  return false;
}

function hasBoundedMarker(s: string, marker: string): boolean {
  let from = 0;
  while (from < s.length) {
    const i = s.indexOf(marker, from);
    if (i < 0) return false;
    if (atTokenStart(s, i)) return true;
    from = i + 1;
  }
  return false;
}

export function tokenLooksSecret(tok: string): boolean {
  if (!tok) return false;
  if (hasBoundedMarker(tok, "PRIVATE KEY-----")) return true;
  if (BASIC_AUTH.test(tok)) return true;
  const lower = tok.toLowerCase();
  for (const h of KEY_VALUE_HINTS) {
    if (hasCredentialPrefix(lower, h)) return true;
  }
  for (const p of VENDOR_PREFIXES) {
    if (hasCredentialPrefix(tok, p)) return true;
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
