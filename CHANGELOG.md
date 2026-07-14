# Changelog

All notable changes to `loxalarm` will be documented in this file.
Format follows Keep a Changelog. The project follows semantic versioning
from v1.0.0 onwards; before that, the API may change without notice.

## [Unreleased]

## [0.1.2] - 2026-07-14

### Added
- Portable snapshot encode/decode helpers with caller-supplied schema IDs.
- Clock-range validation and backward-jump rejection.

### Changed
- Saturating diagnostic counters for activation, reactivation, shelving, and acknowledgements.
- Snapshot persistence now uses explicit wire metadata and reserved-field validation.
- Public version macros and CMake project version advanced to 0.1.2.
- CI and release checks now cover C++ header consumption and version consistency.

## [0.1.1] - 2026-05-23

### Added
- CMake install rules and package export (`loxalarmTargets.cmake`, `loxalarmConfig.cmake`, `loxalarmConfigVersion.cmake`).
- CI install smoke test and `find_package` consumer smoke test.
- Evidence docs: `docs/test-plan.md`, `docs/release-checklist.md`, `docs/evidence-matrix.md`.

### Changed
- README no longer claims hysteresis is implemented in the alarm core; hysteresis is documented as upstream condition logic.
- Snapshot load validation hardened (version/state/resume validation; shelving metadata handling).
- Entering OUT_OF_SERVICE now clears shelving metadata.
- Unit tests expanded for snapshot validation edge cases, overflow handling, one-shot flags, and `needs_attention`/NULL helpers.
- Fuzz-like test now fails deterministically with non-zero exit and prints failure context instead of crashing.
- Release workflow bundles `cmake/` and creates an install-tree archive.
- SECURITY policy updated to prefer private disclosure.

## [0.1.0] - 2026-05-22

### Added
- Header-only implementation + public API (`include/loxalarm/loxalarm.h`).
- State model + docs (`docs/state-model.md`, `docs/limitations.md`, `docs/integration.md`).
- Acceptance tests + fuzz-like invariant test (`tests/test_loxalarm.c`, `tests/test_loxalarm_fuzzlike.c`).
- Examples (`examples/minimal.c`, `examples/shelve_and_persist.c`).
- CMake build + CTest integration (`CMakeLists.txt`).
- GitHub Actions CI + header self-contained checks (`.github/workflows/ci.yml`).
