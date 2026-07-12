<div align="center">

<img src="assets/logo.svg" alt="RF Rosetta" width="700">

# RF Rosetta

**The first universal RF signal identifier for the Flipper Zero.**

Point your Flipper at any unknown wireless signal and find out exactly what it is —
car key, tyre sensor, alarm, smart meter, medical device, weather station, or one of
120+ other identified protocols. In seconds.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-FAP-orange)](https://flipperzero.one)
[![Version](https://img.shields.io/badge/version-1.0-green)]()

---

### If RF Rosetta has been useful, consider buying me a coffee ☕

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-Support%20this%20project-yellow?logo=buy-me-a-coffee)](https://buymeacoffee.com/Joelewis012)

---

</div>

## What is RF Rosetta?

RF Rosetta is a **passive RF signal identification tool** for the Flipper Zero. It
listens across the Sub-GHz spectrum, captures any signal that rises above the noise
floor, analyses its physical characteristics — frequency, modulation, pulse timing,
repetition pattern, bandwidth — and matches them against a database of 50+ known
wireless protocols.

It then tells you not just *what it is*, but everything meaningful about it: what
device made it, what data it contains, whether it's encrypted, whether it has known
security vulnerabilities, and what could theoretically be decoded from it.

No external app required. No cloud. Everything runs on the Flipper.

---

## How it works

```
  LISTENING              CAUGHT               ANALYSING              IDENTIFIED
  ─────────              ──────               ─────────              ──────────
  Sweeping               Signal               Frequency ✓            Car Key Fob
  433MHz...              burst                Modulation ✓           KeeLoq Rolling
                         detected             Pulse width ✓          87% confidence
  ≈≈≈≈≈≈≈≈≈≈                                 Repetition ✓
                                              Bandwidth  ✓           Security: Safe
                                              Matching...            Rolling code ✓
```

1. **Sweep** — scans across all major ISM bands (315, 433, 868, 915 MHz and more)
2. **Catch** — detects when a signal rises above the configurable noise threshold
3. **Analyse** — extracts frequency, modulation, pulse timing, bandwidth, and repetition
4. **Match** — scores the signal against every entry in the protocol database
5. **Report** — displays the result with confidence rating, security notes, and decodable data

---

## What it can identify

| Category | Protocols |
|---|---|
| 🚗 **Automotive** | Car key fobs (fixed & rolling / KeeLoq), TPMS tyre pressure sensors (315 & 433 MHz), keyless entry |
| 🏠 **Home** | Wireless doorbells, garage/gate remotes (fixed & rolling code), mains socket remotes, motorised blinds, RF light switches, wireless thermostats |
| 🔒 **Security** | PIR motion sensors, door/window contact sensors, panic buttons, smoke/CO detectors, wireless sirens, alarm keyfobs |
| 🌦 **Weather** | Weather stations (433 & 868 MHz), soil moisture sensors, rain gauges, anemometers |
| 📡 **IoT** | Z-Wave devices (EU 868 MHz & US 908 MHz), LoRa nodes (EU 868 MHz & US 915 MHz) |
| ⚡ **Utility** | Smart electricity meters AMR (433 & 868 MHz), gas meters, water meters |
| 🏭 **Industrial** | SCADA field sensors, asset tracking beacons, ISM band nodes |
| 💊 **Medical** | Dexcom CGM, Abbott FreeStyle Libre, hearing aid sync signals, personal medical alerts |
| 🎮 **Consumer** | Wireless headphones pairing, game controller sync, wireless keyboards/mice (MouseJack) |
| 🎈 **Misc** | Baby monitors, radiosondes (weather balloons), RC vehicle remotes, pool/spa sensors, wildlife tracking tags |

---

## What it tells you

For every identified signal, RF Rosetta provides:

**Signal measurements**
- Exact frequency in MHz
- RSSI (signal strength in dBm) with real-time trend (▲ getting closer / ▼ moving away)
- Noise floor and SNR at time of capture
- Modulation type (OOK, FSK, GFSK, LoRa/CSS, FM)
- Channel bandwidth, pulse width, estimated bit count, repetition count

**Protocol details**
- Full protocol name, category, and confidence score (0–100%)
- Known brands and devices that use this protocol
- Plain-English description of what the device does

**Security analysis**
- Fixed code vs rolling code — tells you if a signal is replay-vulnerable
- Encryption status and known vulnerability flags (MouseJack, relay attack, etc.)
- Plain-language security note for every protocol

**Signal fingerprinting**
- Detects if the same device transmits again in the same session
- `[x2]` badge shows repeat count — useful for confirming you're tracking the right signal

---

## Scan Modes

| Mode | Preset | Best For |
|---|---|---|
| **All (default)** | Cycles OOK → FSK-N → FSK-W | Unknown signals — catches everything |
| **OOK only** | OOK 650kHz BW | Remotes, doorbells, garage doors, sensors |
| **FSK Narrow** | 2-FSK 238kHz deviation | TPMS, weather stations, utility meters |
| **FSK Wide** | 2-FSK 476kHz deviation | Industrial sensors, pagers, unknown FSK |

**Sweep speed** — on the scanning screen, Left/Right arrows adjust dwell time per frequency
(shown bottom-right, e.g. `300ms`). Slower = more sensitive. Faster = wider coverage.

---

## NRF24 2.4 GHz Scanner

RF Rosetta includes a built-in **NRF24L01 channel scanner** for 2.4 GHz. Launch it
from **NRF24 2.4GHz Scan** on the main menu.

- Sweeps all 125 channels (2.401–2.525 GHz)
- Live bar graph with WiFi (ch1/6/11) and BLE advertising channel markers
- Press **OK** to attempt packet capture on the busiest channel
- Captures are automatically decoded and identified as:
  - **BLE Advertising** — PDU type and advertiser MAC address
  - **Logitech Unifying** — device ID, flagged for MouseJack risk
  - **MouseJack** — unencrypted HID packets, flagged as a security risk
  - **Encrypted payload** — statistical heuristic (byte diversity, run-length
    analysis) flags packets that look like ciphertext even when they don't
    match a known unencrypted pattern
  - **Nordic ShockBurst** — generic packet with hex dump if nothing else matches

> Requires a dev board with NRF24 hardware, with the board's switch (if present) in
> NRF24 position. RF Rosetta detects whether the chip is connected — if not, it
> shows a "Not Detected" screen instead of guessing.

---

## WiFi Scanner (ESP32/Marauder)

RF Rosetta can drive an ESP32 flashed with [Marauder](https://github.com/justcallmekoko/ESP32Marauder)
firmware over UART. Launch it from **WiFi Scan (Marauder)** on the main menu.

- Sends `scanap` to start an access point scan, `stopscan` to stop
- Shows a live, scrolling terminal view of everything Marauder sends back
- Best-effort parses AP lines for SSID, BSSID, RSSI, channel, and encryption
  type, and keeps a running AP count
- Press **OK** to start/stop scanning, **Back** to exit and close the UART

> **Field-testing note**: Marauder's serial output format has changed across
> firmware forks and versions, so the structured parser is best-effort. You'll
> always see the raw text either way — the parser just adds structure on top
> when it recognises a line. If AP details aren't populating on your build,
> the raw scroll is still fully functional as a live monitor.

---

## Dev Board Support

RF Rosetta works with the **CC1101 and NRF24 modules on external Flipper dev boards**,
using a proper GPIO preset system so you're not locked to one specific board.

| Module | What it does in RF Rosetta |
|---|---|
| **External CC1101** | Full sub-GHz scanning + signal capture, same as internal — plus better range on high-gain boards |
| **NRF24L01** | 2.4 GHz channel scanner, packet capture, and decode (BLE adv, ShockBurst, MouseJack detection) |
| **ESP32 (Marauder)** | Live WiFi network scanner over UART — see below |

### Board Presets

Settings → **Dev Board** lets you pick your hardware. RF Rosetta ships with known-good
pinouts for:

| Preset | Boards it covers |
|---|---|
| **3-in-1 Board** (default) | Generic CC1101+NRF24+ESP32 combo boards (most common on AliExpress) |
| **Flipper Dev Board** | Official Flipper dev board CC1101 module |
| **WiFi Dev Board** | Flipper WiFi dev board v1/v2 |
| **CC1101 Breadboard** | Bare CC1101 module, common breadboard wiring |
| **NRF24 Standalone** | Bare NRF24L01+ module |
| **Missile RF** | Rabbit-Labs Missile RF board |
| **Custom** | Set your own pin mapping if your board isn't listed |

If your board isn't in the list and the presets don't work, select **Custom** — RF
Rosetta will use the 3-in-1 default pinout as a starting point which you can verify
against your board's actual wiring.

### GPIO Pinout (3-in-1 default)

All radio modules share the same SPI bus; a physical switch on most boards selects
which chip is active. ESP32 is always available via UART regardless of switch position.

| Flipper GPIO | Signal | Used by |
|---|---|---|
| Pin 2 (PA7) | MOSI | CC1101 + NRF24 |
| Pin 3 (PA6) | MISO | CC1101 + NRF24 |
| Pin 4 (PA4) | CSN | CC1101 + NRF24 |
| Pin 5 (PB3) | SCK | CC1101 + NRF24 |
| Pin 6 (PB2) | GDO0 / CE | CC1101 packet indicator / NRF24 RX enable |
| Pin 13 | UART TX | ESP32 RX |
| Pin 14 | UART RX | ESP32 TX |

To use the external CC1101: set **Antenna → External** in Settings, and pick the
matching **Dev Board** preset. RF Rosetta detects the chip automatically by reading
its VERSION register — if it can't find it, it falls back to the Flipper's internal
antenna and shows a status message.

---

## Installation

### Via FAP Catalog (Recommended)
1. Open the Flipper Zero app on your phone
2. Go to **Apps → Sub-GHz**
3. Find **RF Rosetta** and tap Install

### Manual Install
1. Download the latest `.fap` from [Releases](../../releases)
2. Copy to `SD:/apps/Sub-GHz/rf_rosetta.fap`
3. Launch from the Flipper's Apps menu

### Build from Source
```bash
pip install ufbt
git clone https://github.com/joelewis012/rf_rosetta
cd rf_rosetta
ufbt
```

---

## Settings

| Setting | Options | Notes |
|---|---|---|
| Scan Mode | All / OOK / FSK-N / FSK-W | All is the default — covers every signal type |
| Antenna | Internal / External | External uses the CC1101 on your dev board |
| Dev Board | See [board presets](#board-presets) | Which GPIO pinout to use for external CC1101/NRF24 |
| Threshold | -90 to -60 dBm | Lower = more sensitive, more false triggers |
| Dwell Speed | 100ms – 2s | Time spent per frequency during sweep |
| SD Logging | On / Off | Saves all captures to configurable log path |

---

## Protocol Database

The database lives in `protocol_db.c`. Adding a new protocol:

```c
{
    .name          = "My New Protocol",
    .short_name    = "My Proto",
    .category      = CategoryHome,
    .freq_min      = 433050000, .freq_max = 433920000,
    .modulation    = ModulationOOK,
    .pulse_min     = 300, .pulse_max = 900,
    .bandwidth_khz = 40,
    .repeating     = true, .repeat_min = 2, .repeat_max = 4,
    .fixed_code    = true,
    .brands        = "Brand A, Brand B",
    .description   = "What this device does.",
    .security_note = "Security implications.",
    .extra_data    = "What data can be decoded.",
},
```

Pull requests for new protocols are very welcome.

---

## SD Card Files

| File | Contents |
|---|---|
| `SD:/rf_rosetta/log.txt` | Timestamped log of every captured signal |
| `SD:/rf_rosetta/saved.bin` | Saved/bookmarked signal captures |

---

## FAQ

**Will this work on all Flipper Zero hardware?**
Yes — the Sub-GHz scanner uses the built-in CC1101. NRF24 and WiFi scanning
require a dev board with the relevant module.

**Does it decode the actual content of signals?**
Yes, for a growing set of protocols. TPMS shows live pressure and temperature,
weather stations show temperature/humidity/wind, utility meters show manufacturer
and ID (payload itself stays encrypted where the protocol uses AES). For protocols
without a live decoder, RF Rosetta still identifies what it is and what data it
theoretically carries.

**Is this legal to use?**
RF Rosetta is a passive listener — it does not transmit anything. Passive reception
is legal in virtually all jurisdictions. What you do with the information is your responsibility.

**A signal isn't being identified — what should I do?**
Check the full details screen for the raw characteristics, then open a GitHub issue.
Unknown signals help grow the database.

---

## Roadmap

- [x] Live TPMS pressure and temperature decoding
- [x] Smart meter manufacturer/ID extraction
- [x] Weather station temperature/humidity live decode
- [x] Signal fingerprinting and RSSI trend arrows
- [x] External CC1101 full signal capture support
- [x] NRF24 2.4GHz scanner and packet decode
- [x] .sub file export (compatible with Flipper's Sub-GHz player)
- [x] Custom GPIO / multi-board support
- [x] 120+ protocol database
- [x] ESP32/Marauder WiFi network scanner integration
- [x] NRF24 encrypted payload heuristics (beyond MouseJack/ShockBurst)
- [ ] Community protocol submission via GitHub Issues template
- [ ] Structured Marauder AP parser tuned against real hardware output (currently best-effort)

---

## Contributing

Contributions are welcome, especially:
- New protocol signatures in `protocol_db.c`
- Bug reports with captured signal details
- Tuning the Marauder AP parser against real serial output (see [WiFi Scanner](#wifi-scanner-esp32marauder))

See [CONTRIBUTING.md](CONTRIBUTING.md) for the protocol format, build setup,
and a list of SDK quirks that will save you debugging time. This project
follows the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md).

Please open an issue before a large PR so we can discuss approach.

---

## License

GPL-3.0 — see [LICENSE](LICENSE)

---

<div align="center">

**Built for the Flipper Zero community.**

*If RF Rosetta identified something interesting — share it!*

☕ **[Buy me a coffee](https://buymeacoffee.com/Joelewis012)** if this saved you time.

</div>
