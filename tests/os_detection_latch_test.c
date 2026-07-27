// Pins the upstream QMK behaviour that PY5XZ/hd_host_os.h's Apple latch exists to contain, by
// linking the real quantum/os_detection.c. If this test starts failing because ZSA fixed the
// cumulative-counter bug, hd_apple_host_latch() can be dropped and the policy can go back to
// following every re-report.
//
// The bug: setups_data in quantum/os_detection.c accumulates for the whole power-on session and
// is never reset on re-enumeration, and the guess ladder tests the Windows pattern
// (cnt_ff >= 2 && cnt_04 >= 1) before the macOS pattern. Both counters are monotonic, so once
// they cross those thresholds every later guess is OS_WINDOWS, permanently.
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include "hd_host_os.h"
#include "os_detection.h"

uint32_t hd_test_now_ms = 1000;

// macOS 15.x setup sequences, copied from the collected data in
// qmk_firmware/quantum/os_detection/tests/os_detection.cpp.
static const uint16_t macos_15[]     = {0x02, 0x4E, 0x02, 0x1C, 0x02, 0x1A, 0xFF, 0xFF};
static const uint16_t macos_15_alt[] = {0x02, 0x42, 0x02, 0x1C, 0x02, 0x1A, 0xFF};

static os_variant_t last_report  = OS_UNSURE;
static int          report_count = 0;

// Mirrors the shipped callback in PY5XZ/hd_custom.c, so the assertions below exercise the real
// policy against the real detection code.
static bool apple_confirmed = false;
static bool swap_ctrl_gui   = false;

bool process_detected_host_os_user(os_variant_t os) {
    last_report = os;
    report_count++;
    apple_confirmed = hd_apple_host_latch(apple_confirmed, os);
    swap_ctrl_gui   = apple_confirmed;
    return true;
}

// Mirrors hd_keyboard_post_init(), which runs only on a power cycle.
static void power_cycle(void) {
    apple_confirmed = false;
    swap_ctrl_gui   = false;
    erase_wlength_data();
}

static const struct usb_device_state st_configured = {.configure_state = USB_DEVICE_STATE_CONFIGURED};
static const struct usb_device_state st_init       = {.configure_state = USB_DEVICE_STATE_INIT};

#define FEED(seq) feed((seq), sizeof(seq) / sizeof(*(seq)))

static void feed(const uint16_t *seq, size_t len) {
    for (size_t i = 0; i < len; i++) {
        process_wlength(seq[i]);
    }
}

// Let the debounce expire and run the task, as housekeeping does on the board.
static void settle(void) {
    hd_test_now_ms += 300;
    os_detection_task();
}

static void reenumerate(void) {
    os_detection_notify_usb_device_state_change(st_init);
    os_detection_notify_usb_device_state_change(st_configured);
}

int main(void) {
    // Repeated macOS enumerations on their own are safe: no 4-byte packet, so the Windows branch
    // never opens and detected_os stays OS_MACOS.
    power_cycle();
    report_count = 0;
    os_detection_notify_usb_device_state_change(st_configured);
    FEED(macos_15);
    settle();
    assert(detected_host_os() == OS_MACOS);
    assert(report_count == 1 && last_report == OS_MACOS);
    assert(swap_ctrl_gui);
    for (int round = 0; round < 4; round++) {
        reenumerate();
        FEED(macos_15_alt);
        settle();
    }
    assert(detected_host_os() == OS_MACOS);
    assert(report_count == 1);
    assert(swap_ctrl_gui);

    // A second enumeration pushes cnt_ff past 2; one 4-byte control read then flips the guess to
    // OS_WINDOWS and QMK re-reports it mid-session. This is the regression: the board is still on
    // the same macOS host, but detection now claims Windows.
    power_cycle();
    report_count = 0;
    os_detection_notify_usb_device_state_change(st_configured);
    FEED(macos_15);
    settle();
    assert(detected_host_os() == OS_MACOS);
    assert(swap_ctrl_gui);
    reenumerate();
    FEED(macos_15_alt);
    settle();
    assert(detected_host_os() == OS_MACOS);
    const uint16_t four_byte_read[] = {0x04};
    FEED(four_byte_read);
    settle();
    assert(detected_host_os() == OS_WINDOWS);
    assert(report_count == 2 && last_report == OS_WINDOWS);
    // The latch is what keeps Command on the Ctrl positions through that bogus re-report.
    assert(swap_ctrl_gui);

    // The QMK flip is irrecoverable: the counters only grow, so further complete macOS
    // enumerations never restore OS_MACOS. A policy that followed each re-report would therefore
    // stay stock until the board lost power.
    for (int round = 0; round < 5; round++) {
        reenumerate();
        FEED(macos_15);
        settle();
    }
    assert(detected_host_os() == OS_WINDOWS);
    assert(report_count == 2);
    assert(swap_ctrl_gui);

    // The latch is per power-on session, not permanent: a real host change cuts bus power on this
    // bus-powered board, and the Linux/Windows/unsure results stay stock from a cleared latch.
    power_cycle();
    report_count = 0;
    const uint16_t linux_seq[] = {0xFF, 0xFF, 0xFF};
    os_detection_notify_usb_device_state_change(st_configured);
    FEED(linux_seq);
    settle();
    assert(detected_host_os() == OS_LINUX);
    assert(report_count == 1 && last_report == OS_LINUX);
    assert(!swap_ctrl_gui);
    return 0;
}
