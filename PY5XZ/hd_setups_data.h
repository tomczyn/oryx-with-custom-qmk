#pragma once
#include <stdint.h>
#include "hd_host_os.h"

// quantum/os_detection.c collects the USB setup wLength counters we need and, unlike its verdict,
// the counters themselves are correct. The struct is defined in that .c file but `setups_data`
// has external linkage, so redeclare the layout and read it directly. This is the only way to get
// at the data without patching the pinned zsa/qmk_firmware submodule, which the build Action
// re-checks-out on every run.
//
// Fragile by nature: if upstream changes the struct, this reads the wrong bytes. The static
// assertion below catches added or reordered fields at compile time, and tests/policy_sim.c links
// the real os_detection.c so a behavioural drift shows up there too.
struct setups_data_t {
    uint8_t  count;
    uint8_t  cnt_02;
    uint8_t  cnt_04;
    uint8_t  cnt_ff;
    uint16_t last_wlength;
};

_Static_assert(sizeof(struct setups_data_t) == 6, "os_detection.c setups_data_t layout changed");

extern struct setups_data_t setups_data;

static inline hd_host_class_t hd_current_host_class(void) {
    return hd_classify_host(setups_data.count, setups_data.cnt_02, setups_data.cnt_04, setups_data.cnt_ff);
}
