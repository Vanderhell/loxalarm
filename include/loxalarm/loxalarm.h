/*
 * loxalarm.h - Deterministic alarm-state core for embedded C firmware.
 *
 * SPDX-License-Identifier: MIT
 *
 * Header-only C99 implementation.
 */

#ifndef LOXALARM_LOXALARM_H
#define LOXALARM_LOXALARM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------- */
/* Version                                                                */
/* ---------------------------------------------------------------------- */

#define LOXALARM_VERSION_MAJOR 0
#define LOXALARM_VERSION_MINOR 1
#define LOXALARM_VERSION_PATCH 2

#define LOXALARM_MAX_DELAY_MS UINT32_C(0x7fffffff)
#define LOXALARM_SNAPSHOT_MAGIC UINT32_C(0x41584f4c) /* "LOXA" */
#define LOXALARM_SNAPSHOT_WIRE_VERSION UINT8_C(1)
#define LOXALARM_SNAPSHOT_WIRE_SIZE ((size_t)72u)

/* ---------------------------------------------------------------------- */
/* Errors                                                                 */
/* ---------------------------------------------------------------------- */

typedef enum {
    LOXALARM_OK = 0,
    LOXALARM_ERR_INVALID_ARG = -1,
    LOXALARM_ERR_STATE = -2,
    LOXALARM_ERR_DISABLED = -3,
    LOXALARM_ERR_CLOCK = -4,
    LOXALARM_ERR_INVALID_SNAPSHOT = -5,
    LOXALARM_ERR_SATURATED = -6,
} lox_alarm_status_t;

typedef lox_alarm_status_t lox_err_t;

#define LOX_OK LOXALARM_OK
#define LOX_ERR_INVALID_ARG LOXALARM_ERR_INVALID_ARG
#define LOX_ERR_STATE LOXALARM_ERR_STATE
#define LOX_ERR_DISABLED LOXALARM_ERR_DISABLED
#define LOX_ERR_OVERFLOW LOXALARM_ERR_SATURATED

/* ---------------------------------------------------------------------- */
/* Alarm states (ISA 18.2 / OPC UA Part 9 subset)                         */
/* ---------------------------------------------------------------------- */

typedef enum {
    LOX_ALARM_NORMAL = 0,
    LOX_ALARM_ACTIVE = 1,
    LOX_ALARM_LATCHED_RETURN = 2,
    LOX_ALARM_SHELVED = 3,
    LOX_ALARM_SUPPRESSED = 4,
    LOX_ALARM_OUT_OF_SERVICE = 5,
} lox_alarm_state_t;

/* ---------------------------------------------------------------------- */
/* Transition reason bitmask                                              */
/* ---------------------------------------------------------------------- */

typedef enum {
    LOX_REASON_NONE = 0,
    LOX_REASON_ON_DELAY_MET = 1u << 0,
    LOX_REASON_OFF_DELAY_MET = 1u << 1,
    LOX_REASON_ACK = 1u << 2,
    LOX_REASON_SHELVE = 1u << 3,
    LOX_REASON_SHELVE_EXPIRE = 1u << 4,
    LOX_REASON_OOS_ENTER = 1u << 5,
    LOX_REASON_OOS_EXIT = 1u << 6,
    LOX_REASON_RESET = 1u << 7,
    LOX_REASON_UNSHELVE = 1u << 8,
} lox_alarm_reason_t;

/* ---------------------------------------------------------------------- */
/* Configuration                                                          */
/* ---------------------------------------------------------------------- */

typedef struct {
    uint32_t on_delay_ms;
    uint32_t off_delay_ms;
    bool latched;
    bool shelvable;
    uint32_t max_shelve_ms;
    const char *tag;
} lox_alarm_config_t;

/* ---------------------------------------------------------------------- */
/* In-memory snapshot                                                     */
/* ---------------------------------------------------------------------- */

