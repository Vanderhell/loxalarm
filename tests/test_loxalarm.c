#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <loxalarm/loxalarm.h>

static int g_failures = 0;

static void fail(const char *scenario, const char *msg) {
    ++g_failures;
    fprintf(stderr, "FAIL %s: %s\n", scenario, msg);
}

#define EXPECT_TRUE(SC, EXPR) do { if (!(EXPR)) fail((SC), "expected true: " #EXPR); } while (0)
#define EXPECT_EQ_U32(SC, A, B) do { uint32_t _a=(A), _b=(B); if (_a!=_b) { \
    char _buf[160]; snprintf(_buf, sizeof(_buf), "expected %s == %s (got %u vs %u)", #A, #B, _a, _b); \
    fail((SC), _buf); } } while (0)
#define EXPECT_EQ_I32(SC, A, B) do { int _a=(A), _b=(B); if (_a!=_b) { \
    char _buf[160]; snprintf(_buf, sizeof(_buf), "expected %s == %s (got %d vs %d)", #A, #B, _a, _b); \
    fail((SC), _buf); } } while (0)

static void S16_invalid_args_rejected(void) {
    const char *SC = "S16";
    const lox_alarm_config_t cfg = {0};
    lox_alarm_t a;

    EXPECT_EQ_I32(SC, lox_alarm_init(NULL, &cfg), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_init(&a, NULL), LOX_ERR_INVALID_ARG);

    lox_alarm_init(&a, &cfg);
    EXPECT_EQ_I32(SC, lox_alarm_ack(NULL, 0, 0), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_update(NULL, false, 0), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_shelve(NULL, 1, 0, 0), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_unshelve(NULL, 0), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_set_out_of_service(NULL, true, 0), LOX_ERR_INVALID_ARG);

    lox_alarm_snapshot_t snap;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_save(NULL, &snap), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_save(&a, NULL), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(NULL, &cfg, &snap, 0), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, NULL, &snap, 0), LOX_ERR_INVALID_ARG);
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, &cfg, NULL, 0), LOX_ERR_INVALID_ARG);
}

static void S17_shelve_disabled_and_state_errors(void) {
    const char *SC = "S17";
    const lox_alarm_config_t cfg = { .shelvable = false };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    EXPECT_EQ_I32(SC, lox_alarm_shelve(&a, 1, 0, 0), LOX_ERR_DISABLED);
    EXPECT_EQ_I32(SC, lox_alarm_unshelve(&a, 0), LOX_ERR_STATE);
}

static void S18_unshelve_wrong_state_rejected(void) {
    const char *SC = "S18";
    const lox_alarm_config_t cfg = { .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    /* Not shelved yet. */
    EXPECT_EQ_I32(SC, lox_alarm_unshelve(&a, 0), LOX_ERR_STATE);
}

static void S19_snapshot_version_mismatch_rejected(void) {
    const char *SC = "S19";
    const lox_alarm_config_t cfg = {0};
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.version = 99;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, &cfg, &snap, 0), LOX_ERR_INVALID_ARG);
}

static void S20_oos_exit_resets_to_normal(void) {
    const char *SC = "S20";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .latched = true, .off_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    EXPECT_EQ_I32(SC, lox_alarm_set_out_of_service(&a, true, 10), LOX_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_OUT_OF_SERVICE);

    /* Must reject "exit" call if not OOS is false? Exiting from OOS should work. */
    EXPECT_EQ_I32(SC, lox_alarm_set_out_of_service(&a, false, 20), LOX_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
}

static void S21_invalid_snapshot_state_rejected(void) {
    const char *SC = "S21";
    const lox_alarm_config_t cfg = {0};
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.version = 1;
    snap.state = 250;
    snap.shelve_resume_state = (uint8_t)LOX_ALARM_ACTIVE;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, &cfg, &snap, 0), LOX_ERR_INVALID_ARG);
}

static void S22_invalid_snapshot_shelve_resume_state_rejected(void) {
    const char *SC = "S22";
    const lox_alarm_config_t cfg = {0};
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.version = 1;
    snap.state = (uint8_t)LOX_ALARM_NORMAL;
    snap.shelve_resume_state = 250;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, &cfg, &snap, 0), LOX_ERR_INVALID_ARG);
}

