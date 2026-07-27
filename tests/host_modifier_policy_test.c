#include <assert.h>
#include "hd_host_os.h"

int main(void) {
    assert(!hd_should_swap_ctrl_gui(OS_UNSURE));
    assert(!hd_should_swap_ctrl_gui(OS_LINUX));
    assert(!hd_should_swap_ctrl_gui(OS_WINDOWS));
    assert(hd_should_swap_ctrl_gui(OS_MACOS));
    assert(hd_should_swap_ctrl_gui(OS_IOS));

    assert(!hd_ctrl_gui_swap_needs_update(false, false, false));
    assert(hd_ctrl_gui_swap_needs_update(true, true, false));
    assert(hd_ctrl_gui_swap_needs_update(false, true, false));
    assert(hd_ctrl_gui_swap_needs_update(false, false, true));
    assert(!hd_ctrl_gui_swap_needs_update(true, true, true));
    assert(hd_ctrl_gui_swap_needs_update(true, false, true));

    // Unlatched: every non-Apple result stays stock, so an OS_UNSURE first report can still be
    // refined upward to the swapped policy.
    assert(!hd_apple_host_latch(false, OS_UNSURE));
    assert(!hd_apple_host_latch(false, OS_LINUX));
    assert(!hd_apple_host_latch(false, OS_WINDOWS));
    assert(hd_apple_host_latch(false, OS_MACOS));
    assert(hd_apple_host_latch(false, OS_IOS));

    // Latched: no later re-report may demote a confirmed Apple host. This is what keeps the
    // swap alive when QMK's cumulative wLength counters decay detected_os to OS_WINDOWS
    // mid-session.
    assert(hd_apple_host_latch(true, OS_UNSURE));
    assert(hd_apple_host_latch(true, OS_LINUX));
    assert(hd_apple_host_latch(true, OS_WINDOWS));
    assert(hd_apple_host_latch(true, OS_MACOS));
    assert(hd_apple_host_latch(true, OS_IOS));

    // hd_classify_host: counts are (count, cnt_02, cnt_04, cnt_ff).
    // Too little data to decide.
    assert(hd_classify_host(0, 0, 0, 0) == HD_HOST_UNKNOWN);
    assert(hd_classify_host(2, 2, 0, 0) == HD_HOST_UNKNOWN);

    // Every macOS/iOS sequence in QMK's collected data has cnt_02 >= 2. These are the counter
    // totals for macOS 15.x {02,4E,02,1C,02,1A,FF,FF} and iOS {02,24,02,28}.
    assert(hd_classify_host(8, 3, 0, 2) == HD_HOST_APPLE);
    assert(hd_classify_host(4, 2, 0, 0) == HD_HOST_APPLE);

    // The whole point: a stray 4-byte read cannot move an Apple host to PC, at any counter total.
    // QMK calls the first of these OS_WINDOWS and the second OS_LINUX via the PS5 branch.
    assert(hd_classify_host(9, 3, 1, 2) == HD_HOST_APPLE);
    assert(hd_classify_host(7, 3, 1, 0) == HD_HOST_APPLE);

    // Real PC hosts never send 2-byte string reads, so they still classify as PC.
    assert(hd_classify_host(3, 0, 0, 3) == HD_HOST_PC);  // Linux {FF,FF,FF}
    assert(hd_classify_host(6, 0, 0, 6) == HD_HOST_PC);  // Linux, longer
    assert(hd_classify_host(6, 0, 2, 2) == HD_HOST_PC);  // Windows {FF,FF,04,24,04,24}
    assert(hd_classify_host(5, 0, 1, 3) == HD_HOST_PC);  // V-USB Windows {FF,FF,04,0E,FF}
    assert(hd_classify_host(4, 0, 0, 4) == HD_HOST_PC);  // Quest 2 {FF,FF,FF,FE} -> counted as FF*3

    // Nothing decided yet -> stock, and no indicator claim.
    hd_policy_inputs_t fresh = {0};
    assert(!hd_resolve_swap(fresh));
    assert(hd_policy_source(fresh) == HD_SRC_INIT);

    // A positive Apple result swaps and latches.
    hd_policy_inputs_t mac = {.detected_known = true, .detected_os = OS_MACOS, .apple_latched = true};
    assert(hd_resolve_swap(mac));
    assert(hd_policy_source(mac) == HD_SRC_DETECTED);

    // The QMK decay case: latched Apple, newest positive result is the bogus OS_WINDOWS.
    hd_policy_inputs_t decayed = {.detected_known = true, .detected_os = OS_WINDOWS, .apple_latched = true};
    assert(hd_resolve_swap(decayed));
    assert(hd_policy_source(decayed) == HD_SRC_DETECTED);

    // A positively detected Linux host stays stock and is never latched.
    hd_policy_inputs_t linux_host = {.detected_known = true, .detected_os = OS_LINUX};
    assert(!hd_resolve_swap(linux_host));
    assert(hd_policy_source(linux_host) == HD_SRC_DETECTED);

    // The stall this fixes: detection produced nothing positive, so the fallback decides. A
    // positive Linux result arriving later still wins, because it is not latched.
    hd_policy_inputs_t stalled = {.fallback_due = true};
    assert(hd_resolve_swap(stalled) == HD_UNSURE_FALLBACK_SWAP);
    assert(hd_policy_source(stalled) == HD_SRC_FALLBACK);
    hd_policy_inputs_t stalled_then_linux = {.fallback_due = true, .detected_known = true, .detected_os = OS_LINUX};
    assert(!hd_resolve_swap(stalled_then_linux));
    assert(hd_policy_source(stalled_then_linux) == HD_SRC_DETECTED);

    // Manual override beats detection, the latch, and the fallback, in both directions.
    hd_policy_inputs_t forced_stock = {
        .manual_set = true, .manual_swap = false, .apple_latched = true, .detected_known = true, .detected_os = OS_MACOS};
    assert(!hd_resolve_swap(forced_stock));
    assert(hd_policy_source(forced_stock) == HD_SRC_MANUAL);
    hd_policy_inputs_t forced_swap = {
        .manual_set = true, .manual_swap = true, .detected_known = true, .detected_os = OS_LINUX, .fallback_due = true};
    assert(hd_resolve_swap(forced_swap));
    assert(hd_policy_source(forced_swap) == HD_SRC_MANUAL);
    return 0;
}
