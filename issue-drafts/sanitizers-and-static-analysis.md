# Sanitizers and static analysis not yet exercised in this workspace

## Classification
DEFERRED / NON-BLOCKING

## Problem
The repository now has sanitizer-aware CI and stricter release checks, but sanitizer and static-analysis runs were not part of the user-provided WSL evidence.

## Required evidence
- ASan/UBSan build and test run on a supported clang or gcc toolchain.
- Any available static-analysis run such as clang-tidy, cppcheck, or GCC fanalyzer.
- Confirmed absence of sanitizer-only failures in the current alarm core and snapshot decoder.

## Acceptance criteria
- Sanitizer configuration builds and tests pass.
- Any static-analysis findings are either fixed or explicitly documented as false positives with code-level justification.
- Evidence is attached in-repo or in the release checklist.

## Non-goals
- No API expansion.
- No new alarm semantics.

## Suggested labels
- `verification`
- `tooling`
- `ci`
