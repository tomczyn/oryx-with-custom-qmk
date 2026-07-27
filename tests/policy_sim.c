// Dry simulator for the host-modifier policy. No keyboard involved.
//
// It links the REAL quantum/os_detection.c and calls the REAL policy transitions from
// hd_host_os.h, with a virtual millisecond clock standing in for housekeeping. So for any USB
// setup-packet stream it answers: what does QMK detect, what does it report and when, and what
// mapping does the board end up on.
//
// Build/run: see HOST_MODIFIERS.md.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "hd_host_os.h"
#include "hd_setups_data.h"
#include "os_detection.h"

#ifndef HD_HOST_FALLBACK_MS
#    define HD_HOST_FALLBACK_MS 3000
#endif

uint32_t hd_test_now_ms = 1000;

static hd_policy_inputs_t policy;
static bool               fallback_armed;
static uint32_t           configured_at;
static bool               usb_configured;

static char   report_log[256];
static size_t report_len;

static const char *osname(os_variant_t o) {
    switch (o) {
        case OS_UNSURE: return "unsure";
        case OS_LINUX: return "linux";
        case OS_WINDOWS: return "windows";
        case OS_MACOS: return "macos";
        case OS_IOS: return "ios";
    }
    return "?";
}

static const char *srcname(hd_policy_source_t s) {
    switch (s) {
        case HD_SRC_INIT: return "init";
        case HD_SRC_DETECTED: return "detected";
        case HD_SRC_FALLBACK: return "fallback";
        case HD_SRC_MANUAL: return "manual";
    }
    return "?";
}

// The firmware's callback is intentionally inert; log QMK's verdict so the table can show how
// wrong it would have been.
bool process_detected_host_os_user(os_variant_t os) {
    report_len += (size_t)snprintf(report_log + report_len, sizeof(report_log) - report_len, "%s%s",
                                   report_len ? "," : "", osname(os));
    return true;
}

// This is what the firmware's housekeeping_task_user does, minus pushing into keymap_config.
static void housekeeping(void) {
    switch (hd_current_host_class()) {
        case HD_HOST_APPLE:
            hd_policy_on_detection(&policy, OS_MACOS);
            break;
        case HD_HOST_PC:
            hd_policy_on_detection(&policy, OS_LINUX);
            break;
        case HD_HOST_UNKNOWN:
            break;
    }
    if (!usb_configured) {
        return;
    }
    if (!fallback_armed) {
        fallback_armed = true;
        configured_at  = hd_test_now_ms;
    } else if (!policy.fallback_due && !policy.detected_known &&
               hd_test_now_ms - configured_at >= HD_HOST_FALLBACK_MS) {
        hd_policy_on_fallback(&policy);
    }
}

// Advance the virtual clock the way the real board does: housekeeping and os_detection_task run
// every scan, so step in small increments rather than jumping.
static void advance(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        hd_test_now_ms++;
        os_detection_task();
        housekeeping();
    }
}

static void power_cycle(void) {
    erase_wlength_data();
    memset(&policy, 0, sizeof(policy));
    fallback_armed = false;
    usb_configured = false;
    report_log[0]  = '\0';
    report_len     = 0;
}

static void configure(void) {
    struct usb_device_state st = {.configure_state = USB_DEVICE_STATE_CONFIGURED};
    os_detection_notify_usb_device_state_change(st);
    usb_configured = true;
}

static void feed(const uint16_t *seq, size_t len) {
    for (size_t i = 0; i < len; i++) {
        process_wlength(seq[i]);
        advance(1);
    }
}

#define FEED(seq) feed((seq), sizeof(seq) / sizeof(*(seq)))

static const char *classname(hd_host_class_t c) {
    switch (c) {
        case HD_HOST_UNKNOWN: return "unknown";
        case HD_HOST_APPLE: return "apple";
        case HD_HOST_PC: return "pc";
    }
    return "?";
}

