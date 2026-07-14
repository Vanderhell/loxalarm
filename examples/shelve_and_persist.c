/*
 * examples/shelve_and_persist.c
 *
 * Demonstrates:
 *   - shelving an alarm during maintenance
 *   - snapshotting alarm state for power-loss recovery
 *   - restoring state on boot
 */

#include <stdio.h>
#include <loxalarm/loxalarm.h>

/* Pretend NVRAM-backed byte store. Replace with nvlog or loxdb. */
static uint8_t fake_nvram_slot[LOXALARM_SNAPSHOT_WIRE_SIZE];
static bool fake_nvram_valid = false;

static int nvram_save(const char *key, const void *buf, size_t n) {
    (void)key;
    if (n != sizeof(fake_nvram_slot)) {
        return -1;
    }
    for (size_t i = 0; i < n; ++i) {
        fake_nvram_slot[i] = ((const uint8_t *)buf)[i];
    }
    fake_nvram_valid = true;
    return 0;
}

static int nvram_load(const char *key, void *buf, size_t n) {
    (void)key;
    if (!fake_nvram_valid || n != sizeof(fake_nvram_slot)) {
        return -1;
    }
    for (size_t i = 0; i < n; ++i) {
        ((uint8_t *)buf)[i] = fake_nvram_slot[i];
    }
    return 0;
}

static const lox_alarm_config_t cfg = {
    .on_delay_ms   = 1000,
    .off_delay_ms  = 1000,
    .latched       = true,
    .shelvable     = true,
    .max_shelve_ms = 15u * 60u * 1000u,
    .tag           = "compressor_overtemp",
};

int main(void) {
    lox_alarm_t a;

    /* --- session 1: pre-reboot --- */
    if (lox_alarm_init(&a, &cfg) != LOXALARM_OK) {
        return 1;
    }
    if (lox_alarm_update(&a, true,  1000) != LOXALARM_OK ||
        lox_alarm_update(&a, true,  3000) != LOXALARM_OK ||
        lox_alarm_update(&a, false, 4500) != LOXALARM_OK ||
        lox_alarm_update(&a, false, 5500) != LOXALARM_OK) {
        return 2;
    }

    printf("before shelve: state=%d\n", lox_alarm_state(&a));

    /* Operator shelves for 10 min during maintenance. */
    if (lox_alarm_shelve(&a, 10u * 60u * 1000u, 6000, /*op=*/7) != LOXALARM_OK) {
        return 3;
    }
    printf("after  shelve: state=%d\n", lox_alarm_state(&a));

    /* Snapshot before we go down. */
    lox_alarm_snapshot_t snap;
    if (lox_alarm_snapshot_save(&a, &snap) == LOXALARM_OK) {
        uint8_t wire[LOXALARM_SNAPSHOT_WIRE_SIZE];
        size_t written = 0;
        if (lox_alarm_snapshot_encode(&snap, 1234u, wire, sizeof(wire), &written) == LOXALARM_OK &&
            written == sizeof(wire)) {
            nvram_save("alarm.compressor_overtemp", wire, sizeof(wire));
            printf("snapshot persisted (state=%u)\n", snap.state);
        }
    }

    /* --- power loss happens here --- */
    /* --- session 2: post-reboot --- */
    lox_alarm_t a2;
    uint8_t wire[LOXALARM_SNAPSHOT_WIRE_SIZE];

    if (nvram_load("alarm.compressor_overtemp",
                   wire, sizeof(wire)) == 0) {
        lox_alarm_snapshot_t loaded;
        uint32_t schema_id = 0;
        if (lox_alarm_snapshot_decode(&loaded, &schema_id, wire, sizeof(wire)) == LOXALARM_OK) {
        /* now_ms after reboot starts from 0 (or whatever your clock says) */
            if (lox_alarm_snapshot_load(&a2, &cfg, &loaded, /*now_ms=*/0) != LOXALARM_OK) {
                return 4;
            }
            printf("restored: schema=%u state=%d activations=%u\n",
                   (unsigned)schema_id, lox_alarm_state(&a2), a2.activation_count);
        }
    } else {
        if (lox_alarm_init(&a2, &cfg) != LOXALARM_OK) {
            return 5;
        }
        printf("no snapshot; cold start\n");
    }

    return 0;
}