typedef struct {
    uint32_t schema_id;
    uint8_t flags;
    uint8_t state;
    uint8_t shelve_resume_state;
    uint8_t _reserved0;
    uint32_t activation_count;
    uint32_t reactivation_count;
    uint32_t ack_count;
    uint32_t shelve_count;
    uint32_t ack_id;
    uint32_t shelve_op_id;
    uint32_t unshelve_op_id;
    uint32_t state_entered_ms;
    uint32_t cond_true_since_ms;
    uint32_t cond_false_since_ms;
    uint32_t shelve_expires_ms;
    uint32_t _reserved1;
    uint32_t _reserved2;
} lox_alarm_snapshot_t;

/* ---------------------------------------------------------------------- */
/* Alarm instance                                                         */
/* ---------------------------------------------------------------------- */

typedef struct {
    lox_alarm_state_t state;
    uint32_t reason_flags;
    uint32_t state_entered_ms;
    uint32_t activation_count;
    uint32_t reactivation_count;
    uint32_t ack_count;
    uint32_t shelve_count;
    uint16_t ack_id;
    uint16_t shelve_op_id;
    uint16_t unshelve_op_id;
    bool last_condition;
    bool just_activated;
    bool just_returned;
    bool just_acked;
    bool just_shelved;
    const lox_alarm_config_t *cfg;
    uint32_t cond_true_since_ms;
    uint32_t cond_false_since_ms;
    uint32_t shelve_expires_ms;
    lox_alarm_state_t shelve_resume_state;
    bool cond_true_armed;
    bool cond_false_armed;
    bool shelve_armed;
    bool clock_valid;
    uint32_t last_clock_ms;
    bool initialised;
} lox_alarm_t;

/* ---------------------------------------------------------------------- */
/* Internal helpers                                                       */
/* ---------------------------------------------------------------------- */

static inline bool lox_alarm__is_init(const lox_alarm_t *a) {
    return a != NULL && a->initialised && a->cfg != NULL;
}

static inline bool lox_alarm__is_valid_state_u8(uint8_t v) {
    return v == (uint8_t)LOX_ALARM_NORMAL ||
           v == (uint8_t)LOX_ALARM_ACTIVE ||
           v == (uint8_t)LOX_ALARM_LATCHED_RETURN ||
           v == (uint8_t)LOX_ALARM_SHELVED ||
           v == (uint8_t)LOX_ALARM_SUPPRESSED ||
           v == (uint8_t)LOX_ALARM_OUT_OF_SERVICE;
}

static inline bool lox_alarm__is_supported_state_u8(uint8_t v) {
    return v == (uint8_t)LOX_ALARM_NORMAL ||
           v == (uint8_t)LOX_ALARM_ACTIVE ||
           v == (uint8_t)LOX_ALARM_LATCHED_RETURN ||
           v == (uint8_t)LOX_ALARM_SHELVED ||
           v == (uint8_t)LOX_ALARM_OUT_OF_SERVICE;
}

static inline uint32_t lox_alarm__elapsed(uint32_t now_ms, uint32_t since_ms) {
    return (uint32_t)(now_ms - since_ms);
}

static inline bool lox_alarm__elapsed_at_least(uint32_t now_ms,
                                               uint32_t since_ms,
                                               uint32_t delay_ms) {
    return lox_alarm__elapsed(now_ms, since_ms) >= delay_ms;
}

static inline bool lox_alarm__time_advance_ok(uint32_t prev_ms, uint32_t now_ms) {
    return (uint32_t)(now_ms - prev_ms) <= LOXALARM_MAX_DELAY_MS;
}

static inline bool lox_alarm__deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static inline void lox_alarm__clear_transients(lox_alarm_t *a) {
    a->just_activated = false;
    a->just_returned = false;
    a->just_acked = false;
    a->just_shelved = false;
}

static inline void lox_alarm__clear_timer_arms(lox_alarm_t *a) {
    a->cond_true_armed = false;
    a->cond_false_armed = false;
    a->shelve_armed = false;
}

static inline void lox_alarm__arm_condition_timer(lox_alarm_t *a, bool condition, uint32_t now_ms) {
    if (condition) {
        if (!a->cond_true_armed) {
            a->cond_true_armed = true;
            a->cond_true_since_ms = now_ms;
        }
        a->cond_false_armed = false;
    } else {
        if (!a->cond_false_armed) {
            a->cond_false_armed = true;
            a->cond_false_since_ms = now_ms;
        }
        a->cond_true_armed = false;
    }
}

