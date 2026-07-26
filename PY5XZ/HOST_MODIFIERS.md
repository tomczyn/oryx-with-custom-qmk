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

Do not add a second Voyager-specific Ctrl/GUI remap in Karabiner, macOS keyboard settings, `hidutil`, or the Linux host. Two remaps can cancel or invert the firmware policy.

Run the focused source test for OS classification and the changed-policy guard:

```sh
clang -std=c11 -Wall -Wextra -Werror \
  -I qmk_firmware/quantum \
  -I qmk_firmware/tmk_core/protocol \
  -I PY5XZ \
  tests/host_modifier_policy_test.c \
  -o /tmp/voyager-host-modifier-policy-test
/tmp/voyager-host-modifier-policy-test
```

Deployment acceptance requires all of the following after a successful firmware build, `zapp` exit code 0, and USB re-enumeration:

- Linux: hold `T` for Ctrl; hold `S` for Super.
- macOS: hold `T` for Command; hold `S` for Control.
- Both hosts: Ctrl+Space remains literal Ctrl+Space.
- Releasing a modifier held during initial OS detection leaves no stuck Ctrl or GUI state.

Do not describe a source change as deployed until both hosts pass these checks.
