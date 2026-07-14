# Platform matrix gaps remain unverified

## Classification
DEFERRED / NON-BLOCKING

## Problem
The user-provided WSL verification covered host builds, install, and C/C++ consumers, but not the broader platform claims in the release prompts.

## Required evidence
- Linux GCC and clang release builds and tests.
- macOS clang build and tests.
- MSVC warning-as-error build coverage in CI.
- ARM Cortex-M cross-compile smoke build if toolchains are available.
- Freestanding or no-OS smoke build if supported by the project.

## Acceptance criteria
- Each supported matrix entry either passes or is explicitly marked unsupported.
- Unsupported entries are documented in limitations and the issue backlog.

## Non-goals
- No runtime feature additions.
- No change to the alarm-state contract.

## Suggested labels
- `platform`
- `ci`
- `verification`
