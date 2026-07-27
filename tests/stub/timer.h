// Host-build stub for QMK's platforms/timer.h. Only what quantum/os_detection.c uses, with a
// manually advanced clock so a test can step through debounce windows deterministically.
#pragma once
#include <stdint.h>

typedef uint32_t fast_timer_t;
extern uint32_t hd_test_now_ms;

static inline fast_timer_t timer_read_fast(void) {
    return hd_test_now_ms;
}
static inline fast_timer_t timer_elapsed_fast(fast_timer_t last) {
    return hd_test_now_ms - last;
}
