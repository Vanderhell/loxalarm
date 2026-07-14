#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <loxalarm/loxalarm.h>

static uint32_t g_seed = 0;
static uint32_t g_iter = 0;
static uint32_t g_op = 0;
static uint32_t g_now_ms = 0;
static lox_alarm_state_t g_state = LOX_ALARM_NORMAL;

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static const char *op_name(uint32_t op) {
    switch (op) {
        case 0: return "update";
        case 1: return "update";
        case 2: return "update";
        case 3: return "ack";
        case 4: return "shelve";
        case 5: return "unshelve";
        case 6: return "enter_oos";
        case 7: return "exit_oos";
        case 8: return "reset";
        case 9: return "snapshot_roundtrip";
        default: return "unknown";
    }
}

static void require_(bool ok, const char *msg) {
    if (!ok) {
        fprintf(stderr,
                "FAIL fuzzlike: seed=0x%08X iter=%u op=%u(%s) state=%d now_ms=%u: %s\n",
                (unsigned)g_seed,
                (unsigned)g_iter,
                (unsigned)g_op,
                op_name(g_op),
                (int)g_state,
                (unsigned)g_now_ms,
                msg);
        fflush(stderr);
        exit(1);
    }
}

static bool is_valid_state(lox_alarm_state_t s) {
    return s == LOX_ALARM_NORMAL ||
           s == LOX_ALARM_ACTIVE ||
           s == LOX_ALARM_LATCHED_RETURN ||
           s == LOX_ALARM_SHELVED ||
           s == LOX_ALARM_SUPPRESSED ||
           s == LOX_ALARM_OUT_OF_SERVICE;
}

static void invariant_check(const lox_alarm_t *a) {
    require_(a != NULL, "alarm null");
    require_(a->initialised, "not initialised");
    require_(a->cfg != NULL, "cfg null");
    require_(is_valid_state(a->state), "invalid state enum");
    if (a->state != LOX_ALARM_SHELVED) {
        require_(a->shelve_expires_ms == 0, "shelve_expires_ms nonzero while not SHELVED");
        require_(!a->shelve_armed, "shelve_armed true while not SHELVED");
    }
    if (a->state == LOX_ALARM_SHELVED) {
        require_(a->shelve_resume_state == LOX_ALARM_ACTIVE ||
                 a->shelve_resume_state == LOX_ALARM_LATCHED_RETURN,
                 "shelve_resume_state must be ACTIVE or LATCHED_RETURN");
        require_(a->shelve_armed, "shelve_armed false while SHELVED");
    }
}

static void snapshot_roundtrip_check(const lox_alarm_t *a, uint32_t now_ms) {
    lox_alarm_snapshot_t snap;
    lox_err_t r = lox_alarm_snapshot_save(a, &snap);
    require_(r == LOX_OK, "snapshot_save failed");

    lox_alarm_t b;
    r = lox_alarm_snapshot_load(&b, a->cfg, &snap, now_ms);
    require_(r == LOX_OK, "snapshot_load failed");

    require_(b.state == a->state, "snapshot state mismatch");
    require_(b.activation_count == a->activation_count, "snapshot activation_count mismatch");
    require_(b.shelve_count == a->shelve_count, "snapshot shelve_count mismatch");
}

int main(void) {
    /* Deterministic PRNG seed so failures are reproducible. */
    uint32_t rng = 0xC0FFEE42u;
    g_seed = rng;

    const lox_alarm_config_t cfg = {
        .on_delay_ms = 2000,
        .off_delay_ms = 5000,
        .latched = true,
        .shelvable = true,
        .max_shelve_ms = 10u * 60u * 1000u,
        .tag = "fuzzlike_alarm",
    };

    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);
    uint32_t now_ms = 0;
    bool condition = false;

    for (uint32_t i = 0; i < 200000; ++i) {
        g_iter = i;
        uint32_t r = xorshift32(&rng);
        uint32_t op = r % 10u;
        g_op = op;

        /* Mix in wrap-around windows occasionally. */
        if ((r & 0x3FFFu) == 0x1234u) {
            now_ms = 0xFFFF0000u + (r & 0xFFFFu);
        }

        switch (op) {
            case 0: /* update */
            case 1:
            case 2: {
                /* sometimes toggle condition */
                if ((r & 7u) == 0) condition = !condition;
                now_ms += (r % 200u); /* 0..199ms steps */
                (void)lox_alarm_update(&a, condition, now_ms);
                break;
            }
            case 3: { /* ack */
                now_ms += 1;
                (void)lox_alarm_ack(&a, now_ms, (uint16_t)(r & 0xFFFFu));
                break;
            }
            case 4: { /* shelve */
                now_ms += 1;
                uint32_t dur = (r % (20u * 60u * 1000u)) + 1u; /* 1..20min */
                (void)lox_alarm_shelve(&a, dur, now_ms, (uint16_t)(r & 0xFFFFu));
                break;
            }
            case 5: { /* unshelve */
                now_ms += 1;
                (void)lox_alarm_unshelve(&a, now_ms, (uint16_t)(r & 0xFFFFu));
                break;
            }
            case 6: { /* enter OOS */
                now_ms += 1;
                (void)lox_alarm_set_out_of_service(&a, true, now_ms);
                break;
            }
            case 7: { /* exit OOS */
                now_ms += 1;
                (void)lox_alarm_set_out_of_service(&a, false, now_ms);
                break;
            }
            case 8: { /* reset */
                now_ms += 1;
                (void)lox_alarm_reset(&a, now_ms);
                break;
            }
            case 9: { /* snapshot roundtrip check */
                snapshot_roundtrip_check(&a, now_ms);
                break;
            }
            default:
                break;
        }

        g_now_ms = now_ms;
        g_state = a.state;
        invariant_check(&a);
    }

    printf("OK\n");
    return 0;
}
