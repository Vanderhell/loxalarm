/*
 * examples/shelve_and_persist.c
 *
 * Demonstrates:
 *   - shelving an alarm during maintenance
 *   - snapshotting alarm state for power-loss recovery
 *   - restoring state on boot
 */

#include <stdio.h>
#include <string.h>
#include <loxalarm/loxalarm.h>

/* Pretend NVRAM-backed key/value store. Replace with nvlog or loxdb. */
static lox_alarm_snapshot_t fake_nvram_slot;
static bool                 fake_nvram_valid = false;

static int nvram_save(const char *key, const void *buf, size_t n) {
    (void)key;
    if (n != sizeof(fake_nvram_slot)) return -1;
    memcpy(&fake_nvram_slot, buf, n);
    fake_nvram_valid = true;
    return 0;
}

static int nvram_load(const char *key, void *buf, size_t n) {
    (void)key;
    if (!fake_nvram_valid)              return -1;
    if (n != sizeof(fake_nvram_slot))   return -1;
    memcpy(buf, &fake_nvram_slot, n);
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
    lox_alarm_init(&a, &cfg);
    lox_alarm_update(&a, true,  1000);
    lox_alarm_update(&a, true,  3000);   /* now ACTIVE */
    lox_alarm_update(&a, false, 4500);
    lox_alarm_update(&a, false, 5500);   /* now LATCHED_RETURN */

    printf("before shelve: state=%d\n", lox_alarm_state(&a));

    /* Operator shelves for 10 min during maintenance. */
    lox_alarm_shelve(&a, 10u * 60u * 1000u, 6000, /*op=*/7);
    printf("after  shelve: state=%d\n", lox_alarm_state(&a));

    /* Snapshot before we go down. */
    lox_alarm_snapshot_t snap;
    if (lox_alarm_snapshot_save(&a, &snap) == LOX_OK) {
        nvram_save("alarm.compressor_overtemp", &snap, sizeof(snap));
        printf("snapshot persisted (state=%u)\n", snap.state);
    }

    /* --- power loss happens here --- */
    /* --- session 2: post-reboot --- */
    lox_alarm_t a2;
    lox_alarm_snapshot_t loaded;

    if (nvram_load("alarm.compressor_overtemp",
                   &loaded, sizeof(loaded)) == 0) {
        /* now_ms after reboot starts from 0 (or whatever your clock says) */
        lox_alarm_snapshot_load(&a2, &cfg, &loaded, /*now_ms=*/0);
        printf("restored: state=%d activations=%u\n",
               lox_alarm_state(&a2), a2.activation_count);
    } else {
        lox_alarm_init(&a2, &cfg);
        printf("no snapshot; cold start\n");
    }

    return 0;
}
