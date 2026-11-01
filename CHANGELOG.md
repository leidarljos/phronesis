# Changelog

<!-- towncrier release notes start -->

## 0.2.9 (2026-10-31)

### Fixes

- The crate's vendored header is this release's, so the library phronesis-sys links reports the crate version; the version check refuses a bump that skipped the vendor refresh. 0.2.8 shipped the crate with the 0.2.7 header and failed its own version test.


## 0.2.8 (2026-10-31)

### Features

- `crates/phronesis-sys`: phronesis as a Rust crate built by cargo alone. The library sources, the Janet amalgamation, the Cap'n Proto C runtime, the capnp-janet runtime and the C generated from the schema are vendored under the crate; the pack is embedded and written to the cache in the trusted layout at run time; a `Supervisor` opens a seat and judges a line. `scripts/vendor-refresh.sh` refreshes the vendored trees from the pinned revisions.


## 0.2.7 (2026-10-31)

### Miscellaneous

- The 0.2.6 tag did not build: its header kept the 0.2.5 patch number, which the version check refuses. 0.2.7 is that release.


## 0.2.6 (2026-10-31)

### Fixes

- `git restore --staged` over the whole tree only unstages; the pack no longer refuses it as a discard of the working tree.


## 0.2.5 (2026-10-20)

### Fixes

- The shell check distinguishes short credential prefixes from suffixes inside ordinary words and paths. Credential values in headers, quoted strings and path components remain refused, while a prefix with no payload is not a credential.
- Vendor prefixes and assignment keys use the same token boundary as the short credential prefix. A prefix inside a word passes, including flexapp-x, aKIAsomething and mypassword= in a path. A token at a boundary with a payload is still refused: glpat-, ghp_, gho_, ghu_, ghs_, ghr_, github_pat_, sk-proj-, sk-ant-, Slack xoxb-, xoxa-, xoxp-, xoxr-, xoxs- and xapp- tokens, an AKIA key of 16 uppercase characters, and password=, passwd=, api_key=, apikey=, access_token=, secret_key= and client_secret= in any letter case, including a --password= flag and a PASSWORD= environment assignment. PEM armor must start a token. Redaction of glpat- and ghp_ uses that boundary too.


## 0.2.4 (2026-10-20)

### Fixes

- The default shell law let seven destructive lines through. A download piped into `python3`, `perl`, `ruby` or `node`, or into a shell through a later stage such as `tee`, is now `curl-pipe-shell`, as are `source <(curl ...)`, `. <(curl ...)`, `eval $(curl ...)` and a file that `curl -o` or `wget` saves and a later command on the line runs. `chmod` with a leading-zero or setgid mode (`04755`, `2755`, `g+s`) and `install -m 4755` are now `chmod-setuid`. A `git push --force-with-lease` onto `main`, `master` or `develop`, or one that names no branch (no refspec, `HEAD`, a wildcard, `--all`), is now `git-force-push`; a lease push onto any other branch still passes. `curl URL | python3 -m json.tool`, `curl -o FILE URL`, `chmod 0755` and `git push -u origin BRANCH` stay allowed. The rules match the ljos-policyd table again.


## 0.2.3 (2026-10-18)

### Fixes

- A shell command whose Cap'n body is larger than 512 KiB, such as about 30000 one-byte argv words, was refused as invalidMessage with an empty reason, before the argv count was checked. checkShell now denies that body as fieldTooLong, and the decision's reason says the body exceeds 524288 bytes.
- Shell argv words were copied into a 512-byte path slot, so a word of 512 bytes or more was refused as fieldTooLong before the shell policy could read it. Words are stored at their real length, up to 128 KiB (Linux MAX_ARG_STRLEN). The Cap'n body limit is 512 KiB so a word of that size fits. A longer word is still fieldTooLong, and that decision now carries a reason. Paths stay bounded by PHRONESIS_PATH_MAX.
- The generated schema C builds under `-Werror` with newer clang. capnpc-c writes `capn_null = {CAPN_NULL}`, which Apple clang and clang 19 report as a missing `has_ptr_tag` initializer. `scripts/gen-capnp-c.sh` now rewrites it as `{.type = CAPN_NULL}`, which zeroes the same fields. The installed `share/phronesis/c/*.capnp.c` carries the change, so an embedder that compiles it, such as ljos-policyd, builds too.
- The shell argv count was capped at 256, so a command such as `git add` of a few hundred paths, or `rm` of a glob, was fieldTooLong before the policy read it. The cap is 32768 words. The copy is allocated for the actual count.
- The snapshot job replaced the assets of the snapshot prerelease on each push to main and left the snapshot tag on an older commit. The release and the tag now both point at the commit the job built.
