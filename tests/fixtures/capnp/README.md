# Cap'n fixtures (optional)

Product tests build Cap'n bodies with **c-capnproto** in cmocka
(`tests/test_capnp_ffi.c`) and dispatch via Cap'n methods (`policyd_check_shell`, …).

Checked-in `.bin` blobs (if present) are historical snapshots only; the
suite does not require them.
