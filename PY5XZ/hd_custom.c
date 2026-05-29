#include QMK_KEYBOARD_H

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