static inline lox_alarm_status_t lox_alarm__check_clock(lox_alarm_t *a, uint32_t now_ms) {
    if (a->clock_valid && !lox_alarm__time_advance_ok(a->last_clock_ms, now_ms)) {
        return LOXALARM_ERR_CLOCK;
    }
    a->last_clock_ms = now_ms;
    a->clock_valid = true;
    return LOXALARM_OK;
}

static inline lox_alarm_status_t lox_alarm__validate_config(const lox_alarm_config_t *cfg) {
    if (cfg == NULL) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    if (cfg->on_delay_ms > LOXALARM_MAX_DELAY_MS ||
        cfg->off_delay_ms > LOXALARM_MAX_DELAY_MS) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    if (cfg->shelvable) {
        if (cfg->max_shelve_ms == 0 || cfg->max_shelve_ms > LOXALARM_MAX_DELAY_MS) {
            return LOXALARM_ERR_INVALID_ARG;
        }
    } else if (cfg->max_shelve_ms != 0) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    return LOXALARM_OK;
}

static inline void lox_alarm__enter_state(lox_alarm_t *a, lox_alarm_state_t next, uint32_t now_ms) {
    a->state = next;
    a->state_entered_ms = now_ms;
}

static inline void lox_alarm__sat_inc_u32(uint32_t *v) {
    if (*v != UINT32_MAX) {
        ++(*v);
    }
}

static inline void lox_alarm__snapshot_copy_from_alarm(const lox_alarm_t *a, lox_alarm_snapshot_t *out) {
    out->schema_id = 0;
    out->flags = 0;
    if (a->cond_true_armed) {
        out->flags |= 1u << 0;
    }
    if (a->cond_false_armed) {
        out->flags |= 1u << 1;
    }
    if (a->shelve_armed) {
        out->flags |= 1u << 2;
    }
    out->state = (uint8_t)a->state;
    out->shelve_resume_state = (uint8_t)a->shelve_resume_state;
    out->_reserved0 = 0;
    out->activation_count = a->activation_count;
    out->reactivation_count = a->reactivation_count;
    out->ack_count = a->ack_count;
    out->shelve_count = a->shelve_count;
    out->ack_id = a->ack_id;
    out->shelve_op_id = a->shelve_op_id;
    out->unshelve_op_id = a->unshelve_op_id;
    out->state_entered_ms = a->state_entered_ms;
    out->cond_true_since_ms = a->cond_true_since_ms;
    out->cond_false_since_ms = a->cond_false_since_ms;
    out->shelve_expires_ms = a->shelve_expires_ms;
    out->_reserved1 = 0;
    out->_reserved2 = 0;
}

static inline lox_alarm_status_t lox_alarm__snapshot_validate(const lox_alarm_config_t *cfg,
                                                              const lox_alarm_snapshot_t *snap) {
    if (cfg == NULL || snap == NULL) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    if (!lox_alarm__is_supported_state_u8(snap->state)) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if (!lox_alarm__is_valid_state_u8(snap->shelve_resume_state)) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if ((snap->flags & ~(1u << 0 | 1u << 1 | 1u << 2)) != 0u) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if (snap->_reserved0 != 0 || snap->_reserved1 != 0 || snap->_reserved2 != 0) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if (snap->state == (uint8_t)LOX_ALARM_SHELVED) {
        if (!cfg->shelvable) {
            return LOXALARM_ERR_INVALID_SNAPSHOT;
        }
        if (snap->shelve_resume_state != (uint8_t)LOX_ALARM_ACTIVE &&
            snap->shelve_resume_state != (uint8_t)LOX_ALARM_LATCHED_RETURN) {
            return LOXALARM_ERR_INVALID_SNAPSHOT;
        }
        if ((snap->flags & (1u << 2)) == 0u || snap->shelve_expires_ms == 0u) {
            return LOXALARM_ERR_INVALID_SNAPSHOT;
        }
    } else if ((snap->flags & (1u << 2)) != 0u || snap->shelve_expires_ms != 0u) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if (snap->state == (uint8_t)LOX_ALARM_LATCHED_RETURN && !cfg->latched) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    return LOXALARM_OK;
}

