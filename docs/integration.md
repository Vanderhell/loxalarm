# loxalarm - Integration with the Lox family

`loxalarm` is one runtime object: a single alarm condition with its state
machine. By itself it has no I/O, no persistence, and no transport. This
document shows how it composes with the other modules in the Lox family.

## Typical wiring

```
   sensor/health input
          │
          ▼
   ┌─────────────────┐
   │   microhealth   │  thresholds → bool condition
   └────────┬────────┘
            │
            ▼
   ┌─────────────────┐         ┌──────────────┐
   │    loxalarm     │◄────────│  microconf   │  (limits, delays)
   │  (state core)   │         └──────────────┘
   └────────┬────────┘
            │
       reason flags
            │
            ├─────────► microlog  (transition log)
            │
            ├─────────► nvlog / loxdb  (snapshot for power-loss)
            │
            └─────────► microsh   ("alarm list", "alarm ack 3")
```

## With `microhealth`

`microhealth` produces threshold-crossing booleans. `loxalarm` consumes
those.

```c
bool over = microhealth_get_bool(&hm, "tank_pressure_high");
lox_alarm_update(&pressure_high, over, now_ms);
```

The split is intentional: `microhealth` is about **observability**
(metrics and thresholds). `loxalarm` is about **lifecycle** (on/off
delay, latch, ack, shelve).

## With `microconf`

Store alarm configuration in your config schema and pass pointers:

```c
typedef struct {
    uint32_t on_delay_ms;
    uint32_t off_delay_ms;
    uint32_t max_shelve_ms;
    uint8_t  latched_bits;        /* one bit per alarm */
    uint8_t  shelvable_bits;
} cfg_alarms_t;

static lox_alarm_config_t cfg_pressure;
cfg_pressure.on_delay_ms   = cfg.alarms.on_delay_ms;
cfg_pressure.off_delay_ms  = cfg.alarms.off_delay_ms;
cfg_pressure.latched       = (cfg.alarms.latched_bits & BIT(0)) != 0;
cfg_pressure.shelvable     = (cfg.alarms.shelvable_bits & BIT(0)) != 0;
cfg_pressure.max_shelve_ms = cfg.alarms.max_shelve_ms;
cfg_pressure.tag           = "tank_pressure_high";

lox_alarm_init(&pressure_high, &cfg_pressure);
```

## With `microlog`

Log transitions via the drain pattern:

```c
lox_alarm_update(&a, cond, now_ms);

uint32_t reasons = lox_alarm_drain_reason_flags(&a);
if (reasons != 0) {
    microlog_record_t r = {
        .ts       = now_ms,
        .tag      = a.cfg->tag,
        .state    = (uint8_t)lox_alarm_state(&a),
        .reasons  = reasons,
    };
    microlog_emit(&log, MICROLOG_LEVEL_INFO, &r);
}
```

## With `nvlog` / `loxdb` (persistence across reboot)

`loxalarm` does not write to non-volatile storage. You decide when to
snapshot:

```c
/* on every transition, or on a slow timer */
lox_alarm_snapshot_t snap;
if (lox_alarm_snapshot_save(&pressure_high, &snap) == LOX_OK) {
    nvlog_append(&store, NV_KEY_PRESSURE_ALARM,
                 &snap, sizeof(snap));
}

/* on boot */
lox_alarm_snapshot_t snap;
size_t n = sizeof(snap);
if (nvlog_read_latest(&store, NV_KEY_PRESSURE_ALARM, &snap, &n) == 0) {
    lox_alarm_snapshot_load(&pressure_high, &cfg_pressure,
                            &snap, now_ms);
} else {
    lox_alarm_init(&pressure_high, &cfg_pressure);
}
```

This is how you preserve a LATCHED_RETURN across power loss: the operator
sees the alarm again after reboot and can acknowledge it.

## With `microsh` (operator commands)

Wire shell commands to alarm operations:

```c
static int sh_alarm_ack(int argc, char **argv) {
    if (argc < 2) return -1;
    lox_alarm_t *a = find_alarm(argv[1]);
    if (!a) return -1;
    return lox_alarm_ack(a, now_ms(), MICROSH_OPERATOR_ID);
}

static int sh_alarm_shelve(int argc, char **argv) {
    if (argc < 3) return -1;
    lox_alarm_t *a = find_alarm(argv[1]);
    uint32_t mins = (uint32_t)atoi(argv[2]);
    return lox_alarm_shelve(a, mins * 60 * 1000,
                            now_ms(), MICROSH_OPERATOR_ID);
}

microsh_register(&sh, "alarm-ack",    sh_alarm_ack);
microsh_register(&sh, "alarm-shelve", sh_alarm_shelve);
```

## With `loxguard`

A guard block can wrap the entire alarm update cycle. If something
inside the alarm callbacks goes wrong, the guard captures the failure
without taking down the system:

```c
LOX_GUARD_BLOCK(alarm_tick_guard) {
    for (size_t i = 0; i < ALARM_COUNT; ++i) {
        lox_alarm_update(&alarms[i], conditions[i], now_ms);
    }
}
```

## With `loxbudget`

Use `loxbudget` to throttle alarm-driven side effects. For example,
publishing every alarm transition over MQTT can flood a slow link;
gate the publish through admission control:

```c
if (lox_alarm_just_activated(&a)) {
    if (loxbudget_check(&mqtt_budget, OP_MQTT_PUBLISH) == LOX_OK) {
        mqtt_publish_alarm(&a);
    } else {
        /* dropped or queued for later */
        deferred_publish(&a);
    }
}
```

## With `loxperm` (when available)

`loxperm` is the interlock/permissive evaluator (separate library).
Their relationship:

- `loxperm` answers "may this action start?" - pre-flight.
- `loxalarm` answers "what is the operator-visible state of this
  condition?" — observability.

An interlock chain can use alarm states as one of its conditions:

```c
loxperm_condition_t cond_pressure = {
    .source  = &pressure_alarm,
    .source_check = (loxperm_check_fn)lox_alarm_is_active,
    .negate  = true,   /* permit only if NOT active */
};
```

## With `loxseq` (when available)

`loxseq` is the power-loss-aware sequencer. An alarm being LATCHED can
gate sequence resume:

```c
if (lox_alarm_needs_attention(&pressure_high)) {
    loxseq_request_safe_init(&batch_seq);
} else {
    loxseq_request_resume(&batch_seq);
}
```
