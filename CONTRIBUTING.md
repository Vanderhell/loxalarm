# Contributing

## Quick start

- Build: `cmake -S . -B build && cmake --build build`
- Run tests: `./build/loxalarm_tests` (Windows: `build\\Release\\loxalarm_tests.exe`)

## Coding guidelines

- C99, no heap allocation, no global mutable state.
- Keep `loxalarm` deterministic: all time is passed in via `now_ms`.
- Prefer small, well-scoped changes with tests in `tests/test_loxalarm.c`.

## Releases

Update `CHANGELOG.md`, bump `LOXALARM_VERSION_*` in `include/loxalarm/loxalarm.h`,
then tag (SemVer starts at `v1.0.0`; before that the API may change).