static void S23_shelved_snapshot_invalid_resume_state_rejected(void) {
    const char *SC = "S23";
    const lox_alarm_config_t cfg = {0};
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.version = 1;
    snap.state = (uint8_t)LOX_ALARM_SHELVED;
    snap.shelve_resume_state = (uint8_t)LOX_ALARM_NORMAL;
    snap.shelve_expires_ms = 100;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, &cfg, &snap, 0), LOX_ERR_INVALID_ARG);
}

static void S24_non_shelved_snapshot_does_not_restore_shelve_metadata(void) {
    const char *SC = "S24";
    const lox_alarm_config_t cfg = {0};

    lox_alarm_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));
    snap.version = 1;
    snap.state = (uint8_t)LOX_ALARM_ACTIVE;
    snap.shelve_resume_state = (uint8_t)LOX_ALARM_ACTIVE;
    snap.shelve_expires_ms = 1234;

    lox_alarm_t a;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, &cfg, &snap, 0), LOX_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_EQ_U32(SC, a.shelve_expires_ms, 0u);
}

static void S25_oos_from_shelved_clears_shelving_metadata(void) {
    const char *SC = "S25";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .shelvable = true, .max_shelve_ms = 600000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    EXPECT_EQ_I32(SC, lox_alarm_shelve(&a, 1000, 10, 1), LOX_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_SHELVED);
    EXPECT_TRUE(SC, a.shelve_expires_ms != 0u);

    EXPECT_EQ_I32(SC, lox_alarm_set_out_of_service(&a, true, 20), LOX_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_OUT_OF_SERVICE);
    EXPECT_EQ_U32(SC, a.shelve_expires_ms, 0u);

    EXPECT_EQ_I32(SC, lox_alarm_set_out_of_service(&a, false, 30), LOX_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
}

static void S26_just_acked_is_one_shot(void) {
    const char *SC = "S26";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .off_delay_ms = 0, .latched = true };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, false, 1);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);

    EXPECT_EQ_I32(SC, lox_alarm_ack(&a, 2, 7), LOX_OK);
    EXPECT_TRUE(SC, lox_alarm_just_acked(&a));
    EXPECT_TRUE(SC, !lox_alarm_just_acked(&a));
}

static void S27_just_shelved_is_one_shot(void) {
    const char *SC = "S27";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    EXPECT_EQ_I32(SC, lox_alarm_shelve(&a, 1000, 1, 0), LOX_OK);
    EXPECT_TRUE(SC, lox_alarm_just_shelved(&a));
    EXPECT_TRUE(SC, !lox_alarm_just_shelved(&a));
}

static void S28_shelve_expiry_sets_reason_flag(void) {
    const char *SC = "S28";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    EXPECT_EQ_I32(SC, lox_alarm_shelve(&a, 10, 1, 0), LOX_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_SHELVED);

    lox_alarm_update(&a, true, 50);
    uint32_t flags = lox_alarm_drain_reason_flags(&a);
    EXPECT_TRUE(SC, (flags & LOX_REASON_SHELVE_EXPIRE) != 0u);
}

static void S29_counter_overflow_returns_overflow(void) {
    const char *SC = "S29";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    a.activation_count = UINT32_MAX;
    EXPECT_EQ_I32(SC, lox_alarm_update(&a, true, 0), LOX_ERR_OVERFLOW);

    /* Reset back to NORMAL for shelve test. */
    lox_alarm_init(&a, &cfg);
    a.shelve_count = UINT32_MAX;
    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_shelve(&a, 1, 1, 0), LOX_ERR_OVERFLOW);
}

static void S30_needs_attention_and_null_helpers(void) {
    const char *SC = "S30";

    EXPECT_EQ_I32(SC, lox_alarm_state(NULL), LOX_ALARM_NORMAL);
    EXPECT_EQ_U32(SC, lox_alarm_drain_reason_flags(NULL), 0u);

    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);
    EXPECT_TRUE(SC, !lox_alarm_needs_attention(&a));

    lox_alarm_update(&a, true, 0);
    EXPECT_TRUE(SC, lox_alarm_needs_attention(&a));

    EXPECT_EQ_I32(SC, lox_alarm_shelve(&a, 1000, 1, 0), LOX_OK);
    EXPECT_TRUE(SC, !lox_alarm_needs_attention(&a));

    a.state = LOX_ALARM_SUPPRESSED;
    EXPECT_TRUE(SC, !lox_alarm_needs_attention(&a));

    lox_alarm_set_out_of_service(&a, true, 2);
    EXPECT_TRUE(SC, !lox_alarm_needs_attention(&a));
}

