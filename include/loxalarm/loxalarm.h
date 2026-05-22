/*
 * loxalarm.h - Deterministic alarm-state core for embedded C firmware.
 *
 * SPDX-License-Identifier: MIT
 *
 * Header-only C99 implementation.
 *
 * Conventions:
 *   - All time values are uint32_t milliseconds from a caller-provided
 *     monotonic clock. Wrap-around is handled via unsigned subtraction.
 *   - All return values: 0 = success, negative = error (see lox_err_t).
 *   - The library never allocates. No globals. No I/O.
 */

#ifndef LOXALARM_LOXALARM_H
#define LOXALARM_LOXALARM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* Version                                                                */
/* ---------------------------------------------------------------------- */

#define LOXALARM_VERSION_MAJOR 0
#define LOXALARM_VERSION_MINOR 1
#define LOXALARM_VERSION_PATCH 0

/* ---------------------------------------------------------------------- */
/* Errors                                                                 */
/* ---------------------------------------------------------------------- */

typedef enum {
    LOX_OK              =  0,
    LOX_ERR_INVALID_ARG = -1,
    LOX_ERR_STATE       = -2,   /* operation not allowed in current state */
    LOX_ERR_DISABLED    = -3,   /* feature disabled in config             */
    LOX_ERR_OVERFLOW    = -4,   /* counter overflow                       */
} lox_err_t;

/* ---------------------------------------------------------------------- */
/* Alarm states (ISA 18.2 / OPC UA Part 9 subset)                         */
/* ---------------------------------------------------------------------- */

typedef enum {
    LOX_ALARM_NORMAL          = 0,
    LOX_ALARM_ACTIVE          = 1,  /* condition true past on-delay        */
    LOX_ALARM_LATCHED_RETURN  = 2,  /* condition cleared, waiting for ack  */
    LOX_ALARM_SHELVED         = 3,  /* operator-silenced for a duration    */
    LOX_ALARM_SUPPRESSED      = 4,  /* reserved for higher-level logic     */
    LOX_ALARM_OUT_OF_SERVICE  = 5,  /* maintenance, not evaluated          */
} lox_alarm_state_t;

/* ---------------------------------------------------------------------- */
/* Transition reason bitmask                                              */
/* ---------------------------------------------------------------------- */

typedef enum {
    LOX_REASON_NONE          = 0,
    LOX_REASON_ON_DELAY_MET  = 1u << 0,
    LOX_REASON_OFF_DELAY_MET = 1u << 1,
    LOX_REASON_ACK           = 1u << 2,
    LOX_REASON_SHELVE        = 1u << 3,
    LOX_REASON_SHELVE_EXPIRE = 1u << 4,
    LOX_REASON_OOS_ENTER     = 1u << 5,
    LOX_REASON_OOS_EXIT      = 1u << 6,
    LOX_REASON_RESET         = 1u << 7,
} lox_alarm_reason_t;

/* ---------------------------------------------------------------------- */
/* Configuration                                                          */
/* ---------------------------------------------------------------------- */

typedef struct {
    uint32_t on_delay_ms;
    uint32_t off_delay_ms;
    bool     latched;
    bool     shelvable;
    uint32_t max_shelve_ms; /* 0 = no max (not recommended) */
    const char *tag;        /* optional; pointer not copied */
} lox_alarm_config_t;

/* ---------------------------------------------------------------------- */
/* Alarm instance (caller-allocated)                                      */
/* ---------------------------------------------------------------------- */

typedef struct {
    /* --- public-readable; do not write directly --- */
    lox_alarm_state_t  state;
    uint32_t           reason_flags;       /* OR of lox_alarm_reason_t    */
    uint32_t           state_entered_ms;   /* monotonic ms of last enter  */
    uint32_t           activation_count;   /* lifetime activations        */
    uint32_t           shelve_count;       /* lifetime shelves            */
    bool               last_condition;     /* condition seen in last upd  */

    /* --- transient flags (cleared after each call to is_just_*) --- */
    bool               just_activated;
    bool               just_returned;
    bool               just_acked;
    bool               just_shelved;

    /* --- internal --- */
    const lox_alarm_config_t *cfg;
    uint32_t           cond_true_since_ms;
    uint32_t           cond_false_since_ms;
    uint32_t           shelve_expires_ms;
    uint16_t           ack_id;               /* who acked (0 = anonymous) */
    lox_alarm_state_t  shelve_resume_state;  /* ACTIVE or LATCHED_RETURN  */
    bool               initialised;
} lox_alarm_t;

