# PGRAPH inline packet test

`make check` validates the capacity proof used before bulk packet writes and
the exact low-then-high element order for `NV097_ARRAY_ELEMENT16`.

The runtime bulk path remains disabled for incrementing packets and active
method tracing. Untraced non-incrementing packets use the same bounded bulk
path at every length, avoiding an unmeasured packet-size cutoff. Diagnostic
counters are intentionally absent from the production hot path. Long title or
composite runs are still required for performance claims; this test is a
correctness contract only.
