# Release readiness still has unrun matrix entries

## Classification
DEFERRED / NON-BLOCKING

## Problem
The core library, install tree, and C/C++ consumers are already covered by user-provided WSL evidence, but the full release matrix and release workflow were not exercised end-to-end in this session.

## Required evidence
- Release workflow dry run or equivalent tag validation.
- Archive creation and install-tree packaging on CI.
- Full CI matrix results for all declared platforms.
- Any release-note extraction and version-consistency checks.

## Acceptance criteria
- Release workflow checks succeed without manual intervention.
- Release checklist can be completed from the recorded evidence.

## Non-goals
- No new tagging or publishing.
- No historical tag rewrites.

## Suggested labels
- `release`
- `verification`
- `ci`
