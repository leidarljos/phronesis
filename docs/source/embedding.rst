Embedding the C ABI
===================

Build and install
-----------------

Host tests use **cmocka** (see :doc:`testing`). Library-only build does not.

.. code-block:: bash

   # cmocka-dev / libcmocka-dev + pkg-config required for tests
   make lib test
   make install PREFIX=$HOME/.local
   export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig:$PKG_CONFIG_PATH

Link from C
-----------

.. code-block:: bash

   cc $(pkg-config --cflags grok-policyd) myapp.c \
      $(pkg-config --libs grok-policyd) -o myapp

Or vendor the sources (as ``grokos-shell`` does) and compile
``src/*.c`` into a static archive. The public surface is always
``include/grok-policyd/supervisor.h``.

Minimal open / start / stop
---------------------------

.. code-block:: c

   #include "grok-policyd/supervisor.h"

   grok_supervisor_t *sup = NULL;
   char *argv[] = { "sleep", "60", NULL };

   if (grok_supervisor_open(&sup, state_dir, runtime_dir) != GROK_OK)
       return 1;
   grok_supervisor_start(sup, "agent-a", "agent", workspace, argv);
   grok_supervisor_stop(sup, "agent-a");
   grok_supervisor_close(sup);

Version check
-------------

.. code-block:: c

   if (grok_policyd_api_version() < GROK_POLICYD_API_VERSION) {
       /* headers newer than linked library */
   }

Rust consumers
--------------

Prefer a thin ``extern "C"`` module that mirrors this header (see
``grokos-shell`` ``product/src/policyd.rs``), or generate bindings with
``bindgen`` against the installed header. Do **not** expect a cbindgen
step in this package: the implementation language is C.
