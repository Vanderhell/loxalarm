# loxalarm - State model

`loxalarm` implements a runtime subset of the alarm state model defined by
**ISA 18.2 (Management of Alarm Systems for the Process Industries)** and
**OPC UA Part 9 (Alarms & Conditions)**. It uses that vocabulary, but it is
not a standards-conformance claim.

This document describes exactly which states exist, when transitions
happen, and what the operator-facing semantics are.

## States

| State              | Meaning                                                                     |
|--------------------|-----------------------------------------------------------------------------|
| `NORMAL`           | Condition is false. No operator attention needed.                           |
| `ACTIVE`           | Condition has been true for at least `on_delay_ms`.                         |
| `LATCHED_RETURN`   | Condition cleared while ACTIVE, but `latched=true`. Awaiting ack.           |
| `SHELVED`          | Operator silenced the alarm for a bounded duration.                         |
| `SUPPRESSED`       | Reserved for future work (not entered by v0.1 core).                        |
| `OUT_OF_SERVICE`   | Maintenance state. Condition is not evaluated.                              |

## Transitions

### From NORMAL

- -> `ACTIVE` when `condition == true` and has been true for >= `on_delay_ms`.

### From ACTIVE

- -> `NORMAL` when `condition == false` for >= `off_delay_ms` **and**
  `latched == false`.
- -> `LATCHED_RETURN` when `condition == false` for >= `off_delay_ms` **and**
  `latched == true`.
- -> `SHELVED` when `lox_alarm_shelve()` is called and `shelvable == true`.

### From LATCHED_RETURN

- -> `NORMAL` when `lox_alarm_ack()` is called.
- -> `ACTIVE` when `condition == true` again (re-activation while latched).
- -> `SHELVED` when `lox_alarm_shelve()` is called.

### From SHELVED

- -> previous (ACTIVE or LATCHED_RETURN) when shelve expires or
  `lox_alarm_unshelve()` is called.
- Note: while SHELVED, the underlying condition continues to be tracked
  but does **not** cause `just_activated` flags to fire.

### From SUPPRESSED

`SUPPRESSED` is present as a reserved enum value, but the core does not
provide an API to enter or exit it. If you need suppression today, implement
it above `loxalarm` by gating notifications or by not calling
`lox_alarm_update()` for suppressed alarms.

### From OUT_OF_SERVICE

- -> `NORMAL` when `lox_alarm_set_out_of_service(a, false)` is called.
- While OOS, `lox_alarm_update()` is a no-op for condition tracking.

`lox_alarm_reset()` is a force reset: it clears the live state back to
`NORMAL` without discarding lifetime counters.

## Why on-delay and off-delay are not symmetrical

`on_delay_ms` exists to suppress short transient excursions (signal noise,
inrush spikes, sensor bounce).

`off_delay_ms` exists for a different reason: to suppress brief returns
to normal during ongoing problems, so the alarm does not chatter when the
process is genuinely unstable near the threshold.

They are independent because the natural durations are different. A
typical configuration is on_delay=2s, off_delay=10s.

## Why hysteresis is not in `loxalarm`

Hysteresis (separate set and clear thresholds) is a property of the
**input signal**, not of the alarm state machine. The caller passes a
single boolean condition. If you need hysteresis, compute it before
calling `lox_alarm_update()`:

```c
bool high_temp = (temp > 80) || (last_high_temp && temp > 75);
lox_alarm_update(&a, high_temp, now_ms);
```

This keeps the alarm core narrow and gives you full control over the
threshold logic.

## Diagnostic flags

After each `lox_alarm_update()` call, the following one-shot flags may be
set:

- `just_activated` — alarm just entered ACTIVE
- `just_returned` — condition just cleared (after off_delay)
- `just_acked` — ack happened this cycle
- `just_shelved` — shelve happened this cycle

Reading via `lox_alarm_just_activated()` etc. clears the flag.

For a single transition record per cycle, use
`lox_alarm_drain_reason_flags()` which returns and clears an OR of
`lox_alarm_reason_t` values.

## What this model does NOT do

- **Severity / priority**: not modelled. Higher-level code groups alarms
  by priority. `loxalarm` deals with a single condition.
- **Multi-stage alarms** (e.g. high → high-high → critical): use two or
  three separate `lox_alarm_t` instances with different thresholds.
- **First-out detection**: not in `loxalarm`. See `loxperm` for interlock
  first-out semantics.
- **Audit history**: not stored. Use `microlog` with the reason flags.

## Compliance disclaimers

`loxalarm` is **not** a functional-safety library. It does not claim
SIL 1/2/3, IEC 61508 conformance, or any safety certification.

The state model is named after ISA 18.2 / OPC UA Part 9 because that is
the most widely understood vocabulary for process alarms. The
implementation is informed by those standards but is not certified
against them and does not implement the full OPC UA Condition node tree.
