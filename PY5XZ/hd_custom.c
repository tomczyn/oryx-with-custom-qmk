#include QMK_KEYBOARD_H
#include "hd_custom.h"
#include "hd_host_os.h"

#ifndef ADAPTIVE_TERM
#define ADAPTIVE_TERM 45
#endif

#ifndef COMMA_SHIFT_TERM
#define COMMA_SHIFT_TERM 150
#endif

#ifdef OS_DETECTION_ENABLE
static bool hd_swap_ctrl_gui = false;

static void hd_apply_ctrl_gui_swap(void) {
    if (!hd_ctrl_gui_swap_needs_update(
            keymap_config.swap_lctl_lgui,
            keymap_config.swap_rctl_rgui,
            hd_swap_ctrl_gui)) {
        return;
    }
    keymap_config.swap_lctl_lgui = hd_swap_ctrl_gui;
    keymap_config.swap_rctl_rgui = hd_swap_ctrl_gui;
    clear_keyboard();
}

void hd_keyboard_post_init(void) {
    hd_swap_ctrl_gui = false;
    keymap_config.swap_lctl_lgui = false;
    keymap_config.swap_rctl_rgui = false;
}

bool process_detected_host_os_user(os_variant_t os) {
    hd_swap_ctrl_gui = hd_should_swap_ctrl_gui(os);
    hd_apply_ctrl_gui_swap();
    return true;
}

void housekeeping_task_user(void) {
    hd_apply_ctrl_gui_swap();
}
#endif

bool is_flow_tap_key(uint16_t keycode) {
    if (get_mods() & (MOD_MASK_CTRL | MOD_MASK_ALT | MOD_MASK_GUI)) {
        return false;
    }
    if (IS_QK_LAYER_TAP(keycode)) {
        return false;
    }
    uint16_t tap_kc = IS_QK_MOD_TAP(keycode) ? QK_MOD_TAP_GET_TAP_KEYCODE(keycode) : keycode;
    switch (tap_kc) {
        case KC_A ... KC_Z:
        case KC_DOT:
        case KC_COMMA:
        case KC_SLASH:
            return true;
    }
    return false;
}

uint16_t get_flow_tap_term(uint16_t keycode, keyrecord_t *record, uint16_t prev_keycode) {
    if (IS_QK_LAYER_TAP(keycode) || IS_QK_LAYER_TAP(prev_keycode)) {
        return 0;
    }
    if (is_flow_tap_key(keycode) && is_flow_tap_key(prev_keycode)) {
        return FLOW_TAP_TERM;
    }
    return 0;
}

static uint16_t hd_prior_keycode = KC_NO;
static uint16_t hd_prior_time = 0;

static void hd_emit(uint16_t kc) {
    bool shifted = (get_mods() | get_oneshot_mods()) & MOD_MASK_SHIFT;
#ifdef CAPS_WORD_ENABLE
    if (is_caps_word_on()) {
        shifted = true;
    }
#endif
    if (shifted) {
        register_code(KC_LSFT);
        tap_code(kc);
        unregister_code(KC_LSFT);
    } else {
        tap_code(kc);
    }
}

bool hd_process_adaptive(uint16_t keycode, keyrecord_t *record) {
    if (keycode == LCTL(KC_SPC)) {
        if (record->event.pressed) {
            hd_prior_keycode = KC_NO;
            register_mods(MOD_BIT(KC_LCTL));
            register_code(KC_SPC);
        } else {
            unregister_code(KC_SPC);
            unregister_mods(MOD_BIT(KC_LCTL));
        }
        return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    uint16_t kc;
    if (IS_QK_MOD_TAP(keycode) || IS_QK_LAYER_TAP(keycode)) {
        if (record->tap.count == 0) {
            hd_prior_keycode = KC_NO;
            return true;
        }
        kc = IS_QK_MOD_TAP(keycode) ? QK_MOD_TAP_GET_TAP_KEYCODE(keycode)
                                    : QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
    } else {
        kc = keycode;
    }

    if (kc >= KC_A && kc <= KC_Z
        && hd_prior_keycode == KC_COMMA
        && timer_elapsed(hd_prior_time) < COMMA_SHIFT_TERM
        && !(get_mods() & MOD_MASK_SHIFT)
#ifdef CAPS_WORD_ENABLE
        && !is_caps_word_on()
#endif
    ) {
        tap_code(KC_BSPC);
        register_code(KC_LSFT);
        tap_code(kc);
        unregister_code(KC_LSFT);
        hd_prior_keycode = KC_NO;
        return false;
    }

    if (kc == KC_H && timer_elapsed(hd_prior_time) < ADAPTIVE_TERM) {
        uint16_t repl = KC_NO;
        switch (hd_prior_keycode) {
            case KC_U: repl = KC_A; break;
            case KC_E: repl = KC_O; break;
            case KC_O: repl = KC_E; break;
            case KC_A: repl = KC_U; break;
        }
        if (repl != KC_NO) {
            hd_emit(repl);
            hd_prior_keycode = repl;
            hd_prior_time = timer_read();
            return false;
        }
    }

    hd_prior_keycode = kc;
    hd_prior_time = timer_read();
    return true;
}
