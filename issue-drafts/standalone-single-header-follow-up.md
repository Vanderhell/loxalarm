# Standalone single-header claim still needs a final decision

## Classification
DEFERRED / NON-BLOCKING

## Problem
`loxalarm_single.h` now behaves as a compatibility include that forwards to `loxalarm.h`. That is honest, but it is not yet a truly standalone amalgamated distribution.

## Required evidence
- Decide whether the project wants a real standalone single-header bundle.
- If yes, generate and verify an actual self-contained header.
- If no, remove any remaining wording that implies standalone distribution.

## Acceptance criteria
- README, docs, and CI all match the chosen distribution model.
- Header-consumption checks reflect the final claim.

## Non-goals
- No semantic change to alarm behavior.

## Suggested labels
- `packaging`
- `docs`
- `api`
