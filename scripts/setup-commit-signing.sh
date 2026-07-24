#!/usr/bin/env bash
# One-shot: enable SSH commit signing for GrokOS work (Ali RFC / project policy).
set -euo pipefail

KEY="${1:-}"
if [[ -z "${KEY}" ]]; then
  for cand in "$HOME/.ssh/id_ed25519" "$HOME/.ssh/id_ecdsa" "$HOME/.ssh/id_rsa"; do
    if [[ -f "${cand}" && -f "${cand}.pub" ]]; then
      KEY="${cand}"
      break
    fi
  done
fi
if [[ -z "${KEY}" || ! -f "${KEY}.pub" ]]; then
  echo "Usage: $0 [/path/to/ssh_key_without_extension]" >&2
  echo "No default SSH key found. Generate one: ssh-keygen -t ed25519 -C you@teachx.ai" >&2
  exit 1
fi

git config --global gpg.format ssh
git config --global user.signingkey "${KEY}.pub"
git config --global commit.gpgsign true
git config --global tag.gpgsign true

mkdir -p "${HOME}/.config/git"
EMAIL=$(git config --global user.email || true)
if [[ -z "${EMAIL}" ]]; then
  echo "Set git user.email first: git config --global user.email you@teachx.ai" >&2
  exit 1
fi
PUB=$(cat "${KEY}.pub")
ALLOW="${HOME}/.config/git/allowed_signers"
touch "${ALLOW}"
if ! grep -qF "${PUB}" "${ALLOW}" 2>/dev/null; then
  echo "${EMAIL} namespaces=\"git\" ${PUB}" >> "${ALLOW}"
fi
git config --global gpg.ssh.allowedSignersFile "${ALLOW}"

echo "Configured SSH commit signing with ${KEY}.pub"
echo "Ensure this public key is on GitLab (Preferences → SSH Keys) with"
echo "usage Authentication & Signing (or Signing only)."
echo
echo "Smoke test:"
echo "  git commit --allow-empty -m 'test sign' && git log -1 --show-signature && git reset --hard HEAD~1"