/* ---------------------------------------------------------------------- */
/* Snapshot for persistence                                               */
/* ---------------------------------------------------------------------- */

typedef struct {
    uint8_t  version;          /* = 1 in this release */
    uint8_t  state;            /* lox_alarm_state_t   */
    uint16_t ack_id;
    uint32_t activation_count;
    uint32_t shelve_count;
    uint32_t shelve_expires_ms;
    uint32_t state_entered_ms;
    uint8_t  shelve_resume_state; /* lox_alarm_state_t */
    uint8_t  _reserved0;
    uint16_t _reserved1;
} lox_alarm_snapshot_t;

/* ---------------------------------------------------------------------- */
/* Internal helpers                                                       */
/* ---------------------------------------------------------------------- */

static inline bool lox_alarm__is_init(const lox_alarm_t *a) {
    return a && a->initialised && a->cfg;
}

static inline uint32_t lox_alarm__elapsed(uint32_t now_ms, uint32_t since_ms) {
    return (uint32_t)(now_ms - since_ms);
}

static inline bool lox_alarm__elapsed_at_least(uint32_t now_ms,
                                               uint32_t since_ms,
                                               uint32_t delay_ms) {
    return lox_alarm__elapsed(now_ms, since_ms) >= delay_ms;
}

static inline bool lox_alarm__time_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static inline void lox_alarm__clear_transients(lox_alarm_t *a) {
    a->just_activated = false;
    a->just_returned  = false;
    a->just_acked     = false;
    a->just_shelved   = false;
}

static inline uint32_t lox_alarm__unset_time(void) {
    return UINT32_MAX;
}

static inline void lox_alarm__enter_state(lox_alarm_t *a,
                                         lox_alarm_state_t next,
                                         uint32_t now_ms) {
    a->state = next;
    a->state_entered_ms = now_ms;
}

static inline lox_err_t lox_alarm__inc_u32(uint32_t *v) {
    if (*v == UINT32_MAX) return LOX_ERR_OVERFLOW;
    (*v)++;
    return LOX_OK;
}

/* ---------------------------------------------------------------------- */
/* Lifecycle                                                              */
/* ---------------------------------------------------------------------- */

static inline lox_err_t lox_alarm_init(lox_alarm_t *a, const lox_alarm_config_t *cfg) {
    if (!a || !cfg) return LOX_ERR_INVALID_ARG;

    *a = (lox_alarm_t){0};
    a->cfg = cfg;
    a->state = LOX_ALARM_NORMAL;
    a->shelve_resume_state = LOX_ALARM_ACTIVE;
    a->last_condition = false;
    a->cond_true_since_ms = lox_alarm__unset_time();
    a->cond_false_since_ms = lox_alarm__unset_time();
    a->initialised = true;
    return LOX_OK;
}

static inline lox_err_t lox_alarm_reset(lox_alarm_t *a, uint32_t now_ms) {
    if (!lox_alarm__is_init(a)) return LOX_ERR_INVALID_ARG;

    lox_alarm__clear_transients(a);
    a->reason_flags |= LOX_REASON_RESET;
    a->ack_id = 0;
    a->last_condition = false;
    a->cond_true_since_ms = lox_alarm__unset_time();
    a->cond_false_since_ms = lox_alarm__unset_time();
    a->shelve_expires_ms = 0;
    a->shelve_resume_state = LOX_ALARM_ACTIVE;
    lox_alarm__enter_state(a, LOX_ALARM_NORMAL, now_ms);
    return LOX_OK;
}

