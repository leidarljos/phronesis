# Changelog

<!-- towncrier release notes start -->

## 0.2.3 (2026-10-18)

### Fixes

- A shell command whose Cap'n body is larger than 512 KiB, such as about 30000 one-byte argv words, was refused as invalidMessage with an empty reason, before the argv count was checked. checkShell now denies that body as fieldTooLong, and the decision's reason says the body exceeds 524288 bytes.
- Shell argv words were copied into a 512-byte path slot, so a word of 512 bytes or more was refused as fieldTooLong before the shell policy could read it. Words are stored at their real length, up to 128 KiB (Linux MAX_ARG_STRLEN). The Cap'n body limit is 512 KiB so a word of that size fits. A longer word is still fieldTooLong, and that decision now carries a reason. Paths stay bounded by PHRONESIS_PATH_MAX.
- The generated schema C builds under `-Werror` with newer clang. capnpc-c writes `capn_null = {CAPN_NULL}`, which Apple clang and clang 19 report as a missing `has_ptr_tag` initializer. `scripts/gen-capnp-c.sh` now rewrites it as `{.type = CAPN_NULL}`, which zeroes the same fields. The installed `share/phronesis/c/*.capnp.c` carries the change, so an embedder that compiles it, such as ljos-policyd, builds too.
- The shell argv count was capped at 256, so a command such as `git add` of a few hundred paths, or `rm` of a glob, was fieldTooLong before the policy read it. The cap is 32768 words. The copy is allocated for the actual count.
- The snapshot job replaced the assets of the snapshot prerelease on each push to main and left the snapshot tag on an older commit. The release and the tag now both point at the commit the job built.
