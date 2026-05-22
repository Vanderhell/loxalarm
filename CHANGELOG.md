# Changelog

All notable changes to `loxalarm` will be documented in this file.
Format follows Keep a Changelog. The project follows semantic versioning
from v1.0.0 onwards; before that, the API may change without notice.

## [Unreleased]

### Added
-

### Not yet
- libFuzzer harness.
- Cortex-M0+ memory profiling.

## [0.1.0] - 2026-05-22

### Added
- Header-only implementation + public API (`include/loxalarm/loxalarm.h`).
- State model + docs (`docs/state-model.md`, `docs/limitations.md`, `docs/integration.md`).
- Acceptance tests + fuzz-like invariant test (`tests/test_loxalarm.c`, `tests/test_loxalarm_fuzzlike.c`).
- Examples (`examples/minimal.c`, `examples/shelve_and_persist.c`).
- CMake build + CTest integration (`CMakeLists.txt`).
- GitHub Actions CI + header self-contained checks (`.github/workflows/ci.yml`).
