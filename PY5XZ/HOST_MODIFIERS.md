# Host modifier policy

The firmware owns the Voyager's Ctrl/GUI mapping.

| Detected host | Pointer-finger Ctrl positions | Ring-finger GUI positions |
|---|---|---|
| macOS or iOS | GUI / Command | Ctrl |
| Linux | Ctrl | GUI / Super |
| Windows | Ctrl | GUI |
| Unsure | Ctrl | GUI |

`LCTL(KC_SPC)` bypasses the swap and emits literal Ctrl+Space on every host.

The policy defaults to unswapped during keyboard initialization. QMK OS detection updates it after the detected result stabilizes. The result is RAM-only and is not written to EEPROM. Active HID state is cleared when the policy changes so a modifier held across detection cannot remain stuck. Housekeeping restores the selected policy if another runtime path changes the QMK swap flags.

## The Apple latch

Once macOS or iOS is detected, the swap holds for the rest of the power-on session. Later detection results cannot demote it; only a power cycle can, by way of `hd_keyboard_post_init()`. Non-Apple results are never latched, so an initial `OS_UNSURE` can still be refined up to the swapped policy.

This exists because QMK's OS detection reports a wrong result mid-session and never recovers. `setups_data` in `quantum/os_detection.c` accumulates for the whole power-on session and is never reset on re-enumeration, and the guess ladder tests the Windows pattern (`cnt_ff >= 2 && cnt_04 >= 1`) before the macOS pattern. Both counters only grow. A second macOS enumeration — a host sleep/wake or a hub renegotiation, neither of which cuts bus power — pushes `cnt_ff` to 2, after which a single 4-byte control read latches `detected_os` to `OS_WINDOWS` for good. Every host LED update calls `os_detection_notify_usb_device_state_change()` and re-arms the report window, so the bogus result reaches the callback promptly. Without the latch the board silently reverts to the stock mapping until it is unplugged.

`tests/os_detection_latch_test.c` pins this against the real `quantum/os_detection.c`. If it starts failing because ZSA fixed the counter reset upstream, the latch can be dropped.

Consequence to know: with a self-powered hub or KVM that keeps the board powered while the upstream host changes, a macOS-then-Linux switch keeps the macOS policy. Power-cycle the board when deliberately moving it between hosts that way, including between the two halves of the hardware acceptance check below.

Do not add a second Voyager-specific Ctrl/GUI remap in Karabiner, macOS keyboard settings, `hidutil`, or the Linux host. Two remaps can cancel or invert the firmware policy.

Run the focused source test for OS classification, the changed-policy guard, and the latch:

```sh
clang -std=c11 -Wall -Wextra -Werror \
  -I qmk_firmware/quantum \
  -I qmk_firmware/tmk_core/protocol \
  -I PY5XZ \
  tests/host_modifier_policy_test.c \
  -o /tmp/voyager-host-modifier-policy-test
/tmp/voyager-host-modifier-policy-test
```

Run the end-to-end policy test against the real detection code. It links `quantum/os_detection.c`, so it needs the timer stub in `tests/stub` and cannot use `-Werror` (upstream has unused-parameter warnings):

```sh
clang -std=c11 -Wall -Wextra \
  -I tests/stub \
  -I qmk_firmware/quantum \
  -I qmk_firmware/tmk_core/protocol \
  -I PY5XZ \
  tests/os_detection_latch_test.c qmk_firmware/quantum/os_detection.c \
  -o /tmp/voyager-os-detection-latch-test
/tmp/voyager-os-detection-latch-test
```

Deployment acceptance requires all of the following after a successful firmware build, `zapp` exit code 0, and USB re-enumeration:

- Linux: hold `T` for Ctrl; hold `S` for Super.
- macOS: hold `T` for Command; hold `S` for Control.
- Both hosts: Ctrl+Space remains literal Ctrl+Space.
- Releasing a modifier held during initial OS detection leaves no stuck Ctrl or GUI state.
- macOS: the swap still holds after a sleep/wake cycle and after several hours of normal use.

Do not describe a source change as deployed until both hosts pass these checks.
