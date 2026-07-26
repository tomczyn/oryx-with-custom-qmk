#pragma once
#include "quantum.h"

void hd_keyboard_post_init(void);
bool hd_process_adaptive(uint16_t keycode, keyrecord_t *record);