/* ---------------------------------------------------------------------- */
/* Lifecycle                                                              */
/* ---------------------------------------------------------------------- */

static inline lox_alarm_status_t lox_alarm_init(lox_alarm_t *a, const lox_alarm_config_t *cfg) {
    lox_alarm_status_t r = lox_alarm__validate_config(cfg);
    if (a == NULL || r != LOXALARM_OK) {
        return r;
    }

    *a = (lox_alarm_t){0};
    a->cfg = cfg;
    a->state = LOX_ALARM_NORMAL;
    a->shelve_resume_state = LOX_ALARM_ACTIVE;
    a->initialised = true;
    return LOXALARM_OK;
}

static inline lox_alarm_status_t lox_alarm_reset(lox_alarm_t *a, uint32_t now_ms) {
    if (!lox_alarm__is_init(a)) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    lox_alarm_status_t r = lox_alarm__check_clock(a, now_ms);
    if (r != LOXALARM_OK) {
        return r;
    }

    lox_alarm__clear_transients(a);
    a->reason_flags |= LOX_REASON_RESET;
    a->ack_id = 0;
    a->shelve_op_id = 0;
    a->unshelve_op_id = 0;
    a->last_condition = false;
    a->cond_true_since_ms = 0;
    a->cond_false_since_ms = 0;
    lox_alarm__clear_timer_arms(a);
    a->shelve_expires_ms = 0;
    a->shelve_resume_state = LOX_ALARM_ACTIVE;
    lox_alarm__enter_state(a, LOX_ALARM_NORMAL, now_ms);
    return LOXALARM_OK;
}

static inline lox_alarm_status_t lox_alarm_force_reset(lox_alarm_t *a, uint32_t now_ms) {
    return lox_alarm_reset(a, now_ms);
}

/* ---------------------------------------------------------------------- */
/* Update                                                                 */
/* ---------------------------------------------------------------------- */

static inline lox_alarm_status_t lox_alarm_update(lox_alarm_t *a, bool condition, uint32_t now_ms) {
    if (!lox_alarm__is_init(a)) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    lox_alarm_status_t r = lox_alarm__check_clock(a, now_ms);
    if (r != LOXALARM_OK) {
        return r;
    }

    lox_alarm__clear_transients(a);
    a->last_condition = condition;

    if (a->state == LOX_ALARM_OUT_OF_SERVICE) {
        return LOXALARM_OK;
    }

    if (a->state == LOX_ALARM_SHELVED) {
        if (a->shelve_armed && lox_alarm__deadline_reached(now_ms, a->shelve_expires_ms)) {
            a->reason_flags |= LOX_REASON_SHELVE_EXPIRE;
            lox_alarm__enter_state(a, a->shelve_resume_state, now_ms);
            a->shelve_armed = false;
            a->shelve_expires_ms = 0;
        } else {
            lox_alarm__arm_condition_timer(a, condition, now_ms);
            return LOXALARM_OK;
        }
    }

    lox_alarm__arm_condition_timer(a, condition, now_ms);

    switch (a->state) {
        case LOX_ALARM_NORMAL:
            if (condition &&
                (a->cfg->on_delay_ms == 0 ||
                 (a->cond_true_armed &&
                  lox_alarm__elapsed_at_least(now_ms, a->cond_true_since_ms, a->cfg->on_delay_ms)))) {
                a->reason_flags |= LOX_REASON_ON_DELAY_MET;
                lox_alarm__sat_inc_u32(&a->activation_count);
                lox_alarm__enter_state(a, LOX_ALARM_ACTIVE, now_ms);
                a->just_activated = true;
            }
            break;
        case LOX_ALARM_ACTIVE:
            if (!condition &&
                (a->cfg->off_delay_ms == 0 ||
                 (a->cond_false_armed &&
                  lox_alarm__elapsed_at_least(now_ms, a->cond_false_since_ms, a->cfg->off_delay_ms)))) {
                a->reason_flags |= LOX_REASON_OFF_DELAY_MET;
                a->just_returned = true;
                if (a->cfg->latched) {
                    lox_alarm__enter_state(a, LOX_ALARM_LATCHED_RETURN, now_ms);
                } else {
                    lox_alarm__enter_state(a, LOX_ALARM_NORMAL, now_ms);
                }
            }
            break;
        case LOX_ALARM_LATCHED_RETURN:
            if (condition &&
                (a->cfg->on_delay_ms == 0 ||
                 (a->cond_true_armed &&
                  lox_alarm__elapsed_at_least(now_ms, a->cond_true_since_ms, a->cfg->on_delay_ms)))) {
                a->reason_flags |= LOX_REASON_ON_DELAY_MET;
                lox_alarm__sat_inc_u32(&a->activation_count);
                lox_alarm__sat_inc_u32(&a->reactivation_count);
                lox_alarm__enter_state(a, LOX_ALARM_ACTIVE, now_ms);
                a->just_activated = true;
            }
            break;
        case LOX_ALARM_SUPPRESSED:
            return LOXALARM_ERR_INVALID_SNAPSHOT;
        case LOX_ALARM_SHELVED:
        case LOX_ALARM_OUT_OF_SERVICE:
        default:
            break;
    }

    return LOXALARM_OK;
}