static void report_row(const char *name) {
    bool swap = hd_resolve_swap(policy);
    printf("  %-42s qmk=%-8s ours=%-8s -> %-8s %s\n", name, osname(detected_host_os()),
           classname(hd_current_host_class()), srcname(hd_policy_source(policy)),
           swap ? "SWAPPED (Command)" : "stock (Ctrl)");
}

// Sequences from QMK's own collected data in quantum/os_detection/tests/os_detection.cpp.
static const uint16_t macos_15[]      = {0x02, 0x4E, 0x02, 0x1C, 0x02, 0x1A, 0xFF, 0xFF};
static const uint16_t macos_15_alt[]  = {0x02, 0x42, 0x02, 0x1C, 0x02, 0x1A, 0xFF};
static const uint16_t macos_12[]      = {0x02, 0x24, 0x02, 0x28, 0xFF};
static const uint16_t macos_no_tail[] = {0x02, 0x4E, 0x02, 0x1C, 0x02, 0x1A};
static const uint16_t linux_real[]    = {0xFF, 0xFF, 0xFF};
static const uint16_t windows_real[]  = {0xFF, 0xFF, 0x04, 0x24, 0x04, 0x24};
static const uint16_t stray_04[]      = {0x04};

int main(void) {
    printf("HD_HOST_FALLBACK_MS=%d  HD_UNSURE_FALLBACK_SWAP=%s\n\n", HD_HOST_FALLBACK_MS,
           HD_UNSURE_FALLBACK_SWAP ? "true" : "false");

    printf("== Baseline: hosts behaving as QMK expects ==\n");
    power_cycle();
    configure();
    FEED(macos_15);
    advance(4000);
    report_row("macOS 15.x, clean enumeration");

    power_cycle();
    configure();
    FEED(macos_12);
    advance(4000);
    report_row("macOS 12.5, clean enumeration");

    power_cycle();
    configure();
    FEED(linux_real);
    advance(4000);
    report_row("real Linux");

    power_cycle();
    configure();
    FEED(windows_real);
    advance(4000);
    report_row("real Windows");

    printf("\n== The stall the fallback was meant to fix ==\n");
    power_cycle();
    configure();
    FEED(macos_no_tail);
    advance(4000);
    report_row("macOS, trailing 0xFF never arrives");

    printf("\n== One stray 4-byte control read, injected at different points ==\n");
    power_cycle();
    configure();
    FEED(macos_no_tail);
    FEED(stray_04);
    advance(4000);
    report_row("macOS (no tail) then 0x04");

    power_cycle();
    configure();
    FEED(macos_15);
    advance(4000);
    FEED(stray_04);
    advance(4000);
    report_row("macOS clean, detected, then 0x04 later");

    power_cycle();
    configure();
    FEED(stray_04);
    FEED(macos_15);
    advance(4000);
    report_row("0x04 arrives before the macOS packets");

    power_cycle();
    configure();
    FEED(macos_15);
    FEED(macos_15_alt);
    FEED(stray_04);
    advance(4000);
    report_row("macOS twice (sleep/wake) then 0x04");

    printf("\n== Post-DFU: abbreviated re-enumeration, several shapes ==\n");
    power_cycle();
    configure();
    advance(4000);
    report_row("no setup packets at all");

    power_cycle();
    configure();
    const uint16_t two_only[] = {0x02, 0x4E};
    FEED(two_only);
    advance(4000);
    report_row("two packets, nothing conclusive");

    power_cycle();
    configure();
    const uint16_t three_02[] = {0x02, 0x4E, 0x02, 0x1C, 0x02};
    FEED(three_02);
    FEED(stray_04);
    advance(4000);
    report_row("three 0x02 then 0x04 (PS5 branch)");

    printf("\n== Manual override, on top of the worst case ==\n");
    power_cycle();
    configure();
    FEED(macos_no_tail);
    FEED(stray_04);
    advance(4000);
    hd_policy_on_manual(&policy, true);
    report_row("worst case + CG_SWAP pressed");
    return 0;
}
