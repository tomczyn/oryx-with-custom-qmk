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
    return 0;
}
