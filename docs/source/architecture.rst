Architecture: C ABI vs Cap'n
============================

Two planes
----------

.. list-table::
   :header-rows: 1
   :widths: 20 35 45

   * - Plane
     - Package
     - Transport
   * - Seat goals
     - grokos-session
     - Cap'n Proto (GKSP) over UDS
   * - Multi-agent TCB
     - **grok-policyd** (this repo)
     - **Stable C ABI** (this document)
   * - Session↔TCB
     - future daemon + meta #70
     - Cap'n peer wire (not this header)

Comparison with rgpot / featomic / metatensor
---------------------------------------------

Those libraries implement the core in **Rust** and emit the C header with
**cbindgen** (Doxygen-style comments from Rust docs). The installable
``*.h`` is generated, never hand-edited.

**grok-policyd** is the inverse: the core is **C**, so
``include/grok-policyd/supervisor.h`` **is** the source of truth. There is
no cbindgen pipeline to maintain. The product stack still matches the
ecosystem pattern:

1. Versioned **shared/static library** + **pkg-config**
2. **Doxygen** on the public header
3. **Sphinx + breathe** reference (featomic-style C API pages)
4. Embedding guide for hosts (shell, future daemon)
5. Host tests with **cmocka** (``make test``; not a second framework)

Cap'n Proto remains the cross-process bus for seat work and, later, the
policyd peer. The C ABI is for in-process hosts and for a future daemon
that wraps the same library.

ABI rules
---------

* Opaque ``grok_supervisor_t``: layout free to change.
* Public structs (``grok_agent_status_t``, ``grok_policy_result_t``) and
  fixed buffer sizes: stable within ``GROK_POLICYD_API_VERSION``.
* Additive functions: allowed without API bump.

**Package version vs API generation vs SONAME** (do not conflate):

.. list-table::
   :header-rows: 1
   :widths: 25 30 45

   * - Counter
     - Source
     - What it is for
   * - Package semver
     - ``VERSION`` file
     - Human/release number (``0.1.0``)
   * - SONAME major
     - package major component
     - ELF link name ``libgrok_policyd.so.0``
   * - API generation
     - ``API_VERSION`` file
     - Link-compat counter for the C surface

* **SONAME** tracks **package major** only (``libgrok_policyd.so.$(VERSION_MAJOR)``).
  Bump package major (and thus SONAME) on packaging breaks that force a soname
  rename (typical distro policy). While major stays ``0``, SONAME stays ``.so.0``.
* **``GROK_POLICYD_API_VERSION``** is a **separate** link-compat integer. Embedders
  key on this for “is this header generation compatible with the loaded
  ``.so``?”, not on SONAME. Bump it when removing symbols, changing public
  struct layouts, or breaking return-code semantics.
* A single release may bump API without bumping package major (still rare while
  major is 0); it may bump package major without an API break (packaging-only).
  Document both in the changelog when either moves.
* Single source: edit ``VERSION`` / ``API_VERSION``, run
  ``./scripts/sync-version.sh``, then ``./scripts/check-version.sh`` (also run
  from ``make lib`` / ``pixi run lib``).
