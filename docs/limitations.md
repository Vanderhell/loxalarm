# loxalarm - Limitations and non-goals

This document is the contract for what `loxalarm` does **not** do, so you
do not deploy it expecting capabilities it does not have.

## Not a safety library

`loxalarm` is **not** safety-rated. It is not:

- SIL 1, 2, or 3 conformant
- IEC 61508 conformant
- IEC 61511 conformant
- ISO 26262 ASIL-anything
- DO-178C qualified

If you need an alarm that participates in a functional-safety loop, use
a certified Safety Instrumented System (SIS). Use `loxalarm` for
operator-visible process alarms in the basic process control system
(BPCS) layer, where the failure of the alarm itself is not a hazard.

## No condition logic

`loxalarm` takes a `bool`. It does not:

- read sensors
- apply thresholds
- compute hysteresis
- average or filter signals
- detect rate-of-change

These belong upstream of `loxalarm`. Keep them there.

## No transport

`loxalarm` does not push events to anywhere. No MQTT, no OPC UA, no
Modbus, no email. Use the `just_*` flags or
`lox_alarm_drain_reason_flags()` and wire the publishing yourself.

## No persistence

`loxalarm` does not write to flash, EEPROM, or NVRAM by itself. It
provides a snapshot/load API; you decide when to call it and where to
store the result.

If you do not persist and the device reboots while an alarm is in
LATCHED_RETURN, the alarm will not be visible after reboot. This is
deliberate: persistence has cost and policy implications that vary by
application.

## No multi-stage alarms

`loxalarm` is one alarm per instance. If you want "high → high-high →
critical", instantiate three alarms with three thresholds. They do not
coordinate; that is the caller's job.

## No global alarm list

`loxalarm` does not maintain a registry of all alarm instances. You
own the array (or whatever container). Iterate it yourself.

## No localisation

The `tag` field is an opaque `const char *`. There is no message
catalogue, no language switching, no parametric formatting. If you need
localised messages, look up the tag in your own table.

## No first-out detection

If multiple conditions trip near-simultaneously, `loxalarm` does not
identify which one tripped first across instances. For that, use
`loxperm`'s first-out detection on an interlock group, or correlate
manually using `state_entered_ms`.

## Time source: caller's responsibility

`loxalarm` does not read a clock. You pass `now_ms` to every call. If
your clock jumps (NTP step, RTC correction), behaviour is undefined for
that update. Skip the update during a clock jump or call
`lox_alarm_reset()`.

Monotonic milliseconds are required. A wall-clock that can go backwards
will misbehave.

`uint32_t` ms wraps at ~49.7 days. `loxalarm` handles wrap correctly
provided no single delay exceeds 24.85 days (half the wrap window).

## Re-entrancy: per-instance

Two threads may update two **different** alarm instances concurrently
without locks. The same instance from two contexts requires external
synchronisation. The library has no internal mutexes.

ISR safety: `lox_alarm_update()` does no I/O and no allocation. It is
safe to call from a low-priority ISR provided you ensure no other
context touches the same instance.

## What about OPC UA / ISA 18.2 conformance?

The state model is named after these standards and follows their
vocabulary because that is how the process industry talks about alarms.
`loxalarm` implements a runtime subset of those state models.

It does **not** implement:

- the full OPC UA AlarmConditionType node tree
- ISA 18.2 documentation/lifecycle/MOC processes (these are organisational,
  not runtime)
- AcknowledgeableConditionType, DiscreteAlarmType, NonExclusiveLimitAlarmType,
  etc., as distinct types - `loxalarm` is one struct with config flags

If you need OPC UA conformance, treat `loxalarm` as the state core and
write a wrapper that exposes the OPC UA node types over it.

## Known issues / TODO before v1.0

- Per-instance memory footprint not yet profiled against M0+ targets.
- Snapshot format is v1. The library validates and restores snapshots for
  this version, but cross-version migration/validation policy is still the
  caller's responsibility if you persist snapshots across library upgrades.
- Snapshot integrity (bit flips, torn writes) is not checked by `loxalarm`.
  If you store snapshots in flash/EEPROM/NVRAM, add integrity protection in
  your persistence backend (e.g. CRC, redundancy, journaling).
- The repository includes a deterministic fuzz-like invariant test for the
  update loop, but it is not a coverage-guided fuzzer.
- The repository includes a unit test that exercises `uint32_t` clock wrap at
  the boundary.
- API for SUPPRESSED is minimal (reserved enum value; enter/exit is managed
  above `loxalarm` in v0.1).
