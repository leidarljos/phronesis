Host tests (cmocka)
===================

Host unit tests live under ``tests/`` and use **cmocka** (same as the package
README / CI). There is no second test harness for the C library.

Dependencies
------------

.. code-block:: bash

   # Alpine
   apk add build-base cmocka-dev pkgconf

   # Debian/Ubuntu
   apt install build-essential libcmocka-dev pkg-config

   # macOS
   brew install cmocka pkg-config

``pkg-config --libs cmocka`` must succeed. ``make lib``, ``make example``, and
``make doxygen`` do **not** need cmocka; ``make test`` / ``make all`` do.

Run
---

.. code-block:: bash

   make test
   # builds build/libgrok_policyd.a, build/grok-policyd, build/supervisor_test
   # then runs the cmocka suites

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

CI (``.gitlab-ci.yml`` job ``test:host``) installs ``cmocka-dev`` and runs
``make clean lib test example``.
