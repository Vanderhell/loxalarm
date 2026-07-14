#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <loxalarm/loxalarm.h>

static int g_failures = 0;

static void fail(const char *scenario, const char *msg) {
    ++g_failures;
    fprintf(stderr, "FAIL %s: %s\n", scenario, msg);
}

#define EXPECT_TRUE(SC, EXPR) do { if (!(EXPR)) fail((SC), "expected true: " #EXPR); } while (0)
#define EXPECT_EQ_U32(SC, A, B) do { uint32_t _a = (A), _b = (B); if (_a != _b) { \
    char _buf[160]; snprintf(_buf, sizeof(_buf), "expected %s == %s (got %u vs %u)", #A, #B, _a, _b); \
    fail((SC), _buf); } } while (0)
#define EXPECT_EQ_I32(SC, A, B) do { int _a = (A), _b = (B); if (_a != _b) { \
    char _buf[160]; snprintf(_buf, sizeof(_buf), "expected %s == %s (got %d vs %d)", #A, #B, _a, _b); \
    fail((SC), _buf); } } while (0)

static void S01_on_delay_exact_boundary_activates(void) {
    const char *SC = "S01";
    const lox_alarm_config_t cfg = { .on_delay_ms = 2000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    EXPECT_EQ_I32(SC, lox_alarm_update(&a, true, 0), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_update(&a, true, 1999), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);

    EXPECT_EQ_I32(SC, lox_alarm_update(&a, true, 2000), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_TRUE(SC, lox_alarm_just_activated(&a));
    EXPECT_TRUE(SC, !lox_alarm_just_activated(&a));
    EXPECT_EQ_U32(SC, a.activation_count, 1u);
}

static void S02_off_delay_exact_boundary_clears(void) {
    const char *SC = "S02";
    const lox_alarm_config_t cfg = { .off_delay_ms = 5000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_alarm_update(&a, false, 1000);
    lox_alarm_update(&a, false, 4999);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_alarm_update(&a, false, 5999);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    lox_alarm_update(&a, false, 6000);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
    EXPECT_TRUE(SC, lox_alarm_just_returned(&a));
    EXPECT_TRUE(SC, !lox_alarm_just_returned(&a));
}

static void S03_latched_ack_and_operator_metadata(void) {
    const char *SC = "S03";
    const lox_alarm_config_t cfg = { .latched = true };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, false, 1);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);

    EXPECT_EQ_I32(SC, lox_alarm_ack(&a, 2, 42), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
    EXPECT_EQ_U32(SC, (uint32_t)a.ack_id, 42u);
    EXPECT_EQ_U32(SC, a.ack_count, 1u);
    EXPECT_TRUE(SC, lox_alarm_just_acked(&a));
}

static void S04_reactivation_while_latched_return_counts_twice(void) {
    const char *SC = "S04";
    const lox_alarm_config_t cfg = { .on_delay_ms = 2000, .latched = true };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, true, 2000);
    lox_alarm_update(&a, false, 2100);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_LATCHED_RETURN);

    lox_alarm_update(&a, true, 4100);
    lox_alarm_update(&a, true, 6100);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_EQ_U32(SC, a.activation_count, 2u);
    EXPECT_EQ_U32(SC, a.reactivation_count, 1u);
}

static void S05_shelve_and_unshelve_operator_metadata(void) {
    const char *SC = "S05";
    const lox_alarm_config_t cfg = { .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);

    EXPECT_EQ_I32(SC, lox_alarm_shelve(&a, 1000, 10, 7), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_SHELVED);
    EXPECT_EQ_U32(SC, (uint32_t)a.shelve_op_id, 7u);
    EXPECT_TRUE(SC, lox_alarm_just_shelved(&a));

    EXPECT_EQ_I32(SC, lox_alarm_unshelve(&a, 20, 9), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_EQ_U32(SC, (uint32_t)a.unshelve_op_id, 9u);
}

static void S06_max_shelve_zero_rejected_at_init(void) {
    const char *SC = "S06";
    const lox_alarm_config_t cfg = { .shelvable = true, .max_shelve_ms = 0 };
    lox_alarm_t a;
    EXPECT_EQ_I32(SC, lox_alarm_init(&a, &cfg), LOXALARM_ERR_INVALID_ARG);
}

static void S07_clock_wrap_boundary_is_supported(void) {
    const char *SC = "S07";
    const lox_alarm_config_t cfg = { .on_delay_ms = 2000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, UINT32_MAX - 1000u);
    lox_alarm_update(&a, true, UINT32_MAX);
    lox_alarm_update(&a, true, 1000u);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
}

static void S08_backward_clock_jump_rejected(void) {
    const char *SC = "S08";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 1000);
    EXPECT_EQ_I32(SC, lox_alarm_update(&a, true, 999), LOXALARM_ERR_CLOCK);
}

