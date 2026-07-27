#pragma once
#include "os_detection.h"

static inline bool hd_should_swap_ctrl_gui(os_variant_t os) {
    return os == OS_MACOS || os == OS_IOS;
}

static inline bool hd_ctrl_gui_swap_needs_update(bool left, bool right, bool desired) {
    return left != desired || right != desired;
}

// QMK re-reports the detected OS for the whole power-on session, and its guess can decay to a
// wrong value that never recovers: the wLength counters in quantum/os_detection.c are cumulative
// and are never reset on re-enumeration, while the guess ladder tests the Windows pattern
// (cnt_ff >= 2 && cnt_04 >= 1) before the macOS pattern. Two macOS enumerations plus any single
// 4-byte control read therefore latch detected_os to OS_WINDOWS permanently. So once an Apple
// host is confirmed, hold the swap for the rest of the session; only a power cycle, which runs
// hd_keyboard_post_init(), clears the latch. See HOST_MODIFIERS.md.
static inline bool hd_apple_host_latch(bool latched, os_variant_t os) {
    return latched || hd_should_swap_ctrl_gui(os);
}
