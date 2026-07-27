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