/* ---------------------------------------------------------------------- */
/* Operator actions                                                       */
/* ---------------------------------------------------------------------- */

static inline lox_alarm_status_t lox_alarm_ack(lox_alarm_t *a, uint32_t now_ms, uint16_t op_id) {
    if (!lox_alarm__is_init(a)) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    lox_alarm_status_t r = lox_alarm__check_clock(a, now_ms);
    if (r != LOXALARM_OK) {
        return r;
    }
    if (a->state != LOX_ALARM_LATCHED_RETURN) {
        return LOXALARM_ERR_STATE;
    }

    lox_alarm__clear_transients(a);
    a->ack_id = op_id;
    a->ack_count++;
    a->reason_flags |= LOX_REASON_ACK;
    a->just_acked = true;
    a->cond_true_since_ms = 0;
    a->cond_false_since_ms = 0;
    a->cond_true_armed = false;
    a->cond_false_armed = false;
    lox_alarm__enter_state(a, LOX_ALARM_NORMAL, now_ms);
    return LOXALARM_OK;
}

static inline lox_alarm_status_t lox_alarm_shelve(lox_alarm_t *a,
                                                   uint32_t duration_ms,
                                                   uint32_t now_ms,
                                                   uint16_t op_id) {
    if (!lox_alarm__is_init(a)) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    lox_alarm_status_t r = lox_alarm__check_clock(a, now_ms);
    if (r != LOXALARM_OK) {
        return r;
    }
    if (!a->cfg->shelvable) {
        return LOXALARM_ERR_DISABLED;
    }
    if (!(a->state == LOX_ALARM_ACTIVE || a->state == LOX_ALARM_LATCHED_RETURN)) {
        return LOXALARM_ERR_STATE;
    }
    if (duration_ms == 0 || duration_ms > a->cfg->max_shelve_ms || duration_ms > LOXALARM_MAX_DELAY_MS) {
        return LOXALARM_ERR_INVALID_ARG;
    }

    lox_alarm__clear_transients(a);
    a->reason_flags |= LOX_REASON_SHELVE;
    a->just_shelved = true;
    a->shelve_resume_state = a->state;
    a->shelve_op_id = op_id;
    a->shelve_armed = true;
    a->shelve_expires_ms = (uint32_t)(now_ms + duration_ms);
    lox_alarm__sat_inc_u32(&a->shelve_count);
    lox_alarm__enter_state(a, LOX_ALARM_SHELVED, now_ms);
    return LOXALARM_OK;
}

static inline lox_alarm_status_t lox_alarm_unshelve(lox_alarm_t *a, uint32_t now_ms, uint16_t op_id) {
    if (!lox_alarm__is_init(a)) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    lox_alarm_status_t r = lox_alarm__check_clock(a, now_ms);
    if (r != LOXALARM_OK) {
        return r;
    }
    if (a->state != LOX_ALARM_SHELVED) {
        return LOXALARM_ERR_STATE;
    }

    lox_alarm__clear_transients(a);
    a->unshelve_op_id = op_id;
    a->reason_flags |= LOX_REASON_UNSHELVE;
    a->shelve_armed = false;
    a->shelve_expires_ms = 0;
    lox_alarm__enter_state(a, a->shelve_resume_state, now_ms);
    return LOXALARM_OK;
}