/* ---------------------------------------------------------------------- */
/* Update                                                                 */
/* ---------------------------------------------------------------------- */

static inline lox_err_t lox_alarm_update(lox_alarm_t *a, bool condition, uint32_t now_ms) {
    if (!lox_alarm__is_init(a)) return LOX_ERR_INVALID_ARG;

    lox_alarm__clear_transients(a);
    a->last_condition = condition;

    if (a->state == LOX_ALARM_OUT_OF_SERVICE) {
        return LOX_OK;
    }

    if (a->state == LOX_ALARM_SHELVED) {
        if (lox_alarm__time_reached(now_ms, a->shelve_expires_ms)) {
            a->reason_flags |= LOX_REASON_SHELVE_EXPIRE;
            lox_alarm__enter_state(a, a->shelve_resume_state, now_ms);
            a->shelve_expires_ms = 0;
        } else {
            /* Track condition timers even while shelved. */
            if (condition) {
                if (a->cond_true_since_ms == lox_alarm__unset_time()) a->cond_true_since_ms = now_ms;
                a->cond_false_since_ms = lox_alarm__unset_time();
            } else {
                if (a->cond_false_since_ms == lox_alarm__unset_time()) a->cond_false_since_ms = now_ms;
                a->cond_true_since_ms = lox_alarm__unset_time();
            }
            return LOX_OK;
        }
    }

    /* Condition timer bookkeeping (for states that evaluate the signal). */
    if (condition) {
        if (a->cond_true_since_ms == lox_alarm__unset_time()) a->cond_true_since_ms = now_ms;
        a->cond_false_since_ms = lox_alarm__unset_time();
    } else {
        if (a->cond_false_since_ms == lox_alarm__unset_time()) a->cond_false_since_ms = now_ms;
        a->cond_true_since_ms = lox_alarm__unset_time();
    }

    switch (a->state) {
        case LOX_ALARM_NORMAL: {
            if (condition) {
                if (a->cfg->on_delay_ms == 0 ||
                    lox_alarm__elapsed_at_least(now_ms, a->cond_true_since_ms, a->cfg->on_delay_ms)) {
                    a->reason_flags |= LOX_REASON_ON_DELAY_MET;
                    lox_err_t r = lox_alarm__inc_u32(&a->activation_count);
                    if (r != LOX_OK) return r;
                    lox_alarm__enter_state(a, LOX_ALARM_ACTIVE, now_ms);
                    a->just_activated = true;
                }
            }
            break;
        }
        case LOX_ALARM_ACTIVE: {
            if (!condition) {
                if (a->cfg->off_delay_ms == 0 ||
                    lox_alarm__elapsed_at_least(now_ms, a->cond_false_since_ms, a->cfg->off_delay_ms)) {
                    a->reason_flags |= LOX_REASON_OFF_DELAY_MET;
                    a->just_returned = true;
                    if (a->cfg->latched) {
                        lox_alarm__enter_state(a, LOX_ALARM_LATCHED_RETURN, now_ms);
                    } else {
                        lox_alarm__enter_state(a, LOX_ALARM_NORMAL, now_ms);
                    }
                }
            }
            break;
        }
        case LOX_ALARM_LATCHED_RETURN: {
            if (condition) {
                if (a->cfg->on_delay_ms == 0 ||
                    lox_alarm__elapsed_at_least(now_ms, a->cond_true_since_ms, a->cfg->on_delay_ms)) {
                    a->reason_flags |= LOX_REASON_ON_DELAY_MET;
                    lox_err_t r = lox_alarm__inc_u32(&a->activation_count);
                    if (r != LOX_OK) return r;
                    lox_alarm__enter_state(a, LOX_ALARM_ACTIVE, now_ms);
                    a->just_activated = true;
                }
            }
            break;
        }
        case LOX_ALARM_SUPPRESSED:
            /* v0.1: reserved; higher level can manage suppression policy. */
            break;
        case LOX_ALARM_SHELVED:
        case LOX_ALARM_OUT_OF_SERVICE:
        default:
            break;
    }

    return LOX_OK;
}

