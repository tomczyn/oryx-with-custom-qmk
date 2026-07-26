# Custom Voyager firmware agent rules

The active custom keymap is `PY5XZ/`. Read `PY5XZ/HOST_MODIFIERS.md` before changing Ctrl/GUI behavior.

- Source, committed source, successful builds, flashed firmware, and observed board behavior are separate states. Never describe one as another.
- The host policy is macOS/iOS swapped; Linux/Windows/unsure stock; Ctrl+Space literal on every host.
- Keep OS-derived swap state RAM-only. Do not persist it to EEPROM.
- Preserve `hd_keyboard_post_init()` in Oryx-generated `PY5XZ/keymap.c`; it prevents stale EEPROM swap flags from affecting the first scan.
- Clear active HID state when the policy changes, and do not clear it for repeated identical OS reports.
- Run `tests/host_modifier_policy_test.c`, compile the complete firmware, and complete the two-host acceptance matrix before marking a change deployed.
- Do not push, dispatch GitHub Actions, or flash without explicit user authorization.
- Never use `zapp update`; it replaces the custom build with pure Oryx firmware.