static inline lox_alarm_status_t lox_alarm_set_out_of_service(lox_alarm_t *a, bool oos, uint32_t now_ms) {
    if (!lox_alarm__is_init(a)) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    lox_alarm_status_t r = lox_alarm__check_clock(a, now_ms);
    if (r != LOXALARM_OK) {
        return r;
    }

    lox_alarm__clear_transients(a);
    if (oos) {
        if (a->state == LOX_ALARM_OUT_OF_SERVICE) {
            return LOXALARM_OK;
        }
        a->reason_flags |= LOX_REASON_OOS_ENTER;
        a->shelve_armed = false;
        a->shelve_expires_ms = 0;
        a->shelve_resume_state = LOX_ALARM_ACTIVE;
        lox_alarm__enter_state(a, LOX_ALARM_OUT_OF_SERVICE, now_ms);
        return LOXALARM_OK;
    }

    if (a->state != LOX_ALARM_OUT_OF_SERVICE) {
        return LOXALARM_ERR_STATE;
    }
    a->reason_flags |= LOX_REASON_OOS_EXIT;
    a->last_condition = false;
    a->cond_true_since_ms = 0;
    a->cond_false_since_ms = 0;
    a->cond_true_armed = false;
    a->cond_false_armed = false;
    lox_alarm__enter_state(a, LOX_ALARM_NORMAL, now_ms);
    return LOXALARM_OK;
}

/* ---------------------------------------------------------------------- */
/* Query                                                                  */
/* ---------------------------------------------------------------------- */

static inline bool lox_alarm_is_active(const lox_alarm_t *a) {
    return lox_alarm__is_init(a) && a->state == LOX_ALARM_ACTIVE;
}

static inline bool lox_alarm_needs_attention(const lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) {
        return false;
    }
    return a->state == LOX_ALARM_ACTIVE || a->state == LOX_ALARM_LATCHED_RETURN;
}

static inline bool lox_alarm_just_activated(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) {
        return false;
    }
    bool v = a->just_activated;
    a->just_activated = false;
    return v;
}

static inline bool lox_alarm_just_returned(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) {
        return false;
    }
    bool v = a->just_returned;
    a->just_returned = false;
    return v;
}

static inline bool lox_alarm_just_acked(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) {
        return false;
    }
    bool v = a->just_acked;
    a->just_acked = false;
    return v;
}

static inline bool lox_alarm_just_shelved(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) {
        return false;
    }
    bool v = a->just_shelved;
    a->just_shelved = false;
    return v;
}

static inline lox_alarm_state_t lox_alarm_state(const lox_alarm_t *a) {
    return lox_alarm__is_init(a) ? a->state : LOX_ALARM_NORMAL;
}

static inline uint32_t lox_alarm_drain_reason_flags(lox_alarm_t *a) {
    if (!lox_alarm__is_init(a)) {
        return 0;
    }
    uint32_t v = a->reason_flags;
    a->reason_flags = 0;
    return v;
}

/* ---------------------------------------------------------------------- */
/* Snapshot                                                               */
/* ---------------------------------------------------------------------- */

static inline lox_alarm_status_t lox_alarm_snapshot_save(const lox_alarm_t *a, lox_alarm_snapshot_t *out) {
    if (!lox_alarm__is_init(a) || out == NULL) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    lox_alarm__snapshot_copy_from_alarm(a, out);
    return LOXALARM_OK;
}

