Architecture: C ABI vs Cap'n
============================

Two planes
----------

==============  ============================  =================================
Plane           Package                       Transport
==============  ============================  =================================
Seat goals      grokos-session                Cap'n Proto (GKSP) over UDS
Multi-agent TCB **grok-policyd** (this repo)  **Stable C ABI** (this document)
Session↔TCB     future daemon + meta #70      Cap'n peer wire (not this header)
==============  ============================  =================================

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

Cap'n Proto remains the cross-process bus for seat work and, later, the
policyd peer. The C ABI is for in-process hosts and for a future daemon
that wraps the same library.

ABI rules
---------

* Opaque ``grok_supervisor_t``: layout free to change.
* Public structs (``grok_agent_status_t``, ``grok_policy_result_t``) and
  fixed buffer sizes: stable within ``GROK_POLICYD_API_VERSION``.
* Additive functions: allowed without API bump.
* Breaking change: bump ``GROK_POLICYD_API_VERSION`` and SONAME major.
