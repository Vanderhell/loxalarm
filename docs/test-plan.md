# loxalarm - Test plan

This repository contains:

- Unit tests (`tests/test_loxalarm.c`): scenario-based checks for the public
  API and state transitions, including clock wrap, backward-jump rejection,
  counter saturation, snapshot validation, and portable snapshot encode/decode.
- Fuzz-like invariant test (`tests/test_loxalarm_fuzzlike.c`): a deterministic
  stress test that executes randomized API sequences and checks invariants after
  each operation. Failures print the seed and iteration context and exit non-zero.
- Sanitizer CI job (`.github/workflows/ci.yml`): builds and runs tests with
  ASan/UBSan on Linux (clang).
- Header self-contained checks (`.github/workflows/ci.yml`): compiles tiny
  C and C++ translation units including the public headers with `-Werror` to
  ensure they are self-contained and portable.
