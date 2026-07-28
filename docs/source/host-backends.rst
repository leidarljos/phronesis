Host backends and portability
=============================

Cap'n + nng are multi-OS. The hold-ups for a multi-OS **product** are not the
bus — they are process lifecycle, path layout, and seat integration.

Layers
------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Layer
     - Portability
   * - Cap'n schema + ``codec_c_capn.c``
     - Portable encode/decode
   * - nng req/rep serve loop
     - Portable transport API; **serve is nng-native** (recv timeout + stop flag)
   * - Policy decisions (``policy.c``)
     - Portable string/path rules
   * - Process engine (``supervisor.c``)
     - **POSIX today** (``fork`` / ``setpgid`` / process-group kill)
   * - cgroup v2 (``cgroup.c``)
     - Linux accelerator; no-op stubs elsewhere
   * - Host backend (``host_*.c``)
     - Optional seat glue — **not** required for Cap'n I/O

Serve is never driven by ``sd_event``. Linux units still get ``READY=`` /
``STOPPING=`` / ``WATCHDOG=`` when built with ``-Dsystemd=enabled`` (auto on
Linux when libsystemd is found).

Meson features
--------------

.. code-block:: bash

   meson setup build                     # auto: systemd+libcap if present
   meson setup build -Dsystemd=disabled  # posix host only (no libsystemd)
   meson setup build -Dlibcap=disabled   # no capability drop

.. list-table::
   :header-rows: 1
   :widths: 20 15 65

   * - Option
     - Default
     - Effect
   * - ``systemd``
     - ``auto``
     - ``host_linux_systemd.c`` vs ``host_posix.c``
   * - ``libcap``
     - ``auto``
     - ``GROK_HAVE_LIBCAP`` + link ``-lcap`` for drop-when-root

Host API (CLI only)
-------------------

Implemented by exactly one of ``src/host_posix.c`` or
``src/host_linux_systemd.c``:

* ``grok_host_init`` / ``fini``
* ``grok_host_should_stop`` / ``request_stop``
* ``grok_host_notify_ready`` / ``notify_stopping`` / ``watchdog_ping``
* ``grok_host_drop_bounding_caps``
* ``grok_host_backend_name`` — log string (``posix``, ``linux-systemd``, …)

Never linked into ``libgrok_policyd``.

Process backend (next ports)
----------------------------

The TCB still assumes a Unix process model. A future Windows (or other) port
implements the same supervisor **semantics** behind a small surface:

* spawn agent (argv, workspace)
* poll / reap
* stop tree (Job Object / process group / …)
* is-alive

Until that exists, ``libgrok_policyd`` remains a Linux/POSIX agent supervisor
with a portable Cap'n face.

IPC ACL
-------

On Unix ``ipc://``, peers are gated with ``NNG_OPT_PEER_UID`` (same-uid). That
is fail-closed when the option is missing. Other OSes need a different auth
story (token in Cap'n, named-pipe ACL, …) — do not assume PEER_UID is universal.
