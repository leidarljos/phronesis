grok-policyd
============

Policy / multi-agent supervisor TCB for GrokOS. Host **C library** with a
stable public ABI, plus a small CLI. Sessiond Cap'n Proto peer wire is a
separate track (meta ``#70``).

.. toctree::
   :maxdepth: 2
   :caption: Contents

   embedding
   architecture
   testing
   reference/c_api

Quick links
-----------

* Public header: ``include/grok-policyd/supervisor.h``
* Install: ``make install PREFIX=/usr/local``
* pkg-config: ``pkg-config --cflags --libs grok-policyd``
* Example: ``examples/c/minimal.c`` (``make example``)
