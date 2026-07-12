# Contributing to RF Rosetta

Thanks for wanting to help. This project runs almost entirely on protocol
signatures — the more of those the community adds, the more useful it gets
for everyone. Code contributions are welcome too.

## Ways to help

- **Add a protocol signature** — by far the most valuable contribution
- **Report an unidentified signal** — even without a fix, this grows the database
- **Fix a bug** — see [Known SDK Quirks](#known-sdk-quirks) before you start
- **ESP32/Marauder integration** — the one big feature still missing
- **Improve documentation**

## Adding a protocol signature

Open `protocol_db.c` and add an entry to the `DB[]` array. Every field is
required:

```c
{
    .name             = "Full Device Name",
    .short_name       = "Short Name",       // ≤16 chars, shown on small screens
    .category         = CategoryHome,       // see ProtocolCategory in protocol_db.h
    .freq_min         = 433050000,          // Hz
    .freq_max         = 433920000,          // Hz
    .modulation       = ModulationOOK,      // see Modulation in protocol_db.h
    .pulse_min        = 300,                // µs
    .pulse_max        = 900,                // µs
    .bandwidth_khz    = 40,
    .repeating        = true,
    .repeat_min       = 2,
    .repeat_max       = 5,                  // 0 = uncapped
    .fixed_code       = false,
    .rolling_code     = true,
    .encrypted        = false,
    .security_concern = false,              // true shows a warning banner
    .needs_esp32      = false,
    .brands           = "Brand A, Brand B",
    .description      = "One plain-English sentence about what this device does.",
    .security_note    = "One sentence on the security implications.",
    .extra_data       = "What fields could theoretically be decoded.",
    .confidence_bonus = 3,                  // 0-10, higher for very distinctive signals
},
```

### Where to get the numbers

- **Frequency / pulse timing**: capture the real signal with RF Rosetta (or
  `rtl_433`, or a Flipper's built-in Sub-GHz Read RAW) and read the values off
  the capture
- **Category / modulation**: pick the closest match from the enums in
  `protocol_db.h` — don't add new enum values unless genuinely necessary
- **`confidence_bonus`**: leave at 0-2 for generic/common signal shapes, use
  3-8 for protocols with a very distinctive signature (unusual frequency,
  unusual pulse pattern) that's unlikely to false-positive against something else

### Adding a live decoder

If you can work out the actual bit layout (not just "this is an X"), you can
add real value extraction to `protocol_db_decode()` in `protocol_db.c`. Look
at the existing TPMS/weather/meter decoders for the pattern — they extract
bytes from `cap->raw_pulses[]` using a PWM threshold against `cap->pulse_avg`
and then interpret the resulting bytes.

Please be conservative — a wrong decode is worse than no decode. Gate live
decoders behind as many frequency/modulation/pulse-width checks as you can, so
they don't misfire on a different protocol that happens to share a frequency
band.

## Reporting an unidentified signal

Open an issue with:

- Frequency (MHz)
- Modulation if known (or "unknown")
- What device produced it, if known
- The **Details** screen output from RF Rosetta (RSSI, pulse count, bandwidth, etc.)
- A raw pulse dump if you can get one (Flipper's Sub-GHz "Read RAW" or the
  `.sub` export from RF Rosetta's result screen)

Even if nobody gets to writing a decoder, the raw data is useful for building
a signature later.

## Building from source

```bash
pip install ufbt
git clone https://github.com/joelewis012/rf_rosetta
cd rf_rosetta
ufbt
```

The `.fap` lands in `dist/`. Copy it to `SD:/apps/Sub-GHz/` on your Flipper,
or use `ufbt launch` with the Flipper connected via USB.

## Known SDK quirks

These cost real time to discover — save yourself the trouble:

- **`furi_hal_subghz_load_preset()` does not exist** in this SDK. Use
  `furi_hal_subghz_load_custom_preset()` with a raw `{addr, val, ...}` register
  array instead. See the presets in `signal_capture.c` for examples.
- **`canvas_draw_triangle()` does not exist.** Draw triangles manually with
  three `canvas_draw_line()` calls.
- **`subghz_devices_*` functions don't link** in the FAP SDK, even though the
  headers compile fine. If you need external CC1101 access, use the bit-bang
  SPI driver in `cc1101_ext.c` instead — it talks to the chip directly over
  GPIO rather than through the firmware's device abstraction.
- **`-Werror=double-promotion`** will fail your build on any bare `1.5` style
  float literal in an expression involving `float` variables. Use `1.5f`, or
  better, avoid floating point in hot paths entirely (see how
  `cc1101_ext_set_frequency()` uses `uint64_t` fixed-point math instead).
- **`view_set_input_callback()`** expects a `bool`-returning function
  (`true` = event consumed). A `void`-returning callback will fail to link
  with an "incompatible pointer type" error.
- **View model allocation** (`view_allocate_model`) must happen exactly once,
  in `rf_rosetta_alloc()`, not in each scene's `on_enter`. Doing it in
  `on_enter` causes a `furi_check` crash the second time you enter that scene.
- Any `view_dispatcher_add_view()` call must come **after**
  `view_dispatcher_alloc()`. Order matters in `rf_rosetta_alloc()`.

If in doubt, grep the existing code for a similar pattern before introducing a
new API call — most of the "obvious" Flipper HAL functions have at least one
gotcha in this particular SDK version.

## Before submitting a PR

- Test on real hardware if you can — the Flipper's `furi_check` crashes don't
  always show up as compile errors
- Keep changes focused — one feature or fix per PR is easier to review
- For anything larger than a protocol addition or small bugfix, please open an
  issue first so we can agree on the approach before you put the work in

## Code of Conduct

This project follows a [Code of Conduct](CODE_OF_CONDUCT.md). By
participating you're agreeing to abide by it.