static void S01_spike_below_on_delay(void) {
    const char *SC = "S01";
    const lox_alarm_config_t cfg = { .on_delay_ms = 2000, .off_delay_ms = 0, .latched = false };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, true, 1500);
    lox_alarm_update(&a, false, 1700);

    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
    EXPECT_EQ_U32(SC, a.activation_count, 0u);
}

static void S02_sustained_activates_after_on_delay(void) {
    const char *SC = "S02";
    const lox_alarm_config_t cfg = { .on_delay_ms = 2000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, true, 1999);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);

    lox_alarm_update(&a, true, 2000);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_TRUE(SC, lox_alarm_just_activated(&a));
    EXPECT_TRUE(SC, !lox_alarm_just_activated(&a));
    EXPECT_EQ_U32(SC, a.activation_count, 1u);
}

static void S03_off_delay_before_clearing(void) {
    const char *SC = "S03";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .off_delay_ms = 5000, .latched = false };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_alarm_update(&a, false, 1000);
    lox_alarm_update(&a, false, 4999);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_alarm_update(&a, false, 5000);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_alarm_update(&a, false, 6000);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
    EXPECT_TRUE(SC, lox_alarm_just_returned(&a));
    EXPECT_TRUE(SC, !lox_alarm_just_returned(&a));
}

static void S04_latched_goes_to_latched_return(void) {
    const char *SC = "S04";
    const lox_alarm_config_t cfg = { .latched = true, .off_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    lox_alarm_update(&a, false, 100);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);
}

static void S05_ack_from_latched_clears(void) {
    const char *SC = "S05";
    const lox_alarm_config_t cfg = { .latched = true, .off_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, false, 1);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);

    lox_err_t r = lox_alarm_ack(&a, 2, 5);
    EXPECT_EQ_I32(SC, r, LOX_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
    EXPECT_EQ_U32(SC, (uint32_t)a.ack_id, 5u);
}

static void S06_ack_from_active_rejected(void) {
    const char *SC = "S06";
    const lox_alarm_config_t cfg = { .latched = true, .off_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);
    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_err_t r = lox_alarm_ack(&a, 1, 5);
    EXPECT_EQ_I32(SC, r, LOX_ERR_STATE);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
}

static void S07_reactivation_while_latched_return(void) {
    const char *SC = "S07";
    const lox_alarm_config_t cfg = { .latched = true, .on_delay_ms = 2000, .off_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, true, 2000);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_EQ_U32(SC, a.activation_count, 1u);

    lox_alarm_update(&a, false, 2100);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);

    lox_alarm_update(&a, true, 4100);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);

    lox_alarm_update(&a, true, 6100);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_EQ_U32(SC, a.activation_count, 2u);
}

static void S08_shelve_respects_max_duration(void) {
    const char *SC = "S08";
    const lox_alarm_config_t cfg = {
        .shelvable = true,
        .max_shelve_ms = 600000,
        .on_delay_ms = 0,
    };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);
    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_alarm_shelve(&a, 3600000, 100, 1);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_SHELVED);

    lox_alarm_update(&a, true, 100 + 599999);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_SHELVED);

    lox_alarm_update(&a, true, 100 + 600000);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
}

static void S09_unshelve_restores_prior_state(void) {
    const char *SC = "S09";
    const lox_alarm_config_t cfg = { .shelvable = true, .max_shelve_ms = 600000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);
    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_alarm_shelve(&a, 600000, 100, 1);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_SHELVED);

    lox_alarm_unshelve(&a, 200);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_EQ_U32(SC, a.shelve_expires_ms, 0u);
}

static void S10_snapshot_roundtrip_identity(void) {
    const char *SC = "S10";
    const lox_alarm_config_t cfg = { .latched = true, .shelvable = true, .max_shelve_ms = 600000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);
    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, false, 1);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);
    lox_alarm_shelve(&a, 1000, 2, 9);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_SHELVED);

    lox_alarm_snapshot_t snap;
    lox_alarm_snapshot_save(&a, &snap);

    lox_alarm_t b;
    lox_alarm_snapshot_load(&b, &cfg, &snap, 0);

    EXPECT_EQ_I32(SC, lox_alarm_state(&b), lox_alarm_state(&a));
    EXPECT_EQ_U32(SC, b.activation_count, a.activation_count);
    EXPECT_EQ_U32(SC, (uint32_t)b.ack_id, (uint32_t)a.ack_id);
}

