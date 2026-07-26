C API reference
===============

Generated from Doxygen comments in ``include/grok-policyd/supervisor.h``.

Version
-------

.. doxygendefine:: GROK_POLICYD_VERSION
.. doxygendefine:: GROK_POLICYD_API_VERSION
.. doxygenfunction:: grok_policyd_version_string
.. doxygenfunction:: grok_policyd_api_version

Status codes
------------

.. doxygendefine:: GROK_OK
.. doxygendefine:: GROK_ERR_INVAL
.. doxygendefine:: GROK_ERR_EXISTS
.. doxygendefine:: GROK_ERR_NOTFOUND
.. doxygendefine:: GROK_ERR_IO
.. doxygendefine:: GROK_ERR_SPAWN
.. doxygendefine:: GROK_ERR_STATE
.. doxygendefine:: GROK_ERR_DENIED

Types
-----

.. doxygenenum:: grok_agent_state_t
.. doxygenenum:: grok_decision_t
.. doxygenstruct:: grok_agent_status_t
   :members:
.. doxygenstruct:: grok_policy_result_t
   :members:

Supervisor lifecycle
--------------------

.. doxygenfunction:: grok_supervisor_open
.. doxygenfunction:: grok_supervisor_close
.. doxygenfunction:: grok_supervisor_action_log_path
.. doxygenfunction:: grok_supervisor_state_dir
.. doxygenfunction:: grok_supervisor_runtime_dir

Agent control
-------------

.. doxygenfunction:: grok_supervisor_start
.. doxygenfunction:: grok_supervisor_status
.. doxygenfunction:: grok_supervisor_stop
.. doxygenfunction:: grok_supervisor_log
.. doxygenfunction:: grok_supervisor_log_last

Policy
------

.. doxygenfunction:: grok_policy_check
