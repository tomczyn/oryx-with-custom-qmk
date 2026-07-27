# Host modifier policy

The firmware owns the Voyager's Ctrl/GUI mapping.

| Detected host | Pointer-finger Ctrl positions | Ring-finger GUI positions |
|---|---|---|
| macOS or iOS | GUI / Command | Ctrl |
| Linux | Ctrl | GUI / Super |
| Windows | Ctrl | GUI |
| Unsure, first 3 s | Ctrl | GUI |
| Unsure, after 3 s | GUI / Command | Ctrl |

Precedence, highest first: **manual override**, then a **latched Apple** result, then the newest **positive** detection, then the **unsure fallback**, then stock. `hd_resolve_swap()` in `hd_host_os.h` is the single decision point.

`LCTL(KC_SPC)` bypasses the swap and emits literal Ctrl+Space on every host.

The policy defaults to unswapped during keyboard initialization. QMK OS detection updates it after the detected result stabilizes. The result is RAM-only and is not written to EEPROM. Active HID state is cleared when the policy changes so a modifier held across detection cannot remain stuck. Housekeeping restores the selected policy if another runtime path changes the QMK swap flags.

## The Apple latch

Once macOS or iOS is detected, the swap holds for the rest of the power-on session. Later detection results cannot demote it; only a power cycle can, by way of `hd_keyboard_post_init()`. Non-Apple results are never latched, so an initial `OS_UNSURE` can still be refined up to the swapped policy.

This exists because QMK's OS detection reports a wrong result mid-session and never recovers. `setups_data` in `quantum/os_detection.c` accumulates for the whole power-on session and is never reset on re-enumeration, and the guess ladder tests the Windows pattern (`cnt_ff >= 2 && cnt_04 >= 1`) before the macOS pattern. Both counters only grow. A second macOS enumeration — a host sleep/wake or a hub renegotiation, neither of which cuts bus power — pushes `cnt_ff` to 2, after which a single 4-byte control read latches `detected_os` to `OS_WINDOWS` for good. Every host LED update calls `os_detection_notify_usb_device_state_change()` and re-arms the report window, so the bogus result reaches the callback promptly. Without the latch the board silently reverts to the stock mapping until it is unplugged.

`tests/os_detection_latch_test.c` pins this against the real `quantum/os_detection.c`. If it starts failing because ZSA fixed the counter reset upstream, the latch can be dropped.

Consequence to know: with a self-powered hub or KVM that keeps the board powered while the upstream host changes, a macOS-then-Linux switch keeps the macOS policy. Power-cycle the board when deliberately moving it between hosts that way, including between the two halves of the hardware acceptance check below.

## The unsure fallback

The latch only helps once macOS has been detected at least once. A separate failure leaves the board stock on a macOS host: detection never produces a positive result at all. `OS_MACOS` needs 5+ setup packets ending in `0xFF`, and that trailing `0xFF` comes from the HID report-descriptor read. If the host's enumeration is abbreviated — observed after a DFU flash, where macOS has just seen the device — detection sits on `OS_UNSURE` for the whole session, and treating `OS_UNSURE` as stock means the swap never arms.

So `HD_UNSURE_FALLBACK_SWAP` (default on) applies the macOS policy if nothing positive has been detected `HD_HOST_FALLBACK_MS` (default 3000) after the host configures the device. This is **not** latched, so a positive `OS_LINUX`/`OS_WINDOWS` result still overrides it. That is safe because the Linux pattern is 3 packets of `0xFF` and is recognised long inside the delay, whereas the macOS pattern is the fragile one. It supersedes the earlier "unsure fails safe to stock" rule, which is what left the board stock on its primary host.

This is a deliberate reversal of `de22139`'s `OS_UNSURE` handling, narrower than `032c6c9`: it fires only when detection produced nothing positive, and it is revocable by any later positive result.

## Manual override

`CG_TOGG`, `CG_SWAP` and `CG_NORM` are intercepted in `hd_process_host_override()` and set the policy directly, outranking detection, the latch and the fallback for the rest of the session. Place any of them in Oryx to get a deterministic escape hatch.

The interception is required, not cosmetic: QMK's own handler writes these to EEPROM-backed `keymap_config`, which housekeeping would revert on the next scan, so an un-intercepted `CG_TOGG` would appear to do nothing. `process_record_kb` runs before `process_magic` in the `process_record_quantum` chain, so returning false pre-empts the EEPROM write. The override is RAM-only: a replug returns to automatic behaviour.

## Indicator

LED `HD_INDICATOR_LED` (0) reports the policy for `HD_INDICATOR_MS` (4000) after any change, then releases the key back to normal lighting. Set `HD_INDICATOR_MS` to 0 to keep it lit permanently. It uses `rgb_matrix_indicators_advanced_user` because Oryx's generated keymap owns `rgb_matrix_indicators_user`; the advanced hook renders later, so it wins.

| Colour | Meaning |
|---|---|
| Blue | macOS/iOS detected — swapped |
| Orange | host positively detected as non-Apple — stock |
| Cyan | nothing detected, fallback applied — swapped |
| White | nothing detected, fallback resolved to stock |
| Green | manual override — swapped |
| Red | manual override — stock |

A silent mid-session revert is no longer silent: the indicator fires on every policy change, so the failure reports itself instead of needing a packet-level investigation.

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
- macOS: the swap is live within a few seconds of a DFU flash, without a replug.
- If an override key is placed: it forces both directions and survives later detection reports.

Do not describe a source change as deployed until both hosts pass these checks.