static void S11_out_of_service_freezes_evaluation(void) {
    const char *SC = "S11";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_set_out_of_service(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_OUT_OF_SERVICE);

    lox_alarm_update(&a, true, 1000);
    lox_alarm_update(&a, false, 5000);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_OUT_OF_SERVICE);
}

static void S12_clock_wrap_uint32_boundary(void) {
    const char *SC = "S12";
    const lox_alarm_config_t cfg = { .on_delay_ms = 2000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, UINT32_MAX - 1000u);
    lox_alarm_update(&a, true, UINT32_MAX);
    lox_alarm_update(&a, true, 1000u);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
}

static void S13_reinit_while_running_resets_counters(void) {
    const char *SC = "S13";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);
    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_EQ_U32(SC, a.activation_count, 1u);

    lox_alarm_init(&a, &cfg);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
    EXPECT_EQ_U32(SC, a.activation_count, 0u);
}

static void S14_reset_preserves_lifetime_counters(void) {
    const char *SC = "S14";
    const lox_alarm_config_t cfg = { .latched = true, .off_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    for (int i = 0; i < 7; ++i) {
        lox_alarm_update(&a, true, (uint32_t)(i * 10));
        lox_alarm_update(&a, false, (uint32_t)(i * 10 + 1));
        lox_alarm_ack(&a, (uint32_t)(i * 10 + 2), 1);
    }
    EXPECT_EQ_U32(SC, a.activation_count, 7u);

    lox_alarm_update(&a, true, 1000);
    lox_alarm_update(&a, false, 1001);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);

    lox_alarm_reset(&a, 2000);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
    EXPECT_EQ_U32(SC, a.activation_count, 8u);
}

static void S15_reason_flags_accumulate_between_drains(void) {
    const char *SC = "S15";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0, .latched = true, .off_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0); /* ON_DELAY_MET */
    lox_alarm_update(&a, false, 1); /* OFF_DELAY_MET (ignored for this check) */
    lox_alarm_ack(&a, 2, 42); /* ACK */

    uint32_t flags = lox_alarm_drain_reason_flags(&a);
    EXPECT_TRUE(SC, (flags & LOX_REASON_ON_DELAY_MET) != 0);
    EXPECT_TRUE(SC, (flags & LOX_REASON_ACK) != 0);

    uint32_t flags2 = lox_alarm_drain_reason_flags(&a);
    EXPECT_EQ_U32(SC, flags2, 0u);
}

int main(void) {
    S01_spike_below_on_delay();
    S02_sustained_activates_after_on_delay();
    S03_off_delay_before_clearing();
    S04_latched_goes_to_latched_return();
    S05_ack_from_latched_clears();
    S06_ack_from_active_rejected();
    S07_reactivation_while_latched_return();
    S08_shelve_respects_max_duration();
    S09_unshelve_restores_prior_state();
    S10_snapshot_roundtrip_identity();
    S11_out_of_service_freezes_evaluation();
    S12_clock_wrap_uint32_boundary();
    S13_reinit_while_running_resets_counters();
    S14_reset_preserves_lifetime_counters();
    S15_reason_flags_accumulate_between_drains();
    S16_invalid_args_rejected();
    S17_shelve_disabled_and_state_errors();
    S18_unshelve_wrong_state_rejected();
    S19_snapshot_version_mismatch_rejected();
    S20_oos_exit_resets_to_normal();
    S21_invalid_snapshot_state_rejected();
    S22_invalid_snapshot_shelve_resume_state_rejected();
    S23_shelved_snapshot_invalid_resume_state_rejected();
    S24_non_shelved_snapshot_does_not_restore_shelve_metadata();
    S25_oos_from_shelved_clears_shelving_metadata();
    S26_just_acked_is_one_shot();
    S27_just_shelved_is_one_shot();
    S28_shelve_expiry_sets_reason_flag();
    S29_counter_overflow_returns_overflow();
    S30_needs_attention_and_null_helpers();

    if (g_failures == 0) {
        printf("OK\n");
        return 0;
    }
    fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
}
