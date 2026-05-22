# loxalarm - Test scenarios (v0.1 acceptance)

This document lists the test scenarios that must pass before tagging
v0.1.0. These scenarios are implemented in `tests/test_loxalarm.c`.

## Conventions

All tests use a virtual clock - `now_ms` is supplied explicitly, not
read from any system source. This makes every test deterministic.

Each scenario is one test function. Failures should report the scenario
name, the input sequence, and the resulting state.

## Scenario set

### S01 — Spike below on_delay does not activate

```
config: on_delay=2000, off_delay=0, latched=false
inputs:
  update(true,  0)
  update(true,  1500)
  update(false, 1700)
expect:
  state == NORMAL
  activation_count == 0
```

### S02 — Sustained condition activates after on_delay

```
config: on_delay=2000
inputs:
  update(true, 0)
  update(true, 1999)        --> still NORMAL
  update(true, 2000)        --> ACTIVE
expect:
  just_activated == true (once)
  activation_count == 1
```

### S03 — off_delay must elapse before clearing

```
config: on_delay=0, off_delay=5000, latched=false
inputs:
  update(true,  0)          --> ACTIVE
  update(false, 1000)
  update(false, 4999)       --> still ACTIVE
  update(false, 5000)       --> NORMAL (via return path)
expect:
  state == NORMAL
  just_returned == true (once)
```

### S04 — Latched alarm goes to LATCHED_RETURN, not NORMAL

```
config: latched=true, off_delay=0
inputs:
  update(true,  0)          --> ACTIVE
  update(false, 100)        --> LATCHED_RETURN
expect:
  state == LATCHED_RETURN
```

### S05 — Ack from LATCHED_RETURN clears to NORMAL

```
preconditions: state == LATCHED_RETURN
inputs:
  ack(now_ms, op_id=5)
expect:
  state == NORMAL
  ack_id stored == 5
  return code == LOX_OK
```

### S06 — Ack from ACTIVE rejected

```
preconditions: state == ACTIVE
inputs:
  ack(now_ms, op_id=5)
expect:
  return code == LOX_ERR_STATE
  state unchanged
```

### S07 — Re-activation while LATCHED_RETURN

```
preconditions: state == LATCHED_RETURN
inputs:
  update(true, now_ms + on_delay_ms)
expect:
  state == ACTIVE
  activation_count incremented
```

### S08 — Shelve respects max duration

```
config: shelvable=true, max_shelve_ms=600000  (10 min)
preconditions: state == ACTIVE
inputs:
  shelve(duration=3600000)   (1 h requested)
  update(true, now+599999)
  update(true, now+600000)
expect:
  shelve was capped to 600000
  state transitions back to ACTIVE at or after t=600000
```

### S09 — Unshelve restores prior state

```
preconditions: was ACTIVE, shelve()'d at t=100
inputs:
  unshelve(t=200)
expect:
  state == ACTIVE
  shelve_expires_ms cleared
```

### S10 — Snapshot round-trip is identity

```
preconditions: instance A in arbitrary reachable state X
inputs:
  snap = snapshot_save(A)
  snapshot_load(B, cfg, snap, now_ms)
expect:
  state(B)            == state(A)
  activation_count(B) == activation_count(A)
  ack_id(B)           == ack_id(A)
```

### S11 — Out-of-service freezes evaluation

```
preconditions: any state
inputs:
  set_out_of_service(true, now)
  update(true,  now+1000)
  update(false, now+5000)
expect:
  state == OUT_OF_SERVICE throughout
  no condition transitions logged
```

### S12 — Clock wrap at uint32_t boundary

```
config: on_delay=2000
inputs:
  update(true, UINT32_MAX - 1000)
  update(true, UINT32_MAX)
  update(true, 1000)            (now_ms wrapped)
expect:
  state == ACTIVE
  no overflow / underflow in delays
```

### S13 — Re-init while running

```
preconditions: instance was ACTIVE
inputs:
  init(&a, &same_cfg)
expect:
  state == NORMAL
  activation_count == 0 (counters reset by init)
```

### S14 — Reset preserves lifetime counters

```
preconditions: state == LATCHED_RETURN, activation_count == 7
inputs:
  reset(&a, now_ms)
expect:
  state == NORMAL
  activation_count == 7 (preserved)
```

### S15 — Reason flags accumulate between drains

```
inputs (no drain between):
  update sequence that triggers ON_DELAY_MET and ACK
expect:
  drain_reason_flags() returns (ON_DELAY_MET | ACK)
  next drain returns 0
```

## Out of scope for v0.1

These tests describe behaviours intentionally deferred:

- SUPPRESSED state behaviour (API minimal in v0.1)
- Stress test: 1000 alarm instances on Cortex-M0+ at 1 kHz update rate
- Power-loss test in real hardware

## Fuzz harness (planned)

A libFuzzer harness will feed random sequences of {update, ack, shelve,
unshelve, OOS toggle, reset, init} with random timestamps (including
wrap) and assert state-machine invariants:

- state is always one of the six defined values
- counters are monotonic across updates within a session
- snapshot/load is always an identity for reachable states
- no UB under -fsanitize=undefined