static inline lox_alarm_status_t lox_alarm_snapshot_load(lox_alarm_t *a,
                                                         const lox_alarm_config_t *cfg,
                                                         const lox_alarm_snapshot_t *snap,
                                                         uint32_t now_ms) {
    if (a == NULL || cfg == NULL || snap == NULL) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    lox_alarm_status_t r = lox_alarm__validate_config(cfg);
    if (r != LOXALARM_OK) {
        return r;
    }
    r = lox_alarm__snapshot_validate(cfg, snap);
    if (r != LOXALARM_OK) {
        return r;
    }

    *a = (lox_alarm_t){0};
    a->cfg = cfg;
    a->initialised = true;
    a->state = (lox_alarm_state_t)snap->state;
    a->shelve_resume_state = (lox_alarm_state_t)snap->shelve_resume_state;
    a->activation_count = snap->activation_count;
    a->reactivation_count = snap->reactivation_count;
    a->ack_count = snap->ack_count;
    a->shelve_count = snap->shelve_count;
    a->ack_id = (uint16_t)snap->ack_id;
    a->shelve_op_id = (uint16_t)snap->shelve_op_id;
    a->unshelve_op_id = (uint16_t)snap->unshelve_op_id;
    a->state_entered_ms = snap->state_entered_ms;
    a->cond_true_since_ms = snap->cond_true_since_ms;
    a->cond_false_since_ms = snap->cond_false_since_ms;
    a->shelve_expires_ms = snap->shelve_expires_ms;
    a->cond_true_armed = (snap->flags & (1u << 0)) != 0u;
    a->cond_false_armed = (snap->flags & (1u << 1)) != 0u;
    a->shelve_armed = (snap->flags & (1u << 2)) != 0u;
    a->last_condition = false;
    a->clock_valid = false;
    a->last_clock_ms = now_ms;
    lox_alarm__clear_transients(a);

    if (a->state == LOX_ALARM_SHELVED && a->shelve_armed &&
        lox_alarm__deadline_reached(now_ms, a->shelve_expires_ms)) {
        a->reason_flags |= LOX_REASON_SHELVE_EXPIRE;
        a->state = a->shelve_resume_state;
        a->shelve_armed = false;
        a->shelve_expires_ms = 0;
        a->state_entered_ms = now_ms;
    }

    return LOXALARM_OK;
}

/* ---------------------------------------------------------------------- */
/* Portable bytes                                                         */
/* ---------------------------------------------------------------------- */

static inline size_t lox_alarm_snapshot_wire_size(void) {
    return LOXALARM_SNAPSHOT_WIRE_SIZE;
}

static inline void lox_alarm__write_u32_le(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t)(v & 0xffu);
    dst[1] = (uint8_t)((v >> 8) & 0xffu);
    dst[2] = (uint8_t)((v >> 16) & 0xffu);
    dst[3] = (uint8_t)((v >> 24) & 0xffu);
}

static inline uint32_t lox_alarm__read_u32_le(const uint8_t *src) {
    return ((uint32_t)src[0]) |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static inline lox_alarm_status_t lox_alarm_snapshot_encode(const lox_alarm_snapshot_t *snap,
                                                           uint32_t schema_id,
                                                           uint8_t *out,
                                                           size_t out_size,
                                                           size_t *written) {
    if (snap == NULL || out == NULL) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    if (out_size < LOXALARM_SNAPSHOT_WIRE_SIZE) {
        return LOXALARM_ERR_INVALID_ARG;
    }

    const uint8_t flags = snap->flags & (uint8_t)((1u << 0) | (1u << 1) | (1u << 2));
    if ((snap->flags & ~flags) != 0u) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if (!lox_alarm__is_supported_state_u8(snap->state) ||
        !lox_alarm__is_valid_state_u8(snap->shelve_resume_state)) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }

    uint8_t *p = out;
    lox_alarm__write_u32_le(p, LOXALARM_SNAPSHOT_MAGIC);
    p += 4;
    *p++ = LOXALARM_SNAPSHOT_WIRE_VERSION;
    *p++ = flags;
    *p++ = 0;
    *p++ = 0;
    lox_alarm__write_u32_le(p, (uint32_t)LOXALARM_SNAPSHOT_WIRE_SIZE);
    p += 4;
    lox_alarm__write_u32_le(p, schema_id);
    p += 4;
    *p++ = snap->state;
    *p++ = snap->shelve_resume_state;
    *p++ = 0;
    *p++ = 0;
    lox_alarm__write_u32_le(p, snap->ack_id); p += 4;
    lox_alarm__write_u32_le(p, snap->shelve_op_id); p += 4;
    lox_alarm__write_u32_le(p, snap->unshelve_op_id); p += 4;
    lox_alarm__write_u32_le(p, snap->activation_count); p += 4;
    lox_alarm__write_u32_le(p, snap->reactivation_count); p += 4;
    lox_alarm__write_u32_le(p, snap->ack_count); p += 4;
    lox_alarm__write_u32_le(p, snap->shelve_count); p += 4;
    lox_alarm__write_u32_le(p, snap->state_entered_ms); p += 4;
    lox_alarm__write_u32_le(p, snap->cond_true_since_ms); p += 4;
    lox_alarm__write_u32_le(p, snap->cond_false_since_ms); p += 4;
    lox_alarm__write_u32_le(p, snap->shelve_expires_ms); p += 4;
    lox_alarm__write_u32_le(p, snap->_reserved1); p += 4;
    lox_alarm__write_u32_le(p, snap->_reserved2); p += 4;

    if (written != NULL) {
        *written = LOXALARM_SNAPSHOT_WIRE_SIZE;
    }
    return LOXALARM_OK;
}

