/*
 * examples/minimal.c - Minimal usage of loxalarm.
 *
 * Builds on a host with: cc -I../include minimal.c -o minimal
 * (assuming an impl is present; this example demonstrates the API only)
 */

#include <stdio.h>
#include <loxalarm/loxalarm.h>

/* Imagine: a tank pressure sensor on a small process controller.
 * We want an alarm that:
 *   - waits 2 s before raising (suppress spikes)
 *   - waits 5 s before clearing (avoid chatter at the boundary)
 *   - latches: stays visible until operator acknowledges
 *   - can be shelved for up to 15 min during maintenance
 */

static const lox_alarm_config_t pressure_cfg = {
    .on_delay_ms   = 2000,
    .off_delay_ms  = 5000,
    .latched       = true,
    .shelvable     = true,
    .max_shelve_ms = 15u * 60u * 1000u,
    .tag           = "tank_pressure_high",
};

static lox_alarm_t pressure_alarm;

int main(void) {
    lox_alarm_init(&pressure_alarm, &pressure_cfg);

    /* Pretend control loop. now_ms would come from your monotonic clock. */
    uint32_t now_ms = 0;
    bool over_limit = false;

    for (uint32_t tick = 0; tick < 30; ++tick) {
        now_ms = tick * 1000;

        /* Pretend sensor goes high at t=5 s, stays high until t=20 s. */
        over_limit = (tick >= 5 && tick < 20);

        lox_alarm_update(&pressure_alarm, over_limit, now_ms);

        if (lox_alarm_just_activated(&pressure_alarm)) {
            printf("[t=%us] ALARM ACTIVATED: %s\n",
                   tick, pressure_alarm.cfg->tag);
        }
        if (lox_alarm_just_returned(&pressure_alarm)) {
            printf("[t=%us] condition returned to normal\n", tick);
        }

        switch (lox_alarm_state(&pressure_alarm)) {
            case LOX_ALARM_ACTIVE:
                /* sound the horn, light the lamp, set Modbus bit */
                break;
            case LOX_ALARM_LATCHED_RETURN:
                /* operator hasn't acked yet — keep amber on HMI */
                break;
            default:
                break;
        }
    }

    /* Operator finally acknowledges at t=30 s. */
    lox_err_t r = lox_alarm_ack(&pressure_alarm, 30000, /*op_id=*/42);
    printf("ack result: %d, state now: %d\n",
           r, lox_alarm_state(&pressure_alarm));

    return 0;
}
