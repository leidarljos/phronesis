Host tests (cmocka)
===================

Host unit tests live under ``tests/`` and use **cmocka**. The supported way to
get the toolchain is **pixi** (see package ``pixi.toml``), not ad-hoc system
packages.

Dependencies
------------

.. code-block:: bash

   pixi install --locked
   # provides: C compiler, make, pkg-config, cmocka

``pkg-config --libs cmocka`` succeeds inside ``pixi run``. ``pixi run lib`` and
``pixi run -e docs doxygen`` do not need the test binary; ``pixi run test`` does.

Run
---

.. code-block:: bash

   pixi run test
   # or: pixi run build   # lib + test + example
   # or: pixi run ci      # env-info + build

Suites (``tests/test_*.c`` → ``cmocka_run_group_tests_name``):

* ``paths`` — open, ``/tmp`` runtime refuse, env overrides
* ``lifecycle`` — start / status / stop / restart
* ``kill_tree`` — process-group and nested stop
* ``action_log`` — JSONL append / order
* ``persist`` — slot files across reopen; CLI roundtrip
* ``policy`` — default deny, workspace allowlist, high-risk prompt
* ``version`` — ``GROK_POLICYD_VERSION`` / API generation

Layout
------

.. code-block:: text

   tests/harness.c|.h     shared tmpdirs + open helpers
   tests/test_*.c         one cmocka group per file
   tests/test_main.c      runs every group; non-zero if any fail

CI job ``build:pixi`` runs ``pixi run -e ci build`` (cmocka from the lockfile).
