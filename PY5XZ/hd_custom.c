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
#include "usb_device_state.h"
#include "hd_setups_data.h"

// How long to wait after the host configures the device before assuming the primary host. QMK's
// macOS pattern needs 5+ setup packets ending in 0xFF; if the HID descriptor read that supplies
// that 0xFF is late or absent, detection sits on OS_UNSURE forever and the board stays stock.
#ifndef HD_HOST_FALLBACK_MS
#    define HD_HOST_FALLBACK_MS 3000
#endif

// How long LED 0 reports the policy after it changes. 0 keeps it lit permanently.
#ifndef HD_INDICATOR_MS
#    define HD_INDICATOR_MS 4000
#endif

#ifndef HD_INDICATOR_LED
#    define HD_INDICATOR_LED 0
#endif

static bool               hd_swap_ctrl_gui = false;
static hd_policy_inputs_t hd_policy        = {0};
static hd_policy_source_t hd_policy_src    = HD_SRC_INIT;

static volatile bool hd_usb_configured = false;
static bool          hd_fallback_armed = false;
static uint32_t      hd_configured_at  = 0;
static uint32_t      hd_changed_at     = 0;
static bool          hd_ever_changed   = false;

// Recompute the policy from the current inputs and push it into QMK. Called from the detection
// callback, the override key, and housekeeping, so a drift in the QMK flags is corrected too.
static void hd_refresh_policy(void) {
    hd_swap_ctrl_gui = hd_resolve_swap(hd_policy);
    hd_policy_src    = hd_policy_source(hd_policy);

    if (!hd_ctrl_gui_swap_needs_update(
            keymap_config.swap_lctl_lgui,
            keymap_config.swap_rctl_rgui,
            hd_swap_ctrl_gui)) {
        return;
    }
    keymap_config.swap_lctl_lgui = hd_swap_ctrl_gui;
    keymap_config.swap_rctl_rgui = hd_swap_ctrl_gui;
    clear_keyboard();
    hd_changed_at   = timer_read32();
    hd_ever_changed = true;
}

void hd_keyboard_post_init(void) {
    hd_swap_ctrl_gui  = false;
    hd_policy         = (hd_policy_inputs_t){0};
    hd_policy_src     = HD_SRC_INIT;
    hd_fallback_armed = false;
    hd_ever_changed   = false;
    keymap_config.swap_lctl_lgui = false;
    keymap_config.swap_rctl_rgui = false;
}

// Runs in USB callback context, so only set a flag; housekeeping stamps the timer.
void notify_usb_device_state_change_user(struct usb_device_state usb_device_state) {
    hd_usb_configured = usb_device_state.configure_state == USB_DEVICE_STATE_CONFIGURED;
}

// QMK's verdict is deliberately NOT used to set the policy. On this host it is positively wrong
// whenever a stray 4-byte control read lands (OS_WINDOWS, or OS_LINUX via the PS5 branch), and
// because its counters are cumulative that verdict is permanent. hd_classify_host() reads the same
// counters correctly, so housekeeping owns the decision. Keeping this override documents that the
// callback is intentionally inert rather than merely unimplemented.
bool process_detected_host_os_user(os_variant_t os) {
    (void)os;
    return true;
}

// Consume QMK's Ctrl/GUI magic keycodes so they set our policy instead of QMK's EEPROM-backed
// flags, which housekeeping would otherwise revert on the next scan. Place any of CG_TOGG /
// CG_SWAP / CG_NORM in Oryx to get a manual override; RAM-only, so a replug returns to auto.
static bool hd_process_host_override(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case QK_MAGIC_TOGGLE_CTL_GUI:
        case QK_MAGIC_SWAP_CTL_GUI:
        case QK_MAGIC_UNSWAP_CTL_GUI:
            break;
        default:
            return true;
    }
    if (!record->event.pressed) {
        return false;
    }
    switch (keycode) {
        case QK_MAGIC_SWAP_CTL_GUI:
            hd_policy_on_manual(&hd_policy, true);
            break;
        case QK_MAGIC_UNSWAP_CTL_GUI:
            hd_policy_on_manual(&hd_policy, false);
            break;
        default:
            hd_policy_on_manual(&hd_policy, !hd_swap_ctrl_gui);
            break;
    }
    hd_refresh_policy();
    // Always re-arm the indicator, so pressing the key confirms the policy even when unchanged.
    hd_changed_at   = timer_read32();
    hd_ever_changed = true;
    return false;
}

void housekeeping_task_user(void) {
    // Our own classification of QMK's counters is the primary signal, because QMK's verdict is
    // positively wrong on this host whenever a stray 4-byte control read lands. Feed it in as a
    // synthetic detection result; it outranks nothing, it just supplies a trustworthy one.
    switch (hd_current_host_class()) {
        case HD_HOST_APPLE:
            hd_policy_on_detection(&hd_policy, OS_MACOS);
            break;
        case HD_HOST_PC:
            hd_policy_on_detection(&hd_policy, OS_LINUX);
            break;
        case HD_HOST_UNKNOWN:
            break;
    }

    if (hd_usb_configured) {
        if (!hd_fallback_armed) {
            hd_fallback_armed = true;
            hd_configured_at  = timer_read32();
        } else if (!hd_policy.fallback_due && !hd_policy.detected_known &&
                   timer_elapsed32(hd_configured_at) >= HD_HOST_FALLBACK_MS) {
            hd_policy_on_fallback(&hd_policy);
        }
    }
    hd_refresh_policy();
}

#    ifdef RGB_MATRIX_ENABLE
// Oryx owns rgb_matrix_indicators_user, so use the advanced hook: it renders afterwards and wins.
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (!hd_ever_changed) {
        return true;
    }
#        if HD_INDICATOR_MS > 0
    if (timer_elapsed32(hd_changed_at) >= HD_INDICATOR_MS) {
        return true;
    }
#        endif
    if (HD_INDICATOR_LED < led_min || HD_INDICATOR_LED >= led_max) {
        return true;
    }
    // blue   = macOS/iOS detected, swapped      orange = host detected as non-Apple, stock
    // cyan   = nothing detected, fallback swap  green  = manual override, swapped
    // red    = manual override, stock           white  = fallback resolved to stock
    uint8_t r = 255, g = 255, b = 255;
    if (hd_policy_src == HD_SRC_MANUAL) {
        r = hd_swap_ctrl_gui ? 0 : 255;
        g = hd_swap_ctrl_gui ? 255 : 0;
        b = 0;
    } else if (hd_policy_src == HD_SRC_DETECTED) {
        r = hd_swap_ctrl_gui ? 0 : 255;
        g = hd_swap_ctrl_gui ? 0 : 120;
        b = hd_swap_ctrl_gui ? 255 : 0;
    } else if (hd_policy_src == HD_SRC_FALLBACK && hd_swap_ctrl_gui) {
        r = 0;
        g = 255;
        b = 255;
    }
    rgb_matrix_set_color(HD_INDICATOR_LED, r, g, b);
    return true;
}
#    endif
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
#ifdef OS_DETECTION_ENABLE
    if (!hd_process_host_override(keycode, record)) {
        return false;
    }
#endif

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
