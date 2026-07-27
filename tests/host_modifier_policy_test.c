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
    return 0;
}
