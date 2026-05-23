# loxalarm - Evidence matrix

This document maps repository claims to in-repo evidence.

| Claim | Evidence |
|------:|:---------|
| Header-only library | `CMakeLists.txt` defines `add_library(loxalarm INTERFACE)` and installs headers only. |
| No heap allocation | Source inspection: `include/` contains no `malloc/calloc/realloc/free`. |
| Caller-owned state | `lox_alarm_t` is a caller-allocated struct; API takes `lox_alarm_t *` and never allocates. |
| C99 compatible | `CMakeLists.txt` sets `c_std_99`; CI compiles headers with `-std=c99`. |
| `uint32_t` clock wrap handled | Unit test `S12_clock_wrap_uint32_boundary` in `tests/test_loxalarm.c`. |
| Snapshot rejects invalid version/state | Unit tests `S19_snapshot_version_mismatch_rejected`, `S21_invalid_snapshot_state_rejected`, `S22_invalid_snapshot_shelve_resume_state_rejected`, `S23_shelved_snapshot_invalid_resume_state_rejected` in `tests/test_loxalarm.c`. |
| Snapshot does not restore stale shelving metadata when not shelved | Unit test `S24_non_shelved_snapshot_does_not_restore_shelve_metadata` in `tests/test_loxalarm.c`. |
| OOS-from-SHELVED clears shelving metadata | Unit test `S25_oos_from_shelved_clears_shelving_metadata` in `tests/test_loxalarm.c`. |
| Shelve expiry emits reason flag | Unit test `S28_shelve_expiry_sets_reason_flag` in `tests/test_loxalarm.c`. |

