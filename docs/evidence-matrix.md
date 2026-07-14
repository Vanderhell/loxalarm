# loxalarm - Evidence matrix

This document maps repository claims to in-repo evidence.

| Claim | Evidence |
|------:|:---------|
| Header-only library | `CMakeLists.txt` defines `add_library(loxalarm INTERFACE)` and installs headers only. |
| No heap allocation | Source inspection: `include/` contains no `malloc/calloc/realloc/free`. |
| Caller-owned state | `lox_alarm_t` is a caller-allocated struct; API takes `lox_alarm_t *` and never allocates. |
| C99 compatible | `CMakeLists.txt` sets `c_std_99`; CI compiles headers with `-std=c99`. |
| `uint32_t` clock wrap handled and backward jumps rejected | Unit tests `S07_clock_wrap_boundary_is_supported` and `S08_backward_clock_jump_rejected` in `tests/test_loxalarm.c`. |
| Counter saturation does not block transitions | Unit test `S09_counter_saturation_does_not_block_state_transitions` in `tests/test_loxalarm.c`. |
| Snapshot portable encode/decode round-trips | Unit test `S10_snapshot_portable_roundtrip` in `tests/test_loxalarm.c`. |
| Snapshot decoder rejects corruption | Unit test `S11_snapshot_decoder_rejects_corruption` in `tests/test_loxalarm.c`. |
| OOS freezes evaluation and returns to NORMAL | Unit test `S12_out_of_service_freezes_evaluation` in `tests/test_loxalarm.c`. |
| Reset preserves lifetime counters | Unit test `S13_force_reset_preserves_lifetime_counters` in `tests/test_loxalarm.c`. |
| GCC Debug / Release builds and tests passed | User-provided WSL verification. |
| Clang Debug / Release builds and tests passed | User-provided WSL verification. |
| Fuzz-like test passed | User-provided WSL verification. |
| Examples build and run passed | User-provided WSL verification. |
| Install/package smoke passed | User-provided WSL verification. |
| External C `find_package` consumer passed | User-provided WSL verification. |
| External C++ `find_package` consumer passed | User-provided WSL verification. |
| `git diff --check` passed | User-provided WSL verification. |