static void S09_counter_saturation_does_not_block_state_transitions(void) {
    const char *SC = "S09";
    const lox_alarm_config_t cfg = { .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    a.activation_count = UINT32_MAX;
    EXPECT_EQ_I32(SC, lox_alarm_update(&a, true, 0), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_ACTIVE);
    EXPECT_EQ_U32(SC, a.activation_count, UINT32_MAX);

    a.state = LOX_ALARM_ACTIVE;
    a.shelve_count = UINT32_MAX;
    EXPECT_EQ_I32(SC, lox_alarm_shelve(&a, 10, 1, 3), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_SHELVED);
    EXPECT_EQ_U32(SC, a.shelve_count, UINT32_MAX);
}

static void S10_snapshot_portable_roundtrip(void) {
    const char *SC = "S10";
    const lox_alarm_config_t cfg = { .latched = true, .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, false, 1);
    lox_alarm_shelve(&a, 10, 2, 5);

    lox_alarm_snapshot_t snap;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_save(&a, &snap), LOXALARM_OK);
    snap.schema_id = 77;

    uint8_t wire[LOXALARM_SNAPSHOT_WIRE_SIZE];
    size_t written = 0;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_encode(&snap, snap.schema_id, wire, sizeof(wire), &written), LOXALARM_OK);
    EXPECT_EQ_U32(SC, (uint32_t)written, (uint32_t)sizeof(wire));

    lox_alarm_snapshot_t decoded;
    uint32_t schema_id = 0;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_decode(&decoded, &schema_id, wire, sizeof(wire)), LOXALARM_OK);
    EXPECT_EQ_U32(SC, schema_id, 77u);
    EXPECT_EQ_U32(SC, decoded.activation_count, snap.activation_count);
    EXPECT_EQ_U32(SC, decoded.shelve_count, snap.shelve_count);
    EXPECT_EQ_U32(SC, decoded.shelve_expires_ms, snap.shelve_expires_ms);

    lox_alarm_t b;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&b, &cfg, &decoded, 3), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&b), lox_alarm_state(&a));
}

static void S11_snapshot_decoder_rejects_corruption(void) {
    const char *SC = "S11";
    const uint8_t zero[LOXALARM_SNAPSHOT_WIRE_SIZE] = {0};
    lox_alarm_snapshot_t snap;
    uint32_t schema_id = 0;

    EXPECT_EQ_I32(SC, lox_alarm_snapshot_decode(&snap, &schema_id, zero, sizeof(zero)), LOXALARM_ERR_INVALID_SNAPSHOT);

    uint8_t wire[LOXALARM_SNAPSHOT_WIRE_SIZE] = {0};
    lox_alarm_snapshot_t tmp = {0};
    tmp.state = (uint8_t)LOX_ALARM_ACTIVE;
    tmp.shelve_resume_state = (uint8_t)LOX_ALARM_ACTIVE;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_encode(&tmp, 1, wire, sizeof(wire), NULL), LOXALARM_OK);
    wire[0] ^= 0xffu;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_decode(&snap, &schema_id, wire, sizeof(wire)), LOXALARM_ERR_INVALID_SNAPSHOT);
}

