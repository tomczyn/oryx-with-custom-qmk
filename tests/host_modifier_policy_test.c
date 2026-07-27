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
