#pragma once
#include "os_detection.h"

static inline bool hd_should_swap_ctrl_gui(os_variant_t os) {
    return os == OS_MACOS || os == OS_IOS;
}

static inline bool hd_ctrl_gui_swap_needs_update(bool left, bool right, bool desired) {
    return left != desired || right != desired;
}