static void S12_out_of_service_freezes_evaluation(void) {
    const char *SC = "S12";
    const lox_alarm_config_t cfg = { .on_delay_ms = 0 };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    EXPECT_EQ_I32(SC, lox_alarm_set_out_of_service(&a, true, 0), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_OUT_OF_SERVICE);
    EXPECT_EQ_I32(SC, lox_alarm_update(&a, true, 10), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_OUT_OF_SERVICE);
    EXPECT_EQ_I32(SC, lox_alarm_set_out_of_service(&a, false, 20), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
}

static void S13_force_reset_preserves_lifetime_counters(void) {
    const char *SC = "S13";
    const lox_alarm_config_t cfg = { .latched = true };
    lox_alarm_t a;
    lox_alarm_init(&a, &cfg);

    lox_alarm_update(&a, true, 0);
    lox_alarm_update(&a, false, 1);
    lox_alarm_ack(&a, 2, 1);
    EXPECT_EQ_U32(SC, a.activation_count, 1u);

    EXPECT_EQ_I32(SC, lox_alarm_force_reset(&a, 10), LOXALARM_OK);
    EXPECT_EQ_I32(SC, lox_alarm_state(&a), LOX_ALARM_NORMAL);
    EXPECT_EQ_U32(SC, a.activation_count, 1u);
}

static void S14_null_and_uninitialised_queries_are_safe(void) {
    const char *SC = "S14";
    lox_alarm_t a;
    memset(&a, 0, sizeof(a));

    EXPECT_EQ_I32(SC, lox_alarm_state(NULL), LOX_ALARM_NORMAL);
    EXPECT_EQ_U32(SC, lox_alarm_drain_reason_flags(NULL), 0u);
    EXPECT_TRUE(SC, !lox_alarm_is_active(NULL));
    EXPECT_TRUE(SC, !lox_alarm_needs_attention(&a));
    EXPECT_TRUE(SC, !lox_alarm_just_activated(&a));
}

static void S15_snapshot_rejects_shelved_state_when_not_shelvable(void) {
    const char *SC = "S15";
    const lox_alarm_config_t cfg = {0};
    lox_alarm_snapshot_t snap = {0};
    lox_alarm_t a;

    snap.state = (uint8_t)LOX_ALARM_SHELVED;
    snap.shelve_resume_state = (uint8_t)LOX_ALARM_ACTIVE;
    snap.flags = 1u << 2;
    snap.shelve_expires_ms = 100;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, &cfg, &snap, 0), LOXALARM_ERR_INVALID_SNAPSHOT);
}

static void S16_snapshot_rejects_latched_return_when_not_latched(void) {
    const char *SC = "S16";
    const lox_alarm_config_t cfg = { .shelvable = true, .max_shelve_ms = 1000 };
    lox_alarm_snapshot_t snap = {0};
    lox_alarm_t a;

    snap.state = (uint8_t)LOX_ALARM_LATCHED_RETURN;
    snap.shelve_resume_state = (uint8_t)LOX_ALARM_ACTIVE;
    EXPECT_EQ_I32(SC, lox_alarm_snapshot_load(&a, &cfg, &snap, 0), LOXALARM_ERR_INVALID_SNAPSHOT);
}

int main(void) {
    S01_on_delay_exact_boundary_activates();
    S02_off_delay_exact_boundary_clears();
    S03_latched_ack_and_operator_metadata();
    S04_reactivation_while_latched_return_counts_twice();
    S05_shelve_and_unshelve_operator_metadata();
    S06_max_shelve_zero_rejected_at_init();
    S07_clock_wrap_boundary_is_supported();
    S08_backward_clock_jump_rejected();
    S09_counter_saturation_does_not_block_state_transitions();
    S10_snapshot_portable_roundtrip();
    S11_snapshot_decoder_rejects_corruption();
    S12_out_of_service_freezes_evaluation();
    S13_force_reset_preserves_lifetime_counters();
    S14_null_and_uninitialised_queries_are_safe();
    S15_snapshot_rejects_shelved_state_when_not_shelvable();
    S16_snapshot_rejects_latched_return_when_not_latched();

    if (g_failures == 0) {
        printf("OK\n");
        return 0;
    }
    fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
}
