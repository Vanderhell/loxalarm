# loxalarm - Test plan

This repository contains:

- Unit tests (`tests/test_loxalarm.c`): scenario-based checks for the public API
  and state transitions, including snapshot validation and clock wrap at the
  `uint32_t` boundary.
- Fuzz-like invariant test (`tests/test_loxalarm_fuzzlike.c`): a deterministic
  stress test that executes randomized API sequences and checks invariants after
  each operation. Failures print the seed and iteration context and exit non-zero.
- Sanitizer CI job (`.github/workflows/ci.yml`): builds and runs tests with
  ASan/UBSan on Linux (clang).
- Header self-contained checks (`.github/workflows/ci.yml`): compiles a tiny
  translation unit including the public headers with `-Werror` to ensure they
  are standalone and C99-compatible.

