## 2024-05-19 - NV2A PGRAPH RMW Consolidation
**Learning:** `PG_SET_MASK` in NV2A emulation performs a full read-modify-write on every invocation. Consecutive calls targeting different fields of the same register (like `NV_PGRAPH_TEXFMT0` or `NV_PGRAPH_FOGCOLOR`) multiply the overhead of register reads and writes.
**Action:** Consolidate multiple `PG_SET_MASK` calls to the same register into a single `pgraph_reg_r()`, multiple `SET_MASK()` operations on the local value, and a single `pgraph_reg_w()`.