/* ---------------------------------------------------------------------- */
/* Operator actions                                                       */
/* ---------------------------------------------------------------------- */

static inline lox_err_t lox_alarm_ack(lox_alarm_t *a, uint32_t now_ms, uint16_t op_id) {
    if (!lox_alarm__is_init(a)) return LOX_ERR_INVALID_ARG;
    if (a->state != LOX_ALARM_LATCHED_RETURN) return LOX_ERR_STATE;

    lox_alarm__clear_transients(a);
    a->ack_id = op_id;
    a->reason_flags |= LOX_REASON_ACK;
    a->just_acked = true;
    a->cond_true_since_ms = lox_alarm__unset_time();
    a->cond_false_since_ms = lox_alarm__unset_time();
    lox_alarm__enter_state(a, LOX_ALARM_NORMAL, now_ms);
    return LOX_OK;
}

static inline lox_err_t lox_alarm_shelve(lox_alarm_t *a,
                                        uint32_t duration_ms,
                                        uint32_t now_ms,
                                        uint16_t op_id) {
    (void)op_id;
    if (!lox_alarm__is_init(a)) return LOX_ERR_INVALID_ARG;
    if (!a->cfg->shelvable) return LOX_ERR_DISABLED;
    if (!(a->state == LOX_ALARM_ACTIVE || a->state == LOX_ALARM_LATCHED_RETURN)) {
        return LOX_ERR_STATE;
    }
    if (duration_ms == 0) return LOX_ERR_INVALID_ARG;

    uint32_t capped = duration_ms;
    if (a->cfg->max_shelve_ms != 0 && capped > a->cfg->max_shelve_ms) {
        capped = a->cfg->max_shelve_ms;
    }

    lox_err_t r = lox_alarm__inc_u32(&a->shelve_count);
    if (r != LOX_OK) return r;

    lox_alarm__clear_transients(a);
    a->reason_flags |= LOX_REASON_SHELVE;
    a->just_shelved = true;
    a->shelve_resume_state = a->state;
    a->shelve_expires_ms = now_ms + capped;
    lox_alarm__enter_state(a, LOX_ALARM_SHELVED, now_ms);
    return LOX_OK;
}

static inline lox_err_t lox_alarm_unshelve(lox_alarm_t *a, uint32_t now_ms) {
    if (!lox_alarm__is_init(a)) return LOX_ERR_INVALID_ARG;
    if (a->state != LOX_ALARM_SHELVED) return LOX_ERR_STATE;

    lox_alarm__clear_transients(a);
    lox_alarm__enter_state(a, a->shelve_resume_state, now_ms);
    a->shelve_expires_ms = 0;
    return LOX_OK;
}

static inline lox_err_t lox_alarm_set_out_of_service(lox_alarm_t *a, bool oos, uint32_t now_ms) {
    if (!lox_alarm__is_init(a)) return LOX_ERR_INVALID_ARG;

    lox_alarm__clear_transients(a);
    if (oos) {
        if (a->state == LOX_ALARM_OUT_OF_SERVICE) return LOX_OK;
        a->reason_flags |= LOX_REASON_OOS_ENTER;
        lox_alarm__enter_state(a, LOX_ALARM_OUT_OF_SERVICE, now_ms);
        return LOX_OK;
    }

    if (a->state != LOX_ALARM_OUT_OF_SERVICE) return LOX_ERR_STATE;
    a->reason_flags |= LOX_REASON_OOS_EXIT;
    a->ack_id = 0;
    a->last_condition = false;
    a->cond_true_since_ms = lox_alarm__unset_time();
    a->cond_false_since_ms = lox_alarm__unset_time();
    a->shelve_expires_ms = 0;
    a->shelve_resume_state = LOX_ALARM_ACTIVE;
    lox_alarm__enter_state(a, LOX_ALARM_NORMAL, now_ms);
    return LOX_OK;
}

/* ---------------------------------------------------------------------- */
/* Query                                                                  */
/* ---------------------------------------------------------------------- */

