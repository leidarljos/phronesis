# Deprecated hand layout table. Encoding constants live in policy/lib/layout.janet
# (loaded by the host). Kept as documentation only; not loaded at runtime.

(def ShellView
  @{:dwords 1
    :pwords 3
    :under-workspace-bit 0
    :cwd-ptr 0
    :argv-ptr 1
    :path-probes-ptr 2})

(def PathProbe
  @{:dwords 1
    :pwords 3
    :exists-bit 0
    :arg-ptr 0
    :resolved-ptr 1
    :head-ptr 2})
