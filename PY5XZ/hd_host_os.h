#pragma once
#include "os_detection.h"

// When detection never resolves to a positive result, assume the primary host. macOS is right for
// this board; a positively detected Linux/Windows host still overrides it, because those patterns
// are short and unambiguous (Linux is 3 packets) and always arrive well inside the fallback delay.
#ifndef HD_UNSURE_FALLBACK_SWAP
#    define HD_UNSURE_FALLBACK_SWAP true
#endif

static inline bool hd_should_swap_ctrl_gui(os_variant_t os) {
    return os == OS_MACOS || os == OS_IOS;
}

typedef enum {
    HD_HOST_UNKNOWN = 0,
    HD_HOST_APPLE,
    HD_HOST_PC,
} hd_host_class_t;

// Classify the host from the wLength counters QMK already collects, adding the guard QMK's ladder
// omits: a host that reads string descriptors two bytes at a time is in the Apple family, and can
// therefore never be Windows or the PS5-shaped "Linux".
//
// QMK tests cnt_ff/cnt_04 patterns first and never looks at cnt_02, so on a macOS host a single
// stray 4-byte control read flips its verdict to OS_WINDOWS, or to OS_LINUX via the PS5 branch if
// the trailing 0xFF has not landed yet. Both are positive, confident and wrong, and because the
// counters are cumulative and never reset they are permanent. cnt_02 is monotonic too, so once a
// host looks Apple it keeps looking Apple: no later packet can undo it.
//
// Checked against every sequence in qmk_firmware/quantum/os_detection/tests/os_detection.cpp:
// every macOS and iOS variant has cnt_02 >= 2, while Windows, Linux, Nintendo Switch and Quest 2
// all have cnt_02 == 0. PS5 (cnt_02 == 3) classifies as Apple, which is a host this board is
// never used on, and QMK's own mapping of PS5 to OS_LINUX is just as arbitrary.
static inline hd_host_class_t hd_classify_host(uint8_t count, uint8_t cnt_02, uint8_t cnt_04, uint8_t cnt_ff) {
    if (count < 3) {
        return HD_HOST_UNKNOWN;
    }
    if (cnt_02 >= 2) {
        return HD_HOST_APPLE;
    }
    if (cnt_ff >= 2 && cnt_04 >= 1) {
        return HD_HOST_PC; // Windows
    }
    if (count == cnt_ff) {
        return HD_HOST_PC; // Linux
    }
    return HD_HOST_UNKNOWN;
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

// Everything that can decide the Ctrl/GUI mapping, in precedence order.
typedef struct {
    bool         manual_set;     // an override key was pressed this session
    bool         manual_swap;    // ... and this is what it asked for
    bool         apple_latched;  // a positive macOS/iOS result was seen this session
    bool         detected_known; // a positive (non-OS_UNSURE) result was seen this session
    os_variant_t detected_os;    // ... and this was the most recent one
    bool         fallback_due;   // detection produced nothing positive before the delay expired
} hd_policy_inputs_t;

typedef enum {
    HD_SRC_INIT = 0,  // pre-detection default
    HD_SRC_DETECTED,  // a positive OS result
    HD_SRC_FALLBACK,  // the unsure-timeout default
    HD_SRC_MANUAL,    // the override key
} hd_policy_source_t;

// Manual override beats everything, then a latched Apple host, then the newest positive result,
// then the fallback. Before any of those, stay stock.
static inline bool hd_resolve_swap(hd_policy_inputs_t in) {
    if (in.manual_set) {
        return in.manual_swap;
    }
    if (in.apple_latched) {
        return true;
    }
    if (in.detected_known) {
        return hd_should_swap_ctrl_gui(in.detected_os);
    }
    if (in.fallback_due) {
        return HD_UNSURE_FALLBACK_SWAP;
    }
    return false;
}

// State transitions, kept pure so tests/policy_sim.c can drive the real logic against the real
// quantum/os_detection.c instead of a reimplementation that could drift from the firmware.
static inline void hd_policy_on_detection(hd_policy_inputs_t *in, os_variant_t os) {
    if (os != OS_UNSURE) {
        in->detected_known = true;
        in->detected_os    = os;
    }
    in->apple_latched = hd_apple_host_latch(in->apple_latched, os);
}

static inline void hd_policy_on_fallback(hd_policy_inputs_t *in) {
    in->fallback_due = true;
}

static inline void hd_policy_on_manual(hd_policy_inputs_t *in, bool want_swap) {
    in->manual_set  = true;
    in->manual_swap = want_swap;
}

static inline hd_policy_source_t hd_policy_source(hd_policy_inputs_t in) {
    if (in.manual_set) {
        return HD_SRC_MANUAL;
    }
    if (in.apple_latched || in.detected_known) {
        return HD_SRC_DETECTED;
    }
    if (in.fallback_due) {
        return HD_SRC_FALLBACK;
    }
    return HD_SRC_INIT;
}