static inline lox_alarm_status_t lox_alarm_snapshot_decode(lox_alarm_snapshot_t *snap,
                                                           uint32_t *schema_id_out,
                                                           const uint8_t *in,
                                                           size_t in_size) {
    if (snap == NULL || in == NULL) {
        return LOXALARM_ERR_INVALID_ARG;
    }
    if (in_size != LOXALARM_SNAPSHOT_WIRE_SIZE) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }

    const uint8_t *p = in;
    if (lox_alarm__read_u32_le(p) != LOXALARM_SNAPSHOT_MAGIC) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    p += 4;
    if (*p++ != LOXALARM_SNAPSHOT_WIRE_VERSION) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    const uint8_t flags = *p++;
    p += 2;
    if (lox_alarm__read_u32_le(p) != (uint32_t)LOXALARM_SNAPSHOT_WIRE_SIZE) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    p += 4;
    uint32_t schema_id = lox_alarm__read_u32_le(p);
    p += 4;
    uint8_t state = *p++;
    uint8_t resume_state = *p++;
    uint8_t reserved0 = *p++;
    uint8_t reserved1 = *p++;
    lox_alarm_snapshot_t tmp = {0};
    tmp.schema_id = schema_id;
    tmp.flags = flags;
    tmp.state = state;
    tmp.shelve_resume_state = resume_state;
    tmp._reserved0 = reserved0;
    tmp.ack_id = lox_alarm__read_u32_le(p); p += 4;
    tmp.shelve_op_id = lox_alarm__read_u32_le(p); p += 4;
    tmp.unshelve_op_id = lox_alarm__read_u32_le(p); p += 4;
    tmp.activation_count = lox_alarm__read_u32_le(p); p += 4;
    tmp.reactivation_count = lox_alarm__read_u32_le(p); p += 4;
    tmp.ack_count = lox_alarm__read_u32_le(p); p += 4;
    tmp.shelve_count = lox_alarm__read_u32_le(p); p += 4;
    tmp.state_entered_ms = lox_alarm__read_u32_le(p); p += 4;
    tmp.cond_true_since_ms = lox_alarm__read_u32_le(p); p += 4;
    tmp.cond_false_since_ms = lox_alarm__read_u32_le(p); p += 4;
    tmp.shelve_expires_ms = lox_alarm__read_u32_le(p); p += 4;
    tmp._reserved1 = lox_alarm__read_u32_le(p); p += 4;
    tmp._reserved2 = lox_alarm__read_u32_le(p); p += 4;
    tmp._reserved0 = reserved0;

    if ((flags & ~(1u << 0 | 1u << 1 | 1u << 2)) != 0u) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if (tmp._reserved0 != 0 || reserved1 != 0 || tmp._reserved1 != 0 || tmp._reserved2 != 0) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if (!lox_alarm__is_supported_state_u8(tmp.state) ||
        !lox_alarm__is_valid_state_u8(tmp.shelve_resume_state)) {
        return LOXALARM_ERR_INVALID_SNAPSHOT;
    }
    if (schema_id_out != NULL) {
        *schema_id_out = schema_id;
    }
    *snap = tmp;
    return LOXALARM_OK;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LOXALARM_LOXALARM_H */