static inline bool lox_alarm_is_active(const lox_alarm_t *a) {
    return lox_alarm__is_init(a) && a->state == LOX_ALARM_ACTIVE;
}

static inline bool lox_alarm_needs_attention(const lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) return false;
    if (a->state == LOX_ALARM_SHELVED) return false;
    if (a->state == LOX_ALARM_SUPPRESSED) return false;
    if (a->state == LOX_ALARM_OUT_OF_SERVICE) return false;
    return a->state == LOX_ALARM_ACTIVE || a->state == LOX_ALARM_LATCHED_RETURN;
}

static inline bool lox_alarm_just_activated(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) return false;
    bool v = a->just_activated;
    a->just_activated = false;
    return v;
}

static inline bool lox_alarm_just_returned(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) return false;
    bool v = a->just_returned;
    a->just_returned = false;
    return v;
}

static inline bool lox_alarm_just_acked(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) return false;
    bool v = a->just_acked;
    a->just_acked = false;
    return v;
}

static inline bool lox_alarm_just_shelved(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) return false;
    bool v = a->just_shelved;
    a->just_shelved = false;
    return v;
}

static inline lox_alarm_state_t lox_alarm_state(const lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) return LOX_ALARM_NORMAL;
    return a->state;
}

static inline uint32_t lox_alarm_drain_reason_flags(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) return 0;
    uint32_t v = a->reason_flags;
    a->reason_flags = 0;
    return v;
}

/* ---------------------------------------------------------------------- */
/* Snapshot for persistence                                               */
/* ---------------------------------------------------------------------- */

static inline lox_err_t lox_alarm_snapshot_save(const lox_alarm_t *a, lox_alarm_snapshot_t *out) {
    if (!lox_alarm__is_init(a) || !out) return LOX_ERR_INVALID_ARG;
    *out = (lox_alarm_snapshot_t){
        .version = 1,
        .state = (uint8_t)a->state,
        .ack_id = a->ack_id,
        .activation_count = a->activation_count,
        .shelve_count = a->shelve_count,
        .shelve_expires_ms = a->shelve_expires_ms,
        .state_entered_ms = a->state_entered_ms,
        .shelve_resume_state = (uint8_t)a->shelve_resume_state,
    };
    return LOX_OK;
}

static inline lox_err_t lox_alarm_snapshot_load(lox_alarm_t *a,
                                               const lox_alarm_config_t *cfg,
                                               const lox_alarm_snapshot_t *snap,
                                               uint32_t now_ms) {
    if (!a || !cfg || !snap) return LOX_ERR_INVALID_ARG;
    if (snap->version != 1) return LOX_ERR_INVALID_ARG;

    *a = (lox_alarm_t){0};
    a->cfg = cfg;
    a->initialised = true;
    a->state = (lox_alarm_state_t)snap->state;
    a->ack_id = snap->ack_id;
    a->activation_count = snap->activation_count;
    a->shelve_count = snap->shelve_count;
    a->state_entered_ms = snap->state_entered_ms;
    a->shelve_expires_ms = snap->shelve_expires_ms;
    a->shelve_resume_state = (lox_alarm_state_t)snap->shelve_resume_state;

    /* On restore, the condition is unknown until the caller starts updating. */
    a->last_condition = false;
    a->cond_true_since_ms = lox_alarm__unset_time();
    a->cond_false_since_ms = lox_alarm__unset_time();
    lox_alarm__clear_transients(a);

    /* If snapshot says shelved but the duration already elapsed relative to now_ms, resume immediately. */
    if (a->state == LOX_ALARM_SHELVED && a->shelve_expires_ms != 0 &&
        lox_alarm__time_reached(now_ms, a->shelve_expires_ms)) {
        a->reason_flags |= LOX_REASON_SHELVE_EXPIRE;
        a->state = a->shelve_resume_state;
        a->shelve_expires_ms = 0;
        a->state_entered_ms = now_ms;
    }

    return LOX_OK;
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* LOXALARM_LOXALARM_H */
