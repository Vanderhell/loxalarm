# loxalarm

[![CI](https://github.com/Vanderhell/loxalarm/actions/workflows/ci.yml/badge.svg)](https://github.com/Vanderhell/loxalarm/actions/workflows/ci.yml)
[![Release](https://github.com/Vanderhell/loxalarm/actions/workflows/release.yml/badge.svg)](https://github.com/Vanderhell/loxalarm/actions/workflows/release.yml)

**Deterministic alarm-state core for embedded C firmware.**

`loxalarm` is a small, heap-free **C99** library that implements the runtime
state model of a process alarm: on-delay, off-delay, latch, acknowledge,
shelving, and reason flags.

It is designed for MCU firmware that needs PLC-style alarm semantics
(latch-until-acked, shelve-without-losing-events, chattering suppression)
without depending on a PLC vendor stack, an HMI, an OS, or dynamic memory.

```c
#include "loxalarm/loxalarm.h"

static lox_alarm_t pressure_alarm;

static const lox_alarm_config_t pressure_alarm_cfg = {
    .on_delay_ms       = 2000,   // condition must hold 2 s before active
    .off_delay_ms      = 5000,   // must clear 5 s before inactive
    .latched           = true,   // requires explicit ack
    .shelvable         = true,
    .max_shelve_ms     = 15 * 60 * 1000, // 15 min cap
};

lox_alarm_init(&pressure_alarm, &pressure_alarm_cfg);

// in your control loop:
lox_alarm_update(&pressure_alarm, pressure_above_limit, now_ms);

if (lox_alarm_is_active(&pressure_alarm)) {
    raise_horn();
}
```

---

## What `loxalarm` is

A single, well-defined runtime object: one alarm condition with one
lifecycle. You instantiate as many as you need; each owns its own state
struct (caller-allocated).

The lifecycle is modelled on the alarm states used by **ISA 18.2** and
**OPC UA Part 9 (Alarms & Conditions)**:

```
                  +-------------+
                  |  NORMAL     |
                  +------+------+
                         |
              condition true >= on_delay
                         v
                  +-------------+
            +----->  ACTIVE     +-----+
            |     +------+------+     |
            |            |            |
            |   condition false       |  shelve()
            |   >= off_delay          v
            |            |     +------+------+
            |            |     |  SHELVED    |
            |            |     +------+------+
            |            |            |
            |            v   max_shelve_ms timeout
            |     +-------------+     |
            +-----+ LATCHED-RTN <-----+
                  +------+------+
                         |
                       ack()
                         v
                  +-------------+
                  |  NORMAL     |
                  +-------------+
```

(See `docs/state-model.md` for the full state table including out-of-service.)

## What `loxalarm` is **not**

- It is not a safety-rated alarm system. It does **not** claim SIL
  compliance, IEC 61508 conformance, or any functional safety certification.
- It is not an HMI or alarm history database. It exposes transitions; you
  decide how to render or persist them.
- It is not a network protocol. There is no built-in OPC UA, MQTT, or
  Modbus binding. Those live in adapter layers above `loxalarm`.
- It is not a generic FSM. Use `microfsm` for arbitrary state machines.
  `loxalarm` implements one specific, well-known state model: the alarm.

## Design rules

- **C99**, no compiler extensions.
- **Single-header distribution** option (`loxalarm/loxalarm_single.h`).
- **No heap** - all state is caller-owned (`lox_alarm_t` on stack or in
  static memory).
- **No floating point.** Time is `uint32_t` milliseconds, supplied by the
  caller.
- **No global mutable state.** Every function takes the alarm pointer.
- **No HW dependency.** Caller passes the current condition (bool) and the
  current monotonic time. Time source is your business.
- **Deterministic.** `lox_alarm_update()` has O(1) work and a bounded set
  of state transitions per call.
- **Reentrancy: per-instance.** Two alarms can be updated from two threads
  without locks. The same alarm from two contexts is not supported.

## Minimum viable use

```c
#include "loxalarm/loxalarm.h"

lox_alarm_t a;
lox_alarm_init(&a, &(lox_alarm_config_t){ .latched = true });

while (running) {
    bool cond = read_sensor() > 80;
    lox_alarm_update(&a, cond, get_monotonic_ms());

    if (lox_alarm_just_activated(&a)) {
        log_event("HIGH_TEMP raised");
    }
    if (lox_alarm_just_returned(&a)) {
        log_event("HIGH_TEMP returned to normal");
    }
}

// operator acknowledged via shell or HMI:
lox_alarm_ack(&a, get_monotonic_ms(), OPERATOR_ID);
```

## What problem this solves

Process firmware repeatedly hand-rolls alarm logic:

```c
// the bad version that lives in 10 000 firmware projects
if (pressure > LIMIT) {
    if (!alarm_state) {
        alarm_state = true;
        alarm_time  = now;
        log("ALARM");
    }
} else if (alarm_state && (now - alarm_time > 3000)) {
    alarm_state = false;
}
```

That snippet has at least five real problems:

- no on-delay (chattering at threshold causes alarm flap)
- no clear separation between signal logic and alarm semantics
- no latch (alarm vanishes before operator sees it)
- no acknowledge (no concept of operator-aware reset)
- no shelve (cannot temporarily silence during maintenance without losing
  the underlying state)

`loxalarm` provides one tested, predictable implementation of all of these.

Note: `loxalarm` consumes a boolean condition; if you need hysteresis, implement
it in your signal/threshold logic before calling `lox_alarm_update()` (see
`docs/state-model.md`).

## Comparison

| Aspect                          | Rockwell P_Alarm | OPC UA UA Server | `loxalarm` |
|---------------------------------|------------------|------------------|------------|
| Target platform                 | PLC (Logix)      | Server-side      | MCU C99    |
| Heap allocation                 | n/a              | yes              | **no**     |
| Vendor lock-in                  | Rockwell         | OPC Foundation   | none       |
| State model                     | Ack/Shelf/Supp   | ISA 18.2 full    | ISA 18.2 subset |
| HMI binding included            | yes              | yes              | **no**     |
| Audit / history included        | yes              | yes              | **no**     |
| Suitable for bare-metal MCU     | no               | no               | **yes**    |

`loxalarm` is intentionally narrower than either. It is the runtime alarm
**object** - not the HMI, not the historian, not the server.

## Integration with the Lox family

- **`microhealth`** - provides the input condition (`is_overheat()`).
- **`microconf`** - stores limits, delays, shelve duration as config.
- **`microlog`** - receives transition events for the audit trail.
- **`microsh`** - exposes `alarm ack`, `alarm shelve`, `alarm list` commands.
- **`nvlog` / `loxdb`** - optional persistence of the latched and shelved
  states across reboot. (`loxalarm` does not persist by itself; it offers
  a snapshot/restore pair if persistence is wanted.)

## Building

Drop the `include/` directory into your project. There is nothing to link.

```c
#include "loxalarm/loxalarm.h"

// or, for projects that prefer one-header builds:
#include "loxalarm/loxalarm_single.h"
```

The implementation is header-only (`static inline`) in v0.1.

### CMake

```sh
cmake -S . -B build
cmake --build build
```

## Status

Pre-1.0. Public API is **not** stable yet. Semver applies from v1.0.0
onwards.

## License

MIT. See `LICENSE`.
